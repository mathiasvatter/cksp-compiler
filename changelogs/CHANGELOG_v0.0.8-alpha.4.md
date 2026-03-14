# Changelog

## [v0.0.8-alpha.4] - 2025-11-10

### Added

* Introduced new **pragma directive** `max_callback_depth(<val>)` allowing users to define the **maximum number of concurrent callbacks** ([#55](https://github.com/mathiasvatter/cksp-compiler-issues/issues/55)).
* Added **error handling** for invalid variable names that still contain `#` characters after preprocessing ([#54](https://github.com/mathiasvatter/cksp-compiler-issues/issues/54)).
* Added **error reporting** when using **`polyphonic`** or **`ui_control`** modifier keywords inside **struct member declarations**, which are not permitted.

### Fixed

* Fixed **segmentation fault** during **monomorphization**, improving compiler stability.
* Fixed [#51](https://github.com/mathiasvatter/cksp-compiler-issues/issues/51): **substring substitutions in `#define` parameters** are now supported.
* Fixed [#52](https://github.com/mathiasvatter/cksp-compiler-issues/issues/52): **initializer lists** without commas are now correctly flagged as invalid.
* Fixed [#53](https://github.com/mathiasvatter/cksp-compiler-issues/issues/53): the **`declare` keyword** is no longer required when defining **struct members** with modifier keywords (e.g., `const`, `read`, etc.).

### Meta

* Compiler version bumped to **v0.0.8-alpha.4**.
