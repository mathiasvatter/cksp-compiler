# Changelog

## \[0.0.8-alpha.1] – 2025-10-07

### Added

* Added **TOC4** to the testing pipeline for enhanced compatibility testing.

### Changed

* Improved **short-circuit evaluation transformation**: now only applied when user-defined functions (that are not expression functions) are used in conditions.

### Fixed

* Fixed **issue #40**: overloaded boolean operators and single-function calls now work correctly in conditional expressions.
* Fixed an issue in **Dead Code Elimination**, where too many assignments were incorrectly removed.
* Fixed a bug where `int()` was not removed during compilation if the argument was already an integer array.

### Removed

* Removed the obsolete `wiki` submodule from the project.
