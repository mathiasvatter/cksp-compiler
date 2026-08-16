#!/usr/bin/env python3
"""
Test cases for the cksp language server.

Two groups:

  * Navigation and diagnostics — features that exist today. They are what
    validates the harness itself: a new harness asserting against new code
    proves nothing.

  * Completion — declared with requires="completionProvider". Until the server
    advertises that capability they report as PENDING; the day the provider
    lands they turn into real tests with no edit here.

Run:  python3 tests/lsp/run_lsp_tests.py [--filter substring] [--trace]
"""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from lsp_harness import (  # noqa: E402
    Position,
    action_titled,
    apply_action,
    expect,
    expect_definition,
    expect_labels,
    expect_no_labels,
    expect_position,
    item_named,
    labels_of,
    messages_of,
    port_with_quick_fixes,
    position_of,
    run_suite,
    same_path,
    test,
    uri_to_path,
)


# ==========================================================================
# Navigation — exercises the existing ReferenceIndex end to end
# ==========================================================================

@test("definition: const member jumps to its declaration")
def _(workspace, server):
    fixture = workspace.open("navigation.cksp")
    expect_definition(server.definition(fixture, "const_use"), fixture, "const_decl")


@test("definition: namespace member jumps to its declaration")
def _(workspace, server):
    fixture = workspace.open("navigation.cksp")
    expect_definition(server.definition(fixture, "rate_use"), fixture, "rate_decl")


@test("definition: the qualifier segment jumps to the namespace block")
def _(workspace, server):
    # Clicking `audio` in `audio.rate` must land on the namespace, not on the
    # member — this is what add_qualifier_links() builds.
    fixture = workspace.open("navigation.cksp")
    expect_definition(server.definition(fixture, "ns_use"), fixture, "ns_decl")


@test("definition: a name a macro parameter assembled navigates on both halves")
def _(workspace, server):
    # <#thing#.value> is one word: the parameter half is the call site's argument, the
    # rest is macro-body text. The substituted name matches no file's text, so the
    # reference index has to fall back on the word as it was written.
    fixture = workspace.open("navigation_macro_params.cksp")
    expect(server.diagnostics(fixture) == [], "precondition: the fixture compiles cleanly")

    # The parameter half has two definitions and offers both: the macro parameter it
    # names, and the declaration of what the call site passed for it.
    targets = {
        position_of(link, "targetSelectionRange")
        for link in server.definition(fixture, "param_half")
    }
    expect(
        targets == {fixture.at("macro_param"), fixture.at("inst_decl")},
        f"expected the macro parameter and the argument's declaration, got {targets}",
    )
    expect_definition(server.definition(fixture, "member_half"), fixture, "value_decl")
    expect_definition(server.definition(fixture, "method_half"), fixture, "bump_decl")
    # The control case, which never went through a substitution.
    expect_definition(server.definition(fixture, "plain_member"), fixture, "value_decl")
    # The argument keeps its own link at the call site.
    expect_definition(server.definition(fixture, "argument"), fixture, "inst_decl")


@test("rename: leaves a usage that a macro parameter spells alone")
def _(workspace, server):
    # The macro body reaches `inst` through <#thing#>. That usage is listed and navigable,
    # but the text there names the macro parameter - rewriting it would break the macro,
    # and refusing over it would make the symbol unrenameable.
    fixture = workspace.open("navigation_macro_params.cksp")
    edit = server.rename(fixture, "inst_decl", "renamed")
    changes = (edit or {}).get("changes", {})
    expect(changes, f"rename produced no changes: {edit}")
    edited = sorted(
        (position_of(e).line, position_of(e).character) for e in next(iter(changes.values()))
    )
    # The declaration, the call-site argument, and the body line that spells `inst` outright.
    # Not <#thing#>, which spells the parameter.
    wanted = sorted(
        (fixture.at(marker).line, fixture.at(marker).character)
        for marker in ("inst_decl", "argument", "plain_inst")
    )
    expect(edited == wanted, f"expected edits at {wanted}, got {edited}")


@test("references: a macro body usage is listed even though it spells the parameter")
def _(workspace, server):
    fixture = workspace.open("navigation_macro_params.cksp")
    found = server.references(fixture, "inst_decl", include_declaration=True)
    lines = {location["range"]["start"]["line"] for location in found}
    expect(
        fixture.at("param_half").line in lines,
        f"the <#thing#> usage in the macro body must be listed; got lines {sorted(lines)}",
    )


@test("definition: function call jumps to its definition header")
def _(workspace, server):
    fixture = workspace.open("navigation.cksp")
    links = server.definition(fixture, "reset_use")
    expect(len(links) == 1, f"expected one link, got {links}")
    # Function definitions expose the whole header as targetRange while
    # targetSelectionRange stays on the name.
    expect_position(
        position_of(links[0], "targetSelectionRange"), fixture, "reset_decl",
        what="function definition target",
    )
    header = links[0]["targetRange"]
    expect(
        header["start"]["line"] == fixture.at("reset_decl").line,
        f"targetRange should start on the header line, got {header}",
    )


@test("definition: resolves across an import boundary")
def _(workspace, server):
    shared = workspace.add("imports/shared.cksp")
    main = workspace.open("imports/main.cksp")
    expect_definition(
        server.definition(main, "const_use"), main, "const_decl", in_file=shared
    )


