# Changelog

## [0.1.0-alpha.1]

>[!IMPORTANT]
>This alpha release introduces the first implementation of the **cksp Language Server**. It provides live diagnostics, code navigation, references, renaming, and document highlights for compatible editors. The release also adds **optional chaining**, **nullish coalescing**, and a new **code obfuscation** mode, alongside a broad set of compiler fixes and diagnostic improvements.

### Added
- Added the first implementation of the **cksp Language Server**.
  - Publishes live compiler diagnostics for open and unsaved files.
  - Supports **go to definition**, **find references**, **rename symbol**, and **document highlights** across all languages features and files.
  - Resolves configured main files and standalone entry points so diagnostics and navigation follow the correct import graph.
  - Uses debounced background analysis to keep editor feedback responsive while typing.
- Added **optional chaining** with `?.`, including nested chains, method calls, and guarded assignment targets. Added **nullish coalescing** with `??` for safely using optional-chain results in expressions ([#89](<https://github.com/mathiasvatter/cksp-compiler/issues/89>)).
    ```cksp
    wrapper?.inner.x := 11
    declare v1 := point?.x ?? 0
    ```
- Added a new **obfuscation pass**, enabled with `--obfuscate` or `#pragma obfuscate("true")`, which randomizes user-defined variable names and substitutes most KSP builtin constants with their internal Kontakt integer values ([#113](<https://github.com/mathiasvatter/cksp-compiler/issues/113>)).
- Added `_ := ...` as an explicit way to discard function return values without emitting a warning.
- Added compiler warnings when:
  - A return value is discarded by calling a return function as a bare statement.
  - A pass-by-value function parameter is modified even though the change is not visible to the caller ([#103](<https://github.com/mathiasvatter/cksp-compiler/issues/103>)).
- Added crash reports with stack traces when the compiler or language server encounters a segmentation fault.

### Changed
- Return functions can now be called as bare statements when their result is intentionally unused ([#99](<https://github.com/mathiasvatter/cksp-compiler/issues/99>)).
- Void functions may now reach the end of their body without an explicit return, even when other branches return early.
- Improved compiler diagnostics with precise source ranges, function call stacks and clearer parser errors.
- Improved type-inference performance

### Fixed

- Fixed crashes when declaring **UI controls or UI-control arrays in global scope**.
- Fixed token-pasted macro arguments being substituted too early, which could drop suffixes and generate invalid KSP.
- Fixed incorrect wildcard-dimension resolution in `num_elements()`.
- Fixed constant folding of real-number comparisons.
- Fixed incorrect struct-constructor counts and heap-size calculations when constructors were reached through static function calls or loops.
- Fixed pointer cleanup being skipped for temporary objects and on early or trailing function returns.
- Fixed local constants shadowing global constants being reported as redeclarations

As this is the first alpha containing the language server, please report regressions, confusing diagnostics, and editor-integration issues on GitHub.

Cheers,
Mathias
