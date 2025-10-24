# Changelog

## [v0.0.8-alpha.3] - 2025-10-24

### Added

* **CLI options** for `--pass_by` and `--combine_callbacks` to control compiler behavior directly from the command line.
* Added **support for multiple output files** through both CLI (`-o` flags) and `#pragma output_path()` directives ([#36](https://github.com/mathiasvatter/cksp-compiler-issues/issues/36)).
* Implemented **tokenizing and parsing of numbers in scientific notation**, such as `123e4`, `.5e2`, or `1.2e-10`.

### Changed

* Fix bug where some CLI options would be overridden by pragma options. CLI options now always take precedence over pragma options when they were explicitly set. When CLI options are unset, pragma options will be used if available.
* Updated **GitHub workflow** to automatically include the latest **release notes** from `Changelog.md` when publishing a release.

### Fixed

* Fixed [#47](https://github.com/mathiasvatter/cksp-compiler-issues/issues/47): the **`PathHandler`** now creates a missing `.txt` file automatically when referenced via `#pragma output_path`, preventing runtime errors.
* Fixed [#45](https://github.com/mathiasvatter/cksp-compiler-issues/issues/45): **`struct`**, **`macro`**, or **`function`** definitions at the **first line (position 0)** of a file no longer cause a `PreprocessorError`.
* Fixed issue where **`ndarray` size declarations** were not being properly validated as constant expressions.

### Improved

* The **`declare` keyword** is now **optional** when declaring **struct members**, simplifying CKSP struct syntax.