@test("definition: an error raised during struct lowering still resolves a member type")
def _(workspace, server):
    # The analysis aborts inside the lowering pass, with the leading structs already
    # replaced by their member blocks. The salvage harvest then walks that AST, and the
    # struct lookup it resolves member types through still hands out those structs.
    fixture = workspace.open("struct_lowering_abort.cksp")
    expect(server.diagnostics(fixture), "precondition: the static access aborts the analysis")
    expect_definition(server.definition(fixture, "note_use"), fixture, "note_decl")


@test("definition: function imported from another file")
def _(workspace, server):
    shared = workspace.add("imports/shared.cksp")
    main = workspace.open("imports/main.cksp")
    links = server.definition(main, "call_use")
    expect(len(links) == 1, f"expected one link, got {links}")
    expect(
        same_path(uri_to_path(links[0]["targetUri"]), shared.path),
        f"expected target in shared.cksp, got {uri_to_path(links[0]['targetUri'])}",
    )


@test("documentLink: the import path points at the imported file")
def _(workspace, server):
    shared = workspace.add("imports/shared.cksp")
    main = workspace.open("imports/main.cksp")
    links = server.document_links(main)
    expect(links, "expected at least one document link for the import statement")
    targets = [uri_to_path(link["target"]) for link in links]
    expect(
        any(same_path(target, shared.path) for target in targets),
        f"no link to shared.cksp; got {targets}",
    )


@test("references: finds every use of a namespace member")
def _(workspace, server):
    fixture = workspace.open("navigation.cksp")
    found = server.references(fixture, "rate_decl", include_declaration=True)
    # rate: declaration, the assignment in on init, twice in the following line
    # and once inside reset().
    expect(len(found) >= 4, f"expected at least 4 locations for rate, got {len(found)}: {found}")
    expect(
        all(same_path(uri_to_path(item["uri"]), fixture.path) for item in found),
        "all references live in the fixture file",
    )


@test("references: excludes the declaration when not requested")
def _(workspace, server):
    fixture = workspace.open("navigation.cksp")
    with_declaration = server.references(fixture, "rate_decl", include_declaration=True)
    without = server.references(fixture, "rate_decl", include_declaration=False)
    expect(
        len(without) < len(with_declaration),
        f"includeDeclaration=false must return fewer locations "
        f"({len(without)} vs {len(with_declaration)})",
    )


@test("documentHighlight: highlights only same-file occurrences")
def _(workspace, server):
    fixture = workspace.open("navigation.cksp")
    highlights = server.document_highlight(fixture, "rate_use")
    expect(highlights, "expected highlights for a resolved symbol")
    for highlight in highlights:
        expect("range" in highlight, f"malformed highlight: {highlight}")


@test("prepareRename: accepts a declared symbol")
def _(workspace, server):
    fixture = workspace.open("navigation.cksp")
    result = server.prepare_rename(fixture, "rate_use")
    expect(result is not None, "prepareRename must not refuse a declared symbol")
    start = Position(result["start"]["line"], result["start"]["character"])
    expect_position(start, fixture, "rate_use", what="prepareRename range")


@test("prepareRename: refuses a position without a symbol")
def _(workspace, server):
    fixture = workspace.open("navigation.cksp")
    # Column 0 of the `end on` line: whitespace/keyword, never renameable.
    blank = Position(len(fixture.text.splitlines()) - 1, 0)
    result = server.request("textDocument/prepareRename", {
        "textDocument": {"uri": fixture.uri},
        "position": blank.as_json(),
    })
    expect(result is None, f"expected null for a non-symbol position, got {result}")


@test("rename: rewrites the declaration and every reference")
def _(workspace, server):
    fixture = workspace.open("navigation.cksp")
    edit = server.rename(fixture, "rate_use", "sample_rate")
    changes = (edit or {}).get("changes", {})
    expect(changes, f"rename produced no changes: {edit}")
    edits = next(iter(changes.values()))
    expect(len(edits) >= 4, f"expected at least 4 edits, got {len(edits)}")
    expect(
        all(item["newText"] == "sample_rate" for item in edits),
        "every edit must carry the new name",
    )


# ==========================================================================
# Diagnostics and document lifecycle
# ==========================================================================

@test("diagnostics: an undeclared variable is reported at its position")
def _(workspace, server):
    fixture = workspace.open("diagnostics.cksp")
    diagnostics = server.diagnostics(fixture)
    expect(diagnostics, "expected a diagnostic for the undeclared variable")
    expect(
        any("undeclared_thing" in message for message in messages_of(diagnostics)),
        f"diagnostic should name the variable; got {messages_of(diagnostics)}",
    )
    start = position_of(diagnostics[0])
    expect(
        start.line == fixture.at("undeclared").line,
        f"diagnostic on line {start.line}, expected {fixture.at('undeclared').line}",
    )


