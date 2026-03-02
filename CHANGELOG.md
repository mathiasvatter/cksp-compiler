# Changelog

## [v0.0.8-alpha.6] - 2026-03-02

> Note: The Preprocessor got restructured in this release, and even though I tested it as much as I could, there might still be edge cases that cause issues. If you encounter any problems, please report them so I can fix them as soon as possible.

### Added
* Added support for **`import` statements in any scope**, mirroring `sksp` behaviour. This allows for the importing of code segments anywhere without needing to wrap them in a function, macro or namespace.
* Added support to **override callbacks**, providing more flexibility when extending or redefining behavior of architecture.
* Added validation to prevent usage of **built-in variables inside restricted callbacks** (e.g. `EVENT_NOTE` in `on listener`) — the compiler now throws a clear error ([#69](https://github.com/mathiasvatter/cksp-compiler-issues/issues/69)).


### Fixed
* Fixed [#44](https://github.com/mathiasvatter/cksp-compiler-issues/issues/44): `cksp` no longer requires brackets in list block declarations when `sksp` does not.
* Fixed [#72](https://github.com/mathiasvatter/cksp-compiler-issues/issues/72): compound assignments are no longer incorrectly lowered to `inc`/`dec` when using `get_control` shorthands.
* Fixed [#81](https://github.com/mathiasvatter/cksp-compiler-issues/issues/81): empty namespaces no longer cause parsing errors.
* Fixed [#77](https://github.com/mathiasvatter/cksp-compiler-issues/issues/77): namespaces now allow declaration of multiple `ui_control` elements.
* Fixed [#83](https://github.com/mathiasvatter/cksp-compiler-issues/issues/83): strings using single quotes are no longer transformed incorrectly.
* Fixed [#84](https://github.com/mathiasvatter/cksp-compiler-issues/issues/84): local variables inside ternary expressions no longer trigger `UndeclaredVariable` errors.


### Internal Changes
* **Preprocessor completely restructured**:
  - Rewrote handling of **`import`**, **`macro`**, and **`define`** preprocessing.
  - Reintroduced **`set_condition`**, **`reset_condition`**, **`use_code_if`** and **`import`** as explicit **AST nodes**, including full parsing support.
  - Migrated condition and import handling to the **visitor pattern**.
* Improved **TypeInference** and **ReturnFunctionRewriting** passes for better performance.
* Improved parser efficiency by handling `peek()` and `consume()` return values as references.
* Updated CI/CD workflow
