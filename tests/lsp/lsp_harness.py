#!/usr/bin/env python3
"""
Protocol harness for the cksp language server.

Drives `cksp --lsp` as a subprocess over stdio JSON-RPC. Provides three things
that a language server test needs and that are easy to get subtly wrong:

  * Framing and dispatch. Notifications arrive interleaved with responses, so a
    reader thread demultiplexes them; requests block on their own id only.

  * A real synchronisation barrier. Analysis runs asynchronously on a worker
    with a 120 ms debounce (LanguageServer.h ANALYSIS_DEBOUNCE), so a request
    sent right after didOpen races the analysis. DiagnosticPublisher always
    publishes the entry source, even with zero diagnostics, which makes
    publishDiagnostics the one reliable "analysis finished" signal. Every
    wait uses a generation counter captured *before* the edit, so a
    notification that arrives while we are still sending cannot be missed.

  * Cursor markers. Fixtures carry `<|name|>` markers instead of hand-counted
    line/column pairs. The harness strips them, computes positions on the
    marker-free text and writes that text into a throwaway workspace.

Fixtures live in fixtures/ and use the .cksp.in extension: they contain
markers and are therefore not valid sources, and the extra suffix keeps the
language server (and your editor) from analysing them. The harness strips the
suffix when materialising. Run with --keep-workspace to inspect the resulting
sources.
"""

from __future__ import annotations

import json
import os
import queue
import re
import shutil
import subprocess
import sys
import tempfile
import threading
import traceback
import urllib.parse
from dataclasses import dataclass, field
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
FIXTURE_ROOT = Path(__file__).resolve().parent / "fixtures"
DEFAULT_BINARIES = (
    REPO_ROOT / "cmake-build-release" / "cksp",
    REPO_ROOT / "cmake-build-debug" / "cksp",
)

GREEN, RED, YELLOW, DIM, RESET = (
    "\033[0;32m",
    "\033[0;31m",
    "\033[0;33m",
    "\033[2m",
    "\033[0m",
)


# --------------------------------------------------------------------------
# URIs and paths
# --------------------------------------------------------------------------

def path_to_uri(path: str | os.PathLike) -> str:
    return "file://" + urllib.parse.quote(str(Path(path).resolve()), safe="/")


def uri_to_path(uri: str) -> str:
    """Decodes a file: URI. The server percent-encodes with lowercase hex and
    resolves symlinks (weakly_canonical), so never compare URI strings; compare
    the realpath of both sides via same_path()."""
    if not uri.startswith("file://"):
        return uri
    return urllib.parse.unquote(uri[len("file://"):])


def same_path(left: str, right: str | os.PathLike) -> bool:
    return os.path.realpath(left) == os.path.realpath(str(right))


# --------------------------------------------------------------------------
# Fixtures and markers
# --------------------------------------------------------------------------

MARKER_PATTERN = re.compile(r"<\|(\w*)\|>")


@dataclass(frozen=True)
class Position:
    line: int  # zero-based, as in LSP
    character: int

    def as_json(self) -> dict:
        return {"line": self.line, "character": self.character}

    def __str__(self) -> str:
        return f"{self.line + 1}:{self.character + 1}"


@dataclass
class Fixture:
    """A materialised source file plus the marker positions found in it."""

    name: str
    path: Path
    text: str
    markers: dict[str, Position]

    @property
    def uri(self) -> str:
        return path_to_uri(self.path)

    def at(self, marker: str = "") -> Position:
        if marker not in self.markers:
            known = ", ".join(sorted(self.markers)) or "<none>"
            raise KeyError(f"fixture {self.name}: no marker '{marker}' (have: {known})")
        return self.markers[marker]

    def line_of(self, position: Position) -> str:
        lines = self.text.splitlines()
        return lines[position.line] if position.line < len(lines) else ""


def parse_markers(raw: str) -> tuple[str, dict[str, Position]]:
    """Strips `<|name|>` markers and returns the clean text plus positions.

    Positions are computed on the clean text, so several markers on one line
    do not shift each other."""
    markers: dict[str, Position] = {}
    out: list[str] = []
    line = character = 0
    index = 0
    for match in MARKER_PATTERN.finditer(raw):
        chunk = raw[index:match.start()]
        out.append(chunk)
        newlines = chunk.count("\n")
        if newlines:
            line += newlines
            character = len(chunk) - chunk.rfind("\n") - 1
        else:
            character += len(chunk)
        name = match.group(1)
        if name in markers:
            raise ValueError(f"duplicate marker '{name}'")
        markers[name] = Position(line, character)
        index = match.end()
    out.append(raw[index:])
    return "".join(out), markers