@test("diagnostics: an error in a macro body points back at the body line")
def _(workspace, server):
    # The token was assembled by a <#param#> substitution, so it is reported at the call
    # site. Without the related location the caret sits on a macro call that looks fine.
    fixture = workspace.open("diagnostics_macro_body.cksp")
    diagnostics = server.diagnostics(fixture)
    expect(diagnostics, "expected a diagnostic for the missing member")

    diagnostic = diagnostics[0]
    expect(
        position_of(diagnostic).line == fixture.at("call_site").line,
        f"the diagnostic itself stays at the call site; got line {position_of(diagnostic).line}",
    )
    related = diagnostic.get("relatedInformation")
    expect(related, f"expected relatedInformation on {diagnostic}")
    location = related[0]["location"]
    expect(
        same_path(uri_to_path(location["uri"]), fixture.path),
        f"related location should be in the fixture; got {location['uri']}",
    )
    expect_position(
        position_of(location), fixture, "in_body", what="expansion location",
    )
    expect(
        "no_such_member" in related[0]["message"],
        f"the related message should name the word; got {related[0]['message']!r}",
    )


@test("diagnostics: a declaration named after a define is reported on the declaration")
def _(workspace, server):
    fixture = workspace.open("diagnostics_define_name.cksp")
    diagnostics = server.diagnostics(fixture)
    expect(diagnostics, "expected a diagnostic for the borrowed name")

    diagnostic = diagnostics[0]
    expect_position(
        position_of(diagnostic), fixture, "borrowed_name", what="diagnostic position",
    )
    expect(
        "some_define" in diagnostic["message"],
        f"the message must name what the source spells; got {diagnostic['message']!r}",
    )


@test("diagnostics: a clean file publishes an empty list")
def _(workspace, server):
    fixture = workspace.open("navigation.cksp")
    expect(
        server.diagnostics(fixture) == [],
        f"expected no diagnostics, got {messages_of(server.diagnostics(fixture))}",
    )


@test("diagnostics: fixing an error clears it on the next analysis")
def _(workspace, server):
    fixture = workspace.open("diagnostics.cksp")
    expect(server.diagnostics(fixture), "precondition: the file starts out broken")
    fixed = server.did_change(fixture, "on init\n    message(42)\nend on\n")
    expect(
        server.diagnostics(fixed) == [],
        f"diagnostics should be cleared, got {messages_of(server.diagnostics(fixed))}",
    )


@test("lifecycle: an unsaved edit is analysed from the buffer, not from disk")
def _(workspace, server):
    fixture = workspace.open("navigation.cksp")
    expect(server.diagnostics(fixture) == [], "precondition: fixture is clean")
    broken = server.did_change(
        fixture, fixture.text.replace("message(mode)", "message(never_declared)")
    )
    expect(
        any("never_declared" in message for message in messages_of(server.diagnostics(broken))),
        "the in-memory buffer must win over the file on disk",
    )


# ==========================================================================
# Completion — PENDING until the server advertises completionProvider
# ==========================================================================

@test("completion: capability is advertised with '.' as trigger character",
      requires="completionProvider")
def _(workspace, server):
    options = server.capabilities.get("completionProvider")
    expect(isinstance(options, dict), f"completionProvider should be an object, got {options}")
    triggers = options.get("triggerCharacters", [])
    expect("." in triggers, f"'.' must be a trigger character, got {triggers}")


@test("completion: const block members", requires="completionProvider")
def _(workspace, server):
    fixture = workspace.open("completion_valid.cksp")
    items = server.completion(fixture, "const_member")
    expect_labels(items, ["LOW", "MID", "HIGH"], exactly=True)


@test("completion: family members", requires="completionProvider")
def _(workspace, server):
    fixture = workspace.open("completion_valid.cksp")
    items = server.completion(fixture, "family_member")
    expect_labels(items, ["index", "gain"], exactly=True)


@test("completion: namespace members include nested namespaces and functions",
      requires="completionProvider")
def _(workspace, server):
    fixture = workspace.open("completion_valid.cksp")
    items = server.completion(fixture, "namespace_member")
    expect_labels(items, ["rate", "channels", "mixer", "reset"])
    expect_no_labels(items, ["volume", "mute"])  # those need the mixer. qualifier


@test("completion: nested namespace members", requires="completionProvider")
def _(workspace, server):
    fixture = workspace.open("completion_valid.cksp")
    items = server.completion(fixture, "nested_member")
    expect_labels(items, ["volume", "mute"], exactly=True)


@test("completion: struct offers statics but never instance members",
      requires="completionProvider")
def _(workspace, server):
    fixture = workspace.open("completion_valid.cksp")
    items = server.completion(fixture, "struct_static")
    expect_labels(items, ["MAX_STAGES", "DEFAULT_ATTACK", "describe"])
    # The negative half is the actual claim: Foo. is not an instance.
    expect_no_labels(items, ["attack", "release", "retrigger", "self"])


@test("completion: signatures carry the source parameter names, not the renamed ones",
      requires="completionProvider")
