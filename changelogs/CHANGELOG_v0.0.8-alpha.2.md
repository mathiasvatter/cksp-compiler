# Changelog

## [v0.0.8-alpha.2] - 2025-10-20

### Added

* Introduced new **pragma option** `<combine_callbacks>` to enable **callback merging functionality** ([#43](https://github.com/mathiasvatter/cksp-compiler-issues/issues/43)).
* Improved **error and warning messages** when multiple callbacks of the same type are defined.

### Fixed

* Fixed issue [#26](https://github.com/mathiasvatter/cksp-compiler-issues/issues/26) where **comparisons inside message functions** would cause **Kontakt runtime errors** — these now correctly evaluate to boolean values.
* Fixed issue [#42](https://github.com/mathiasvatter/cksp-compiler-issues/issues/42) where **declaring a `ui_button` with parameters** could trigger a **vector out-of-bounds error**.