class Workspace:
    """A throwaway directory holding the marker-free copies of fixtures."""

    def __init__(self, root: Path, server: "LanguageServerClient"):
        self.root = root
        self._server = server
        self._fixtures: dict[str, Fixture] = {}

    def add(self, relative: str, *, as_name: str | None = None) -> Fixture:
        """Materialises fixtures/<relative>.in into the workspace as <relative>,
        without opening it. Tests name the target file ("navigation.cksp"); the
        template on disk carries the extra .in suffix."""
        source = FIXTURE_ROOT / (relative + ".in")
        if not source.is_file():
            raise FileNotFoundError(f"missing fixture: {source}")
        text, markers = parse_markers(source.read_text(encoding="utf-8"))
        target = self.root / (as_name or relative)
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(text, encoding="utf-8")
        fixture = Fixture(relative, target, text, markers)
        self._fixtures[relative] = fixture
        return fixture

    def add_all(self, *relatives: str) -> list[Fixture]:
        return [self.add(relative) for relative in relatives]

    def open(self, relative: str, *, wait: bool = True) -> Fixture:
        """Materialises, opens and (by default) waits for the analysis to land."""
        fixture = self._fixtures.get(relative) or self.add(relative)
        self._server.did_open(fixture, wait=wait)
        return fixture

    def write(self, relative: str, text: str) -> Fixture:
        """Creates a file from inline text (no markers) — for generated cases."""
        target = self.root / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(text, encoding="utf-8")
        fixture = Fixture(relative, target, text, {})
        self._fixtures[relative] = fixture
        return fixture


# --------------------------------------------------------------------------
# Client
# --------------------------------------------------------------------------

class LspError(RuntimeError):
    pass


class LspTimeout(RuntimeError):
    pass