def _(workspace, server):
    # UniqueParameterNamesProvider uniquifies parameter names right before the index is
    # harvested, which used to surface <amount0>. The signature must come from the
    # declaration tokens instead.
    fixture = workspace.open("completion_valid.cksp")
    fade = item_named(server.completion(fixture, "namespace_member"), "fade")
    expect(
        fade["labelDetails"]["detail"] == "(amount: int, target: int)",
        f"unexpected parameter list: {fade.get('labelDetails')}",
    )
    expect(
        fade["detail"] == "function fade(amount: int, target: int): int",
        f"unexpected signature: {fade.get('detail')}",
    )

    describe = item_named(server.completion(fixture, "struct_static"), "describe")
    expect(
        describe["detail"] == "static function describe(label: string, count: int): int",
        f"unexpected static method signature: {describe.get('detail')}",
    )

    # An array parameter is the case that regressed: NodeArray::get_token_string
    # rendered `name`, so the uniquified name came back as <values0[]>.
    scan = item_named(server.completion(fixture, "namespace_member"), "scan")
    expect(
        scan["labelDetails"]["detail"] == "(values: any[], depth: int)",
        f"array parameter name is not the source one: {scan.get('labelDetails')}",
    )


@test("completion: function locals are not members of the enclosing namespace",
      requires="completionProvider")
def _(workspace, server):
    # The namespace desugaring prefixes function-local declarations exactly like real
    # members (<audio.hidden_local>), so they are indistinguishable in the index
    # unless the harvest skips function bodies.
    fixture = workspace.open("completion_valid.cksp")
    items = server.completion(fixture, "namespace_member")
    expect_no_labels(items, ["hidden_local", "values", "depth", "amount", "target"])
    expect_labels(items, ["rate", "channels", "mixer", "reset", "fade", "scan"])


@test("completion: items are shaped like the cksp-tools ones",
      requires="completionProvider")
def _(workspace, server):
    fixture = workspace.open("completion_valid.cksp")
    items = server.completion(fixture, "namespace_member")

    # labelDetails.description is the construct word shown greyed after the label.
    categories = {item["label"]: item["labelDetails"].get("description") for item in items}
    expect(categories.get("fade") == "function", f"got {categories}")
    expect(categories.get("rate") == "variable", f"got {categories}")
    expect(categories.get("mixer") == "namespace", f"got {categories}")

    # A callable documents its signature as a cksp code block; a variable does not,
    # its bare type is already in the detail slot.
    fade = item_named(items, "fade")
    documentation = fade["documentation"]
    expect(documentation["kind"] == "markdown", f"got {documentation}")
    expect(
        documentation["value"] == "```cksp\nfunction fade(amount: int, target: int): int\n```",
        f"unexpected documentation: {documentation['value']!r}",
    )
    rate = item_named(items, "rate")
    expect(rate["detail"] == "int", f"variable detail should be the type, got {rate.get('detail')}")
    expect("documentation" not in rate, "a plain variable needs no documentation block")

    const_item = item_named(server.completion(fixture, "const_member"), "HIGH")
    expect(
        const_item["labelDetails"].get("description") == "const",
        f"got {const_item.get('labelDetails')}",
    )


@test("completion: statics of a struct declared inside a namespace",
      requires="completionProvider")
def _(workspace, server):
    fixture = workspace.open("completion_namespaced_struct.cksp")
    # The struct itself is a member of its namespace.
    expect_labels(server.completion(fixture, "namespace_member"), ["rate", "Envelope"])
    for marker in ("qualified", "shortened"):
        items = server.completion(fixture, marker)
        expect_labels(items, ["MAX_STAGES", "DEFAULT_ATTACK", "describe"], exactly=True)
        expect_no_labels(items, ["attack", "release", "retrigger"])


@test("completion: instance members of a struct-typed declaration",
      requires="completionProvider")
def _(workspace, server):
    fixture = workspace.open("completion_instance.cksp")
    expect(server.diagnostics(fixture) == [], "precondition: the fixture compiles cleanly")
    items = server.completion(fixture, "instance")
    expect_labels(items, ["count", "zone", "tick"], exactly=True)
    # An instance is not the type: statics and lifecycle methods stay out.
    expect_no_labels(items, ["MAX", "__init__", "__del__", "__repr__", "self"])


@test("completion: <self.> resolves inside a hand written constructor",
      requires="completionProvider")
def _(workspace, server):
    # The lifecycle methods are filtered out of the offered surface; that filter must
    # not also drop the self scope of the body they were written in.
    fixture = workspace.open("completion_instance.cksp")
    items = server.completion(fixture, "self_in_init")
    expect_labels(items, ["count", "zone", "tick"], exactly=True)
    expect_no_labels(items, ["__init__", "__repr__", "self"])


@test("completion: a chain walks on through member types", requires="completionProvider")
def _(workspace, server):
    fixture = workspace.open("completion_instance.cksp")
    for marker in ("chained", "qualified", "element", "self_member"):
        items = server.completion(fixture, marker)
        expect_labels(items, ["idx", "file", "ping"], exactly=True)
        expect_no_labels(items, ["MAX"])


@test("completion: each construct is labelled as what it was declared as",
      requires="completionProvider")
