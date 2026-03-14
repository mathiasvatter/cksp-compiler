# Changelog

## [0.0.8] – 2026-03-14

>This release summarizes all changes in pre-release versions leading up to `0.0.8`. It introduces major improvements to the CKSP compiler, including a fully restructured preprocessor, support for overriding callbacks, scientific notation, and numerous stability fixes.

>With this version, the `cksp-compiler` repository will be **public** under the **Apache-2.0 License**, allowing for open-source contributions.

### Language

- Added support for **`import` statements in any scope**, mirroring `sksp` behaviour.
- Added support for **overriding callbacks**.
- Introduced **scientific notation** support for numeric literals (e.g. `1.2e-10`, `.5e2`).
- Added support to **declare arrays without brackets** when used as variables.
- The **`declare` keyword is now optional** for struct member declarations.

### Compiler

- Introduced new pragma directives:
  - `combine_callbacks`
  - `max_callback_depth(<value>)`
- Added CLI options for:
  - `--combine_callbacks`
  - `--pass_by`
  - multiple output files (`-o`) and `#pragma output_path()`.
- Added validation for:
  - invalid variable names still containing `#` after preprocessing
  - constants declared without initialization
  - usage of built-in variables inside restricted callbacks
- Added new built-in constants introduced with **Kontakt 8.3**.

### Improvements

- Improved **TypeInference** and **ReturnFunctionRewriting** passes.
- Improved parser efficiency.
- Improved efficiency of `import` handling in the preprocessor
- Improved error and warning messages for duplicate callbacks.
- Improved short-circuit evaluation transformation logic in regards to function calls in conditions.

### Fixes

Fixed multiple compiler issues affecting language correctness and runtime behavior:
  - Fix incorrect evaluation of comparisons inside message functions [#26](https://github.com/mathiasvatter/cksp-compiler-issues/issues/26)
  - Fix incorrect transformation of single-quoted strings [#83](https://github.com/mathiasvatter/cksp-compiler-issues/issues/83)
  - Fix incorrect lowering of compound assignments when using `get_control` shorthands [#72](https://github.com/mathiasvatter/cksp-compiler-issues/issues/72)
  - Fix local variables inside ternary expressions causing `UndeclaredVariable` errors [#84](https://github.com/mathiasvatter/cksp-compiler-issues/issues/84)
  - Fix parsing errors for empty namespaces [#81](https://github.com/mathiasvatter/cksp-compiler-issues/issues/81)
  - Fix namespace restrictions preventing multiple `ui_control` declarations [#77](https://github.com/mathiasvatter/cksp-compiler-issues/issues/77)
  - Fix segmentation fault during monomorphization
  - Fix substring substitutions in `#define` parameters [#51](https://github.com/mathiasvatter/cksp-compiler-issues/issues/51)
  - Fix incorrect validation of `ndarray` size expressions
  - Fix initializer lists without commas not producing errors [#52](https://github.com/mathiasvatter/cksp-compiler-issues/issues/52)
  - Fix multidimensional array declarations with empty brackets [#58](https://github.com/mathiasvatter/cksp-compiler-issues/issues/58)
  - Fix incorrect f-string lowering with single quotes [#66](https://github.com/mathiasvatter/cksp-compiler-issues/issues/66)
  - Fix optimization pass removing assignments still used inside functions [#63](https://github.com/mathiasvatter/cksp-compiler-issues/issues/63)
  - Fix incorrect dead-code elimination behavior
  - Fix `.txt` files being incorrectly flagged as unsupported
  - Fix missing warning when `set_num_user_zones` is used outside `on init` [#75](https://github.com/mathiasvatter/cksp-compiler-issues/issues/75)
  - Fix issue where `struct`, `macro`, or `function` at file position 0 caused a `PreprocessorError` [#45](https://github.com/mathiasvatter/cksp-compiler-issues/issues/45)

### Preprocessor

Major restructuring of the preprocessor:
  - Rewritten handling of `import`, `macro`, and `define`.
  - Reintroduced `set_condition`, `reset_condition`, `use_code_if`, and `import` as dedicated AST nodes.