class LanguageServerClient:
    def __init__(self, binary: Path, *, timeout: float = 20.0, trace: bool = False):
        self.binary = Path(binary)
        self.timeout = timeout
        self.trace = trace
        self.capabilities: dict = {}

        self._process = subprocess.Popen(
            [str(self.binary), "--lsp"],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        self._next_id = 0
        self._write_lock = threading.Lock()
        self._state = threading.Condition()
        self._responses: dict[int, dict] = {}
        self._notifications: queue.Queue[dict] = queue.Queue()
        # path -> (diagnostics, generation). The generation counter is what makes
        # waiting race-free: capture it before an edit, wait for a higher one.
        self._diagnostics: dict[str, list] = {}
        self._generations: dict[str, int] = {}
        self._stderr: list[str] = []
        self._closed = False

        self._reader = threading.Thread(target=self._read_loop, daemon=True)
        self._reader.start()
        self._stderr_reader = threading.Thread(target=self._drain_stderr, daemon=True)
        self._stderr_reader.start()

    # -- transport ---------------------------------------------------------

    def _drain_stderr(self) -> None:
        # Never let the server block on a full stderr pipe, and keep the output
        # around: a crash report here is usually the real explanation for a
        # timeout further up.
        for raw in iter(self._process.stderr.readline, b""):
            self._stderr.append(raw.decode("utf-8", "replace").rstrip("\n"))

    def _read_loop(self) -> None:
        stream = self._process.stdout
        while True:
            message = self._read_message(stream)
            if message is None:
                with self._state:
                    self._closed = True
                    self._state.notify_all()
                return
            self._dispatch(message)

    def _read_message(self, stream) -> dict | None:
        headers: dict[str, str] = {}
        while True:
            raw = stream.readline()
            if not raw:
                return None
            line = raw.decode("ascii", "replace")
            if line in ("\r\n", "\n"):
                break
            if ":" in line:
                key, value = line.split(":", 1)
                headers[key.strip().lower()] = value.strip()
        length = int(headers.get("content-length", 0))
        body = b""
        while len(body) < length:
            chunk = stream.read(length - len(body))
            if not chunk:
                return None
            body += chunk
        if self.trace:
            print(f"{DIM}<< {body.decode('utf-8', 'replace')}{RESET}", file=sys.stderr)
        return json.loads(body.decode("utf-8"))

    def _dispatch(self, message: dict) -> None:
        if "id" in message and ("result" in message or "error" in message):
            with self._state:
                self._responses[message["id"]] = message
                self._state.notify_all()
            return

        method = message.get("method")
        if method and "id" in message:
            # The server does not currently send requests. Answer anyway so a
            # future one cannot deadlock the test run.
            self._send({
                "jsonrpc": "2.0",
                "id": message["id"],
                "error": {"code": -32601, "message": "not implemented in harness"},
            })
            return

        if method == "textDocument/publishDiagnostics":
            params = message.get("params") or {}
            path = os.path.realpath(uri_to_path(params.get("uri", "")))
            with self._state:
                self._diagnostics[path] = params.get("diagnostics", [])
                self._generations[path] = self._generations.get(path, 0) + 1
                self._state.notify_all()
        self._notifications.put(message)

    def _send(self, payload: dict) -> None:
        body = json.dumps(payload).encode("utf-8")
        if self.trace:
            print(f"{DIM}>> {body.decode('utf-8')}{RESET}", file=sys.stderr)
        with self._write_lock:
            if self._process.poll() is not None:
                raise LspError("server process has exited: " + self.stderr_tail())
            self._process.stdin.write(
                f"Content-Length: {len(body)}\r\n\r\n".encode("ascii") + body
            )
            self._process.stdin.flush()

    def request(self, method: str, params: dict | None = None, *, timeout: float | None = None):
        self._next_id += 1
        request_id = self._next_id
        self._send({"jsonrpc": "2.0", "id": request_id, "method": method, "params": params or {}})

        deadline = timeout if timeout is not None else self.timeout
        with self._state:
            ok = self._state.wait_for(
                lambda: request_id in self._responses or self._closed, deadline
            )
            if not ok or request_id not in self._responses:
                raise LspTimeout(
                    f"no response to {method} within {deadline}s. {self.stderr_tail()}"
                )
            message = self._responses.pop(request_id)
        if "error" in message:
            raise LspError(f"{method} failed: {message['error']}")
        return message.get("result")

    def notify(self, method: str, params: dict | None = None) -> None:
        self._send({"jsonrpc": "2.0", "method": method, "params": params or {}})

    def stderr_tail(self, lines: int = 12) -> str:
        if not self._stderr:
            return ""
        return "server stderr:\n  " + "\n  ".join(self._stderr[-lines:])

    # -- lifecycle ---------------------------------------------------------

    def initialize(self, root: Path, *, entry_points: list[str] | None = None) -> dict:
        options: dict = {}
        if entry_points:
            options["entryPoints"] = entry_points
        result = self.request("initialize", {
            "processId": os.getpid(),
            "rootUri": path_to_uri(root),
            "capabilities": {
                "textDocument": {
                    "completion": {"completionItem": {"snippetSupport": False}},
                    "definition": {"linkSupport": True},
                }
            },
            "initializationOptions": options,
        })
        self.capabilities = (result or {}).get("capabilities", {})
        self.notify("initialized", {})
        return result

    def supports(self, capability: str) -> bool:
        return bool(self.capabilities.get(capability))

    def shutdown(self) -> None:
        try:
            self.request("shutdown", timeout=5.0)
            self.notify("exit")
        except (LspError, LspTimeout, OSError, ValueError):
            pass
        try:
            self._process.wait(timeout=5.0)
        except subprocess.TimeoutExpired:
            self._process.kill()
            self._process.wait(timeout=5.0)
        for stream in (self._process.stdin, self._process.stdout, self._process.stderr):
            try:
                stream.close()
            except OSError:
                pass

    # -- documents and the analysis barrier --------------------------------

    def diagnostics_generation(self, target: Fixture | str) -> int:
        path = os.path.realpath(str(target.path if isinstance(target, Fixture) else target))
        with self._state:
            return self._generations.get(path, 0)

    def wait_for_analysis(self, target: Fixture | str, *, after: int = 0,
                          timeout: float | None = None) -> list:
        """Blocks until diagnostics for `target` are published past generation
        `after`. This is the only sound way to sequence a request behind an
        edit — the analysis worker debounces for 120 ms and then runs a full
        frontend pass.

        Always pass `after=diagnostics_generation(target)` captured *before*
        sending the edit. Calling this without `after` on a document that has
        already been analysed returns immediately and silently races the
        analysis you meant to wait for; did_open/did_change do it correctly."""
        path = os.path.realpath(str(target.path if isinstance(target, Fixture) else target))
        deadline = timeout if timeout is not None else self.timeout
        with self._state:
            ok = self._state.wait_for(
                lambda: self._generations.get(path, 0) > after or self._closed, deadline
            )
            if not ok or self._generations.get(path, 0) <= after:
                raise LspTimeout(
                    f"no diagnostics for {Path(path).name} within {deadline}s "
                    f"(generation still {self._generations.get(path, 0)}). {self.stderr_tail()}"
                )
            return list(self._diagnostics.get(path, []))

    def diagnostics(self, target: Fixture | str) -> list:
        path = os.path.realpath(str(target.path if isinstance(target, Fixture) else target))
        with self._state:
            return list(self._diagnostics.get(path, []))

    def did_open(self, fixture: Fixture, *, wait: bool = True) -> None:
        baseline = self.diagnostics_generation(fixture)
        self.notify("textDocument/didOpen", {
            "textDocument": {
                "uri": fixture.uri,
                "languageId": "cksp",
                "version": 1,
                "text": fixture.text,
            }
        })
        if wait:
            self.wait_for_analysis(fixture, after=baseline)

    def did_change(self, fixture: Fixture, text: str, *, version: int = 2,
                   wait: bool = True) -> Fixture:
        """Replaces the whole document (the server negotiates full sync) and
        returns an updated fixture. Markers are re-parsed from `text` if it
        carries any, so a typing simulation can move its cursor along."""
        clean, markers = parse_markers(text)
        baseline = self.diagnostics_generation(fixture)
        self.notify("textDocument/didChange", {
            "textDocument": {"uri": fixture.uri, "version": version},
            "contentChanges": [{"text": clean}],
        })
        updated = Fixture(fixture.name, fixture.path, clean, markers or fixture.markers)
        if wait:
            self.wait_for_analysis(fixture, after=baseline)
        return updated

    def did_close(self, fixture: Fixture) -> None:
        self.notify("textDocument/didClose", {"textDocument": {"uri": fixture.uri}})

    # -- language features -------------------------------------------------

    def _position_params(self, fixture: Fixture, position: Position) -> dict:
        return {
            "textDocument": {"uri": fixture.uri},
            "position": position.as_json(),
        }

    def definition(self, fixture: Fixture, marker: str = "") -> list:
        return self.request(
            "textDocument/definition", self._position_params(fixture, fixture.at(marker))
        ) or []

    def references(self, fixture: Fixture, marker: str = "", *,
                   include_declaration: bool = True) -> list:
        params = self._position_params(fixture, fixture.at(marker))
        params["context"] = {"includeDeclaration": include_declaration}
        return self.request("textDocument/references", params) or []

    def document_highlight(self, fixture: Fixture, marker: str = "") -> list:
        return self.request(
            "textDocument/documentHighlight", self._position_params(fixture, fixture.at(marker))
        ) or []

    def prepare_rename(self, fixture: Fixture, marker: str = ""):
        return self.request(
            "textDocument/prepareRename", self._position_params(fixture, fixture.at(marker))
        )

    def rename(self, fixture: Fixture, marker: str, new_name: str):
        params = self._position_params(fixture, fixture.at(marker))
        params["newName"] = new_name
        return self.request("textDocument/rename", params)

    def document_links(self, fixture: Fixture) -> list:
        return self.request(
            "textDocument/documentLink", {"textDocument": {"uri": fixture.uri}}
        ) or []

    def code_actions(self, fixture: Fixture, diagnostics: list | None = None) -> list:
        """Quick fixes offered for `diagnostics`, defaulting to everything the
        last analysis published for the fixture. The server derives actions from
        the diagnostics the client sends back, so the request carries them."""
        if diagnostics is None:
            diagnostics = self.diagnostics(fixture)
        if not diagnostics:
            return []
        span = {"start": diagnostics[0]["range"]["start"],
                "end": diagnostics[-1]["range"]["end"]}
        return self.request("textDocument/codeAction", {
            "textDocument": {"uri": fixture.uri},
            "range": span,
            "context": {"diagnostics": diagnostics, "only": ["quickfix"]},
        }) or []

    def completion(self, fixture: Fixture, marker: str = "", *,
                   trigger_character: str | None = ".") -> list:
        params = self._position_params(fixture, fixture.at(marker))
        params["context"] = (
            {"triggerKind": 2, "triggerCharacter": trigger_character}
            if trigger_character else {"triggerKind": 1}
        )
        result = self.request("textDocument/completion", params)
        if result is None:
            return []
        # Either CompletionItem[] or CompletionList.
        return result["items"] if isinstance(result, dict) else result

    def completion_at(self, fixture: Fixture, position: Position, *,
                      trigger_character: str | None = ".") -> list:
        params = {
            "textDocument": {"uri": fixture.uri},
            "position": position.as_json(),
            "context": (
                {"triggerKind": 2, "triggerCharacter": trigger_character}
                if trigger_character else {"triggerKind": 1}
            ),
        }
        result = self.request("textDocument/completion", params)
        if result is None:
            return []
        return result["items"] if isinstance(result, dict) else result


# --------------------------------------------------------------------------
# Assertions
# --------------------------------------------------------------------------

def expect(condition, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def position_of(node: dict, key: str = "range") -> Position:
    start = node[key]["start"]
    return Position(start["line"], start["character"])


def expect_position(actual: Position, fixture: Fixture, marker: str, *, what: str = "position") -> None:
    wanted = fixture.at(marker)
    expect(
        actual == wanted,
        f"{what}: expected {wanted} (marker '{marker}' in {fixture.name}) but got {actual}\n"
        f"    line at expectation: {fixture.line_of(wanted)!r}",
    )


def expect_definition(links: list, fixture: Fixture, marker: str, *,
                      in_file: Fixture | None = None) -> dict:
    """Checks that go-to-definition landed on `marker`. The declaration's name
    range is targetSelectionRange; targetRange may span a whole function
    header, so it is deliberately not asserted here."""
    expect(len(links) == 1, f"expected exactly one definition link, got {len(links)}: {links}")
    link = links[0]
    target_file = in_file or fixture
    expect(
        same_path(uri_to_path(link["targetUri"]), target_file.path),
        f"definition points at {uri_to_path(link['targetUri'])}, expected {target_file.path}",
    )
    expect_position(
        position_of(link, "targetSelectionRange"), target_file, marker, what="definition target"
    )
    return link


def labels_of(items: list) -> list[str]:
    return [item.get("label", "") for item in items]


def item_named(items: list, label: str) -> dict:
    for item in items:
        if item.get("label") == label:
            return item
    raise AssertionError(f"no completion item labelled {label!r}; got {sorted(labels_of(items))}")


def expect_labels(items: list, expected: list[str], *, exactly: bool = False) -> None:
    found = labels_of(items)
    missing = [label for label in expected if label not in found]
    expect(not missing, f"missing completion labels {missing}; got {sorted(found)}")
    if exactly:
        extra = [label for label in found if label not in expected]
        expect(not extra, f"unexpected completion labels {sorted(extra)}")


def expect_no_labels(items: list, forbidden: list[str]) -> None:
    found = labels_of(items)
    present = [label for label in forbidden if label in found]
    expect(not present, f"labels {present} must not be offered; got {sorted(found)}")


def messages_of(diagnostics: list) -> list[str]:
    return [d.get("message", "") for d in diagnostics]


def titles_of(actions: list) -> list[str]:
    return [action.get("title", "") for action in actions]


def action_titled(actions: list, needle: str) -> dict:
    for action in actions:
        if needle in action.get("title", ""):
            return action
    raise AssertionError(f"no quick fix whose title contains {needle!r}; "
                         f"got {titles_of(actions)}")


def apply_action(text: str, action: dict, fixture: Fixture) -> str:
    """Applies a quick fix's edits to `text` the way an editor would. Edits are
    applied last-to-first so the earlier ones keep the offsets they were
    computed against."""
    edits = action["edit"]["changes"][fixture.uri]
    lines = text.split("\n")
    for edit in sorted(edits,
                       key=lambda e: (e["range"]["start"]["line"],
                                      e["range"]["start"]["character"]),
                       reverse=True):
        start, end = edit["range"]["start"], edit["range"]["end"]
        expect(start["line"] == end["line"],
               f"quick fix edits are single-line by construction; got {edit}")
        line = lines[start["line"]]
        lines[start["line"]] = (line[:start["character"]] + edit["newText"]
                                + line[end["character"]:])
    return "\n".join(lines)


def port_with_quick_fixes(fixture: Fixture, server: "LanguageServerClient", text: str,
                          *, limit: int = 12) -> tuple[str, list[str]]:
    """Applies quick fixes until none is left, the way a user clicking the
    lightbulb would. Returns the ported text and the titles applied in order."""
    applied: list[str] = []
    for round_index in range(limit):
        diagnostics = [d for d in server.diagnostics(fixture)
                       if (d.get("data") or {}).get("edits")]
        if not diagnostics:
            return text, applied
        actions = server.code_actions(fixture, diagnostics[:1])
        expect(actions, f"diagnostic carries a fix but no action was offered: {diagnostics[0]}")
        applied.append(actions[0]["title"])
        text = apply_action(text, actions[0], fixture)
        server.did_change(fixture, text, version=round_index + 2)
    raise AssertionError(f"quick fixes did not converge within {limit} rounds; "
                         f"applied {applied}")


# --------------------------------------------------------------------------
# Test registry and runner
# --------------------------------------------------------------------------

@dataclass
class TestCase:
    name: str
    function: object
    requires: str | None = None
    entry_points: list[str] = field(default_factory=list)


REGISTRY: list[TestCase] = []


def test(name: str, *, requires: str | None = None, entry_points: list[str] | None = None):
    """Registers a case. `requires` names a server capability; when the server
    does not advertise it the case is reported as PENDING instead of failing —
    that is how the completion cases stay in the suite before the provider
    exists."""
    def decorator(function):
        REGISTRY.append(TestCase(name, function, requires, entry_points or []))
        return function
    return decorator


def resolve_binary(explicit: str | None) -> Path:
    if explicit:
        path = Path(explicit)
        if not path.is_file():
            raise SystemExit(f"binary not found: {path}")
        return path
    for candidate in DEFAULT_BINARIES:
        if candidate.is_file():
            return candidate
    raise SystemExit(
        "no cksp binary found. Build one first:\n"
        "  cmake --build cmake-build-release --target cksp -j 8"
    )


def run_suite(argv: list[str] | None = None) -> int:
    import argparse

    parser = argparse.ArgumentParser(description="Run the cksp LSP protocol tests.")
    parser.add_argument("--binary", help="path to the cksp binary")
    parser.add_argument("--filter", default="", help="substring filter on test names")
    parser.add_argument("--timeout", type=float, default=20.0, help="per-request timeout")
    parser.add_argument("--trace", action="store_true", help="dump JSON-RPC traffic")
    parser.add_argument("--keep-workspace", action="store_true",
                        help="keep the materialised workspaces and print their paths")
    args = parser.parse_args(argv)

    binary = resolve_binary(args.binary)
    selected = [case for case in REGISTRY if args.filter in case.name]
    if not selected:
        print(f"no tests match {args.filter!r}")
        return 1

    print(f"🔌 LSP protocol tests — {binary}")
    print("=" * 78)

    passed = failed = pending = 0
    failures: list[tuple[str, str]] = []

    for case in selected:
        workspace_dir = Path(tempfile.mkdtemp(prefix="cksp-lsp-"))
        client = None
        try:
            client = LanguageServerClient(binary, timeout=args.timeout, trace=args.trace)
            entries = [str(workspace_dir / entry) for entry in case.entry_points]
            client.initialize(workspace_dir, entry_points=entries)

            if case.requires and not client.supports(case.requires):
                print(f"⏭️  {YELLOW}PENDING{RESET} {case.name} "
                      f"{DIM}(server does not advertise {case.requires}){RESET}")
                pending += 1
                continue

            case.function(Workspace(workspace_dir, client), client)
            print(f"✅ {GREEN}PASS{RESET}    {case.name}")
            passed += 1
        except AssertionError as error:
            print(f"❌ {RED}FAIL{RESET}    {case.name}")
            failures.append((case.name, str(error)))
            failed += 1
        except Exception:  # noqa: BLE001 - a harness error must not hide the others
            print(f"💥 {RED}ERROR{RESET}   {case.name}")
            failures.append((case.name, traceback.format_exc()))
            failed += 1
        finally:
            if client is not None:
                client.shutdown()
            if args.keep_workspace:
                print(f"   {DIM}workspace: {workspace_dir}{RESET}")
            else:
                shutil.rmtree(workspace_dir, ignore_errors=True)

    if failures:
        print("-" * 78)
        for name, detail in failures:
            print(f"\n{RED}{name}{RESET}")
            for line in detail.rstrip().splitlines():
                print(f"    {line}")

    print("-" * 78)
    summary = f"✅ {passed}   ❌ {failed}"
    if pending:
        summary += f"   ⏭️  {pending} pending"
    print(f"📦 Summary: {summary}")
    return 1 if failed else 0