def _(workspace, server):
    # The construct word cannot be derived from the LSP kind: one Field covers a
    # <family> member and a struct member, one Method covers a <static function> and a
    # method. Kind and word are asserted together so neither can drift.
    instance = workspace.open("completion_instance.cksp")
    for label, kind, category in [("idx", 5, "member"), ("ping", 2, "method")]:
        item = item_named(server.completion(instance, "chained"), label)
        expect(item["kind"] == kind, f"{label}: kind {item['kind']}, expected {kind}")
        expect(
            item["labelDetails"]["description"] == category,
            f"{label}: labelled {item['labelDetails'].get('description')!r}, expected {category!r}",
        )

    valid = workspace.open("completion_valid.cksp")
    for marker, label, kind, category in [
        ("struct_static", "MAX_STAGES", 21, "static const"),
        ("struct_static", "describe", 2, "static function"),
        ("const_member", "HIGH", 21, "const"),
        ("family_member", "index", 5, "family"),
        ("namespace_member", "rate", 6, "variable"),
        ("namespace_member", "mixer", 9, "namespace"),
        ("namespace_member", "fade", 3, "function"),
    ]:
        item = item_named(server.completion(valid, marker), label)
        expect(item["kind"] == kind, f"{label}: kind {item['kind']}, expected {kind}")
        expect(
            item["labelDetails"]["description"] == category,
            f"{label}: labelled {item['labelDetails'].get('description')!r}, expected {category!r}",
        )


@test("completion: same-named locals resolve per function body",
      requires="completionProvider")
def _(workspace, server):
    # A function body is the only scope in CKSP that hides a name, so this is the
    # one case where the same identifier must yield different members.
    fixture = workspace.open("completion_instance.cksp")
    expect_labels(server.completion(fixture, "local_zone"), ["idx", "file", "ping"], exactly=True)
    expect_labels(server.completion(fixture, "local_group"), ["zone", "count", "tick"], exactly=True)


@test("completion: a struct name still offers only its statics",
      requires="completionProvider")
def _(workspace, server):
    fixture = workspace.open("completion_instance.cksp")
    items = server.completion(fixture, "type_qualified")
    expect_labels(items, ["MAX"], exactly=True)
    expect_no_labels(items, ["idx", "file", "ping"])


@test("completion: an unqualified position offers what is visible there",
      requires="completionProvider")
def _(workspace, server):
    fixture = workspace.open("completion_instance.cksp")
    items = server.completion(fixture, "inside_function", trigger_character=None)
    # Globals, structs, functions - plus this function's own locals.
    expect_labels(items, ["Zone", "Group", "Holder", "g", "h", "uses_zone", "uses_group",
                          "item", "only_in_uses_zone", "audio"])
    # Struct members are only reachable through an instance, and the compiler's
    # renamed machinery is not nameable at all.
    expect_no_labels(items, ["idx", "file", "ping", "count", "self", "MAX"])
    for label in labels_of(items):
        expect(
            not label.endswith(("0", "1", "2")) or label in ("g", "h"),
            f"{label!r} looks like a uniquified internal name",
        )


@test("completion: defines and macros are offered", requires="completionProvider")
def _(workspace, server):
    # These are substituted away before the AST exists, so they come from the PreAST.
    fixture = workspace.open("completion_preprocessor.cksp")
    expect(server.diagnostics(fixture) == [], "precondition: the fixture compiles cleanly")
    items = server.completion(fixture, "global", trigger_character=None)
    expect_labels(items, ["MAX_VOICES", "SCALED", "AREA", "setup", "no_args"])
    # The declarations a macro expands into are real and stay offered.
    expect_labels(items, ["x", "vol_count", "plain_thing"])

    for label, category, kind in [("MAX_VOICES", "define", 21), ("AREA", "define", 3),
                                  ("setup", "macro", 3), ("no_args", "macro", 3)]:
        item = item_named(items, label)
        expect(item["kind"] == kind, f"{label}: kind {item['kind']}, expected {kind}")
        expect(
            item["labelDetails"]["description"] == category,
            f"{label}: labelled {item['labelDetails'].get('description')!r}",
        )

    # A parameterless define stands for a value, and the harvest runs after the
    # substitution pass folded it, so the detail shows what it actually expands to.
    expect(
        item_named(items, "SCALED")["detail"] == "define SCALED := 48",
        f"unexpected define detail: {item_named(items, 'SCALED').get('detail')}",
    )
    # Parameter spelling is the source one, including the macro's # markers.
    expect(
        item_named(items, "setup")["labelDetails"]["detail"] == "(#name#)",
        f"unexpected macro parameters: {item_named(items, 'setup').get('labelDetails')}",
    )


@test("completion: a name that merely contains dots is reachable through them",
      requires="completionProvider")
def _(workspace, server):
    # <macro nks.init()> is one flat name; nothing declares `nks`, so the qualified
    # branch has to derive the members from the names.
    fixture = workspace.open("completion_dotted_names.cksp")
    expect(server.diagnostics(fixture) == [], "precondition: the fixture compiles cleanly")

    items = server.completion(fixture, "dotted")
    expect_labels(items, ["init", "update_labels", "MAX", "counter", "labels"], exactly=True)
    expect(
        item_named(items, "init")["labelDetails"]["description"] == "macro",
        f"got {item_named(items, 'init').get('labelDetails')}",
    )
    # The signature keeps the dots, because that is how the name was written.
    expect(
        item_named(items, "update_labels")["detail"] == "function nks.update_labels()",
        f"unexpected signature: {item_named(items, 'update_labels').get('detail')}",
    )

    # Unqualified they stay reachable under their full names, so typing "nks" matches.
    unqualified = server.completion(fixture, "global", trigger_character=None)
    expect_labels(unqualified, ["nks.init", "nks.update_labels", "nks.MAX",
                                "nks.counter", "nks.labels", "nks_ready"])


