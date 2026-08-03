# Changelog

## [0.1.0-alpha.3]

> [!IMPORTANT]
> This alpha significantly expands the **cksp Language Server** with code actions, smarter project handling, and better editor integration. On the language side it introduces constants as ui control array sizes, import aliases, several syntax improvements, and many compiler and diagnostic fixes. 

## Language

### Added
- Added support for constants and constant initializer lists as **UI-control array sizes** ([#6](<https://github.com/mathiasvatter/cksp-compiler/issues/6>)).
- Added support for `import ... as <alias>`, allowing imported modules to be accessed through namespaces ([#115](<https://github.com/mathiasvatter/cksp-compiler/issues/115>)).
- Added **source-map generation** via `--source-map`. The compiler now generates a `.ckspmap` file that maps generated KSP lines back to the original cksp source.
- Added `default` as shorthand for the default branch of a `select` statement.
- Added support for real-number literals without digits after the decimal point (e.g. `960.`).
- Added symbolic Windows crash reports using `dbghelp`, making compiler crash reports much easier to understand.

### Improved
- Improved diagnostics for undeclared functions with better suggestions for likely intended definitions.
- Improved compiler performance through more efficient AST flattening, move semantics, and cached path handling.
- Obfuscation now preserves builtin KSP engine constants instead of replacing them with integer values.

### Fixed
- Fixed a compiler crash when generating the `__decr__` method of structs with non-linear recursion, e.g. a struct with two members of a recursive struct type or a tree with several child members.
- Fixed the ref counting of non-linearly recursive structs, which neither released the object itself nor the objects reachable through its members. The stack based traversal now pushes every reachable object on the stack of its own struct and drains all stacks of the recursion cycle.
- Fixed generated helper variables such as `_iter0` potentially colliding with user-defined variables.
- Fixed UI-control declarations incorrectly retaining their right-hand-side values ([#117](https://github.com/mathiasvatter/cksp-compiler/issues/117)).
- Fixed array and ndarray references that could initially be interpreted as scalar variables during type inference.

## Language Server

### Added
- Added **Code Actions** for common compiler diagnostics.
  - Automatically adds missing `ref` parameters where required.
  - Converts deprecated function return syntax to the current syntax.
- Added clickable **Document Links** for `import` and `#pragma` paths, allowing referenced files to be opened directly from the editor.
- Added support for **multiple configured project entry points**.
- Added syntax-only analysis for standalone files that are not configured as project entry points, avoiding misleading project-context diagnostics.

### Improved
- Improved navigation and diagnostics by providing more accurate source ranges for namespaces, constants, families, unary operators, and binary operators.
- Improved overall language-server performance through cached reference-path normalization.

### Fixed
- Fixed diagnostics not being cleared after changes in imported files.
- Fixed a language-server crash caused by stale static pointers in the type registry.

Please keep reporting regressions, confusing diagnostics, and editor-integration issues on GitHub.

Cheers,  
Mathias