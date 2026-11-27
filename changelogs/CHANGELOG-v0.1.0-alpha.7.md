# Changelog v0.1.0-alpha.7

> [!IMPORTANT]
> This alpha brings **semantic highlighting**, **hovers** and **inlay hints** for inferred types to the language server, so the editor now shows more of what the compiler knows about a script. On the language side, it is mostly about **correctness**: reference counting, namespaces, generic functions and the nesting limit of Kontakt's parser all got a round of fixes.

## Language

### Improved
- **Explicit `delete` now destroys the instance** and clears the pointer or array slot it was deleted through, no matter how many references are left. Releasing a reference stays the job of `:= nil`.
- **Large scripts no longer fail in Kontakt with *"memory exhausted"***. The compiler now keeps nested code within the actual limit of Kontakt's parser instead of estimating it.
- **Too many constructor arguments** are reported at the call instead of as an internal error at `__init__` ([#135](https://github.com/mathiasvatter/cksp-compiler/issues/135)).
- **Wrong argument counts on methods** are reported as such instead of as a missing method.
- A **bare `return` outside a function** is reported as an error instead of crashing the compiler.
- **Diagnostics in an imported file name the entry points they come from**. A shared import can compile from one entry point and fail from another, and the merged message did not say which one failed ([#156](https://github.com/mathiasvatter/cksp-compiler/issues/156)).

### Fixed
- Fixed [#140](https://github.com/mathiasvatter/cksp-compiler/issues/140): the **implicit string conversion** of an object ignored a user-defined `__repr__`.
- Fixed [#141](https://github.com/mathiasvatter/cksp-compiler/issues/141): **assigning `nil` to a pointer field** did not release the old reference.
- Fixed [#144](https://github.com/mathiasvatter/cksp-compiler/issues/144): a **pointer assigned from a ternary or a constructor** was counted wrong. A constructor no longer retains a reference `__init__` already counted.
- Fixed **loop-local pointers** not being released before `break` and `continue`.
- Fixed [#143](https://github.com/mathiasvatter/cksp-compiler/issues/143): **locals lost thread-safety** when a call was nested in `wait()` or in the arguments of a waiting function.
- Fixed [#146](https://github.com/mathiasvatter/cksp-compiler/issues/146), [#147](https://github.com/mathiasvatter/cksp-compiler/issues/147) and [#136](https://github.com/mathiasvatter/cksp-compiler/issues/136): **locals of namespace functions leaked into the namespace**. `Ns.x` and `obj?.method()` could resolve to a local of the same name, and a for-each variable could be undeclared when a nested block had declared the same name earlier.
- Fixed [#148](https://github.com/mathiasvatter/cksp-compiler/issues/148): an **untyped declaration from a generic call** failed when the function was specialized for both `int` and `real`. Monomorphized clones now return their specialized types, and `declare r := add(1.0, 2.0)` no longer ends up as an integer.
- Fixed a **function with a single call site** keeping the `number` type from the first inference pass after its parameters were bound, as in `return a + b`.
- Fixed [#133](https://github.com/mathiasvatter/cksp-compiler/issues/133): **discarding an object with `_ :=`** crashed the compiler, and the first `_ := …` fixed the type of every other `_`.
- Fixed [#134](https://github.com/mathiasvatter/cksp-compiler/issues/134): `search` and `sort` accepted **string arrays**, which Kontakt does not support.
- Fixed [#150](https://github.com/mathiasvatter/cksp-compiler/issues/150): **identity casts** such as `int($x)` reached the `-O0` output, where Kontakt does not parse them. They are now removed at every optimization level.

## Migrating from SublimeKSP

### Fixed
- Fixed [#155](https://github.com/mathiasvatter/cksp-compiler/issues/155): a **macro parameter followed by a parameter list**, such as `#name#(x)` in a function header, crashed the preprocessor. The argument is now substituted as a name; anything other than a single word is reported as a preprocessor error.

## Language Server

### Added
- Added **semantic tokens**. Variables, functions, structs, namespaces, overloaded operators and CKSP builtins are highlighted by what they are, including their modifiers such as `const` or `persistent`.
- Added **hovers** served by the language server. They show the declaration's signature, persistence, defines, macros and doc comments. Hovering with Cmd/Ctrl previews the declaration.
- Added **inlay hints for inferred types** on untyped variables, arrays, parameters and function return types.
- Added **parameter name inlay hints** for user-defined functions and methods. KSP builtins keep the hints from the extension's database.

Please keep reporting regressions, confusing diagnostics, and editor-integration issues on GitHub.

Cheers,  
Mathias