@test("completion: locals of another function stay invisible",
      requires="completionProvider")
def _(workspace, server):
    fixture = workspace.open("completion_instance.cksp")
    # `only_in_uses_zone` is declared in uses_zone; from the callback it must be gone.
    items = server.completion(fixture, "instance_scope", trigger_character=None)
    expect_no_labels(items, ["only_in_uses_zone"])
    expect_labels(items, ["g", "h", "uses_zone"])


@test("completion: a loop or branch body ends the life of its declarations",
      requires="completionProvider")
def _(workspace, server):
    fixture = workspace.open("completion_block_scopes.cksp")
    expect(server.diagnostics(fixture) == [], "precondition: the fixture compiles cleanly")

    def offered(marker):
        return server.completion(fixture, marker, trigger_character=None)

    # Innermost out: each <end> takes the names declared behind it with it.
    expect_labels(offered("in_branch"),
                  ["in_branch", "in_inner_loop", "in_outer_loop", "in_function", "set_id"])
    expect_no_labels(offered("in_inner_loop"), ["in_branch"])
    expect_labels(offered("in_inner_loop"), ["in_inner_loop", "ks", "in_outer_loop"])
    expect_no_labels(offered("after_inner_loop"), ["in_branch", "in_inner_loop", "ks"])
    expect_labels(offered("after_inner_loop"), ["in_outer_loop", "inst"])
    expect_no_labels(offered("after_outer_loop"), ["in_outer_loop", "inst"])
    expect_labels(offered("after_outer_loop"), ["in_function", "set_id"])

    expect_labels(offered("in_while"), ["in_while"])
    expect_no_labels(offered("after_while"), ["in_while"])

    # A parameter belongs to its own function and to no other.
    expect_no_labels(offered("after_outer_loop"), ["only_in_helper"])

    # A callback declaration is hoisted to the global scope, so no block hides it.
    expect_labels(offered("after_callback_loop"), ["hoisted_from_loop"])


@test("completion: inside a namespace its members are offered unqualified",
      requires="completionProvider")
def _(workspace, server):
    fixture = workspace.open("completion_valid.cksp")
    items = server.completion(fixture, "inside_namespace", trigger_character=None)
    # Written inside `namespace audio`, <audio.rate> is typed as <rate>.
    expect_labels(items, ["rate", "channels", "mixer", "fade", "scan"])
    expect_no_labels(items, ["audio.rate", "audio.channels"])


@test("completion: a shortened qualifier resolves against the enclosing namespace",
      requires="completionProvider")
def _(workspace, server):
    fixture = workspace.open("completion_shadowed.cksp")
    items = server.completion(fixture, "inner")
    expect_labels(items, ["volume", "mute"])


@test("completion: the enclosing block picks between same-named qualifiers",
      requires="completionProvider")
def _(workspace, server):
    # Two nested `mixer` blocks exist. Without the scope pass, suffix matching
    # merges both member sets; `exactly` is what makes this discriminating.
    fixture = workspace.open("completion_ambiguous.cksp")
    expect(server.diagnostics(fixture) == [], "precondition: the fixture compiles cleanly")
    items = server.completion(fixture, "in_audio")
    expect_labels(items, ["volume", "mute"], exactly=True)
    items = server.completion(fixture, "in_midi")
    expect_labels(items, ["channel", "port"], exactly=True)


@test("completion: a fully qualified chain resolves without a scope",
      requires="completionProvider")
def _(workspace, server):
    fixture = workspace.open("completion_ambiguous.cksp")
    items = server.completion(fixture, "outside")
    expect_labels(items, ["volume", "mute"], exactly=True)


@test("completion: an unknown qualifier yields nothing", requires="completionProvider")
def _(workspace, server):
    fixture = workspace.open("completion_valid.cksp")
    text = fixture.text.replace("message(mode)", "nichtda.<|unknown|>")
    updated = server.did_change(fixture, text)
    items = server.completion(updated, "unknown")
    expect(items == [], f"unknown qualifier must not fall back to everything: {labels_of(items)}")


@test("completion: answers from the last good snapshot while the buffer is broken",
      requires="completionProvider")
def _(workspace, server):
    # Open valid, then break it with a trailing dot — the normal state while
    # typing. The failed analysis must not take the previous snapshot with it.
    fixture = workspace.open("completion_valid.cksp")
    broken = server.did_change(
        fixture, fixture.text.replace("audio.mixer.volume := 90", "audio.<|typing|>")
    )
    expect(server.diagnostics(broken), "precondition: the trailing dot breaks the file")
    items = server.completion(broken, "typing")
    expect_labels(items, ["rate", "channels", "mixer", "reset"])


