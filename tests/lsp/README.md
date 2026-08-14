# LSP protocol tests

Black-box tests for the cksp language server. They spawn `cksp --lsp` as a
subprocess and speak JSON-RPC over stdio — no test framework, no new build
targets, no dependencies beyond the Python 3 that CMake already requires.

```bash
./run_tests.sh --lsp                      # build release + run the suite
python3 tests/lsp/run_lsp_tests.py        # run against an existing binary
python3 tests/lsp/run_lsp_tests.py --filter definition --trace
python3 tests/lsp/run_lsp_tests.py --keep-workspace   # inspect the sources sent
```

Exit code is 0 when everything passed, 1 otherwise.

## Layout

| File | Purpose |
|---|---|
| `lsp_harness.py` | protocol client, fixture materialisation, assertions, runner |
| `run_lsp_tests.py` | the test cases |
| `fixtures/*.cksp.in` | fixture templates with cursor markers |

## Fixtures and markers

Fixtures carry `<|name|>` markers instead of hand-counted positions:

```cksp
declare <|rate_decl|>rate: int := 44100
...
audio.<|rate_use|>rate := 48000
```

`fixture.at("rate_use")` returns the zero-based LSP position of that marker.
The harness strips the markers, computes positions on the clean text and
writes it into a throwaway workspace, so several markers on one line do not
shift each other.

The `.in` suffix matters: it keeps these files out of the language server's
supported extensions, so your editor does not flag them as broken sources.
Use `--keep-workspace` to see the materialised, marker-free files.

## Waiting for the analysis

Analysis is asynchronous with a 120 ms debounce, so a request sent right after
`didOpen` races it. The barrier is `publishDiagnostics`, which
`DiagnosticPublisher` emits for the entry source even when there are no
diagnostics. `did_open()` and `did_change()` handle this with a generation
counter captured before the edit — never add a `sleep`.

## Pending cases

Cases declared with `requires="completionProvider"` report as PENDING while the
server does not advertise that capability. They double as an executable
specification for the completion provider: when it lands, they run as-is.