@test("completion: survives a semantic error in the buffer", requires="completionProvider")
def _(workspace, server):
    # A file that parses but fails later still yields a full index: the salvage
    # in Compiler::analyze harvests the AST after desugaring attached the
    # prefixes. No previous successful analysis is involved here.
    fixture = workspace.write("semantic_error.cksp", (
        "namespace audio\n"
        "    declare rate: int := 44100\n"
        "    declare channels: int := 2\n"
        "end namespace\n"
        "\n"
        "on init\n"
        "    message(undeclared_thing)\n"
        "    audio.rate := 1\n"
        "end on\n"
    ))
    server.did_open(fixture)
    expect(server.diagnostics(fixture), "precondition: the file has a semantic error")
    items = server.completion_at(fixture, Position(7, 10))
    expect_labels(items, ["rate", "channels"])


@test("completion: a file that never parsed has nothing to offer",
      requires="completionProvider")
def _(workspace, server):
    # Documents the boundary of v1: a tokenizer/parser error leaves no AST, so
    # completion needs one analysis of the entry that got at least past parsing.
    # If parser error recovery is ever added, this expectation should flip.
    fixture = workspace.open("completion_broken.cksp")
    expect(server.diagnostics(fixture), "precondition: the fixture does not parse")
    items = server.completion(fixture, "typing")
    expect(items == [], f"expected no completions without a parsed AST, got {labels_of(items)}")


@test("completion: no suggestions inside strings, comments or after a bracket",
      requires="completionProvider")
def _(workspace, server):
    # The qualifier scanner reads raw text, so these are the cases where it can
    # wrongly think it sits behind a qualifier.
    fixture = workspace.open("completion_valid.cksp")
    header = 'namespace audio\n    declare rate: int := 44100\nend namespace\n\non init\n'
    # An index is deliberately *not* listed here: `zones[0].` completes the element
    # type, which completion_instance.cksp covers.
    cases = {
        "string literal": '    message("audio.")\n',
        "block comment": '    { audio. }\n',
        "line comment": '    // audio.\n',
        "after a closing paren": '    declare x := abs(1).\n',
        "after a number": '    declare x := 1.\n',
    }
    for index, (what, line) in enumerate(cases.items(), start=2):
        body = header + line + "end on\n"
        updated = server.did_change(fixture, body, version=index, wait=False)
        cursor_line = 5 + line.count("\n") - 1
        cursor = Position(cursor_line, line.splitlines()[-1].index(".") + 1)
        items = server.completion_at(updated, cursor)
        expect(items == [], f"{what}: expected no completions, got {labels_of(items)}")


@test("completion: stays correct while the qualifier is typed character by character",
      requires="completionProvider")
def _(workspace, server):
    # The hardest case: every keystroke re-triggers the debounced analysis, so
    # this is where staleness and races surface. Static fixtures never show it.
    fixture = workspace.write("typing.cksp", (
        "namespace audio\n"
        "    declare rate: int := 44100\n"
        "    declare channels: int := 2\n"
        "end namespace\n"
        "\n"
        "on init\n"
        "    message(audio.rate)\n"
        "end on\n"
    ))
    server.did_open(fixture)

    head = (
        "namespace audio\n"
        "    declare rate: int := 44100\n"
        "    declare channels: int := 2\n"
        "end namespace\n"
        "\n"
        "on init\n"
        "    message(audio.rate)\n"
        "    "
    )
    tail = "\nend on\n"
    typed = ""
    for version, character in enumerate("audio.", start=2):
        typed += character
        current = server.did_change(fixture, head + typed + tail, version=version)
        if character != ".":
            continue
        cursor = Position(7, len(typed) + 4)
        items = server.completion_at(current, cursor)
        expect_labels(items, ["rate", "channels"])


# ==========================================================================
# SublimeKSP migration — taskfunc and TCM are rejected, but with a way out
# ==========================================================================

@test("migration: a taskfunc is named as SublimeKSP's, not as an unknown construct",
      entry_points=["migration_taskfunc.cksp"])
def _(workspace, server):
    fixture = workspace.open("migration_taskfunc.cksp")
    diagnostics = server.diagnostics(fixture)
    expect(diagnostics, "expected a diagnostic for the taskfunc block")
    message = diagnostics[0]["message"]
    expect("taskfunc" in message and "SublimeKSP" in message,
           f"the diagnostic should name the construct and its dialect; got {message!r}")
    expect_position(position_of(diagnostics[0]), fixture, "taskfunc",
                    what="taskfunc diagnostic")


@test("migration: the taskfunc quick fix rewrites keywords, parameters and tcm.wait",
      entry_points=["migration_taskfunc.cksp"])
def _(workspace, server):
    fixture = workspace.open("migration_taskfunc.cksp")
    action = action_titled(server.code_actions(fixture), "get_random_value")
    ported = apply_action(fixture.text, action, fixture)
    first = ported.split("end function")[0]
    for expected in ["function get_random_value(min, max) -> result", "wait(500000)"]:
        expect(expected in first, f"expected {expected!r} in the ported block:\n{first}")
    expect("taskfunc" not in first, f"taskfunc keyword survived:\n{first}")


@test("migration: applying every quick fix ports the script to compiling cksp",
      entry_points=["migration_taskfunc.cksp"])
def _(workspace, server):
    fixture = workspace.open("migration_taskfunc.cksp")
    ported, applied = port_with_quick_fixes(fixture, server, fixture.text)
    expect(len(applied) == 4, f"expected four fixes (two taskfuncs, the arrow return and "
                              f"tcm.init); applied {applied}")
    expect(not server.diagnostics(fixture),
           f"ported script still reports {messages_of(server.diagnostics(fixture))}")
    for expected in ["function swap_get_max(ref a, ref b, ref max)",
                     "#pragma max_callback_depth(100)",
                     "return r"]:
        expect(expected in ported, f"expected {expected!r} in:\n{ported}")
    expect("taskfunc" not in ported and "tcm." not in ported,
           f"SublimeKSP constructs survived:\n{ported}")


@test("migration: tcm.init with a literal becomes the pragma that replaces it",
      entry_points=["tcm_init.cksp"])
def _(workspace, server):
    fixture = workspace.write("tcm_init.cksp",
                              "on init\n    tcm.init(64)\nend on\n")
    server.did_open(fixture)
    action = action_titled(server.code_actions(fixture), "max_callback_depth")
    expect("#pragma max_callback_depth(64)" in apply_action(fixture.text, action, fixture),
           "the fix should write the pragma with the call's own depth")


@test("migration: a computed tcm.init depth is explained instead of half-fixed",
      entry_points=["tcm_computed.cksp"])
def _(workspace, server):
    # A pragma argument is read before anything is folded, so a fix here would
    # produce a file that does not compile. The message has to carry the port.
    fixture = workspace.write(
        "tcm_computed.cksp",
        "on init\n    declare depth := 64\n    tcm.init(depth * 2)\nend on\n")
    server.did_open(fixture)
    diagnostics = server.diagnostics(fixture)
    expect(diagnostics, "expected a diagnostic for tcm.init")
    expect("max_callback_depth" in diagnostics[0]["message"],
           f"the message should name the option; got {diagnostics[0]['message']!r}")
    expect(not server.code_actions(fixture),
           "no fix may be offered for a depth that cannot become a pragma argument")


@test("migration: an unknown tcm call is explained without a fix",
      entry_points=["tcm_unknown.cksp"])
def _(workspace, server):
    fixture = workspace.write("tcm_unknown.cksp",
                              "on note\n    tcm.reset()\nend on\n")
    server.did_open(fixture)
    diagnostics = server.diagnostics(fixture)
    expect(diagnostics, "expected a diagnostic for the unknown tcm call")
    expect("Task Control Module" in diagnostics[0]["message"],
           f"the message should name TCM; got {diagnostics[0]['message']!r}")
    expect(not server.code_actions(fixture), "an unknown tcm call has nothing to rewrite to")


@test("migration: a name that differs only in case is offered as an edit",
      entry_points=["case_variable.cksp"])
def _(workspace, server):
    # SublimeKSP resolves names case-insensitively, so this is the single most common
    # error a ported script produces.
    source = ("on init\n    declare myCounter\nend on\n"
              "\non note\n    MyCounter := 1\nend on\n")
    fixture = workspace.write("case_variable.cksp", source)
    server.did_open(fixture)
    action = action_titled(server.code_actions(fixture), "MyCounter")
    expect(action["title"] == "Change 'MyCounter' to 'myCounter'",
           f"unexpected title: {action['title']!r}")
    ported = apply_action(source, action, fixture)
    server.did_change(fixture, ported)
    expect(not server.diagnostics(fixture),
           f"corrected source still reports {messages_of(server.diagnostics(fixture))}")


@test("migration: a miscased function name and a dotted one are both offered",
      entry_points=["case_function.cksp", "case_dotted.cksp"])
def _(workspace, server):
    for name, source, title in [
        ("case_function.cksp",
         "function doThing(a)\n    message(a)\nend function\n"
         "\non note\n    DoThing(1)\nend on\n",
         "Change 'DoThing' to 'doThing'"),
        # The reference token spans the whole dotted name, so the edit replaces all of it.
        ("case_dotted.cksp",
         "namespace audio\n    declare sampleRate := 44100\nend namespace\n"
         "\non init\n    message(audio.SampleRate)\nend on\n",
         "Change 'audio.SampleRate' to 'audio.sampleRate'"),
    ]:
        fixture = workspace.write(name, source)
        server.did_open(fixture)
        action = action_titled(server.code_actions(fixture), "Change")
        expect(action["title"] == title, f"{name}: unexpected title {action['title']!r}")
        server.did_change(fixture, apply_action(source, action, fixture))
        expect(not server.diagnostics(fixture),
               f"{name} still reports {messages_of(server.diagnostics(fixture))}")


@test("migration: a real typo stays a suggestion and offers no edit",
      entry_points=["case_typo.cksp"])
def _(workspace, server):
    # Same letters in a different order is a guess about intent, not the same identifier;
    # only a pure case difference is safe to apply on the user's behalf.
    fixture = workspace.write("case_typo.cksp",
                              "on init\n    declare counter\nend on\n"
                              "\non note\n    countr := 1\nend on\n")
    server.did_open(fixture)
    diagnostics = server.diagnostics(fixture)
    expect(any("counter" in message for message in messages_of(diagnostics)),
           f"the message should still suggest the name; got {messages_of(diagnostics)}")
    expect(not server.code_actions(fixture),
           "a typo that is not a case difference must not be offered as an edit")


if __name__ == "__main__":
    raise SystemExit(run_suite())
