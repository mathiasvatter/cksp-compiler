# Changelog

## \[0.0.7-alpha.2] - 2024-12-12

### Added

* Support for **initializer lists** in `foreach` loops.
* Introduced **new `foreach` loop syntax** for improved readability and flexibility.
* Moved `ForEach` and `For` loop handling into the **AST lowering phase** for better compilation logic.
* Introduced `NodePairs` to support advanced loop and iteration constructs.
* Implemented `iterate_over` method for `NDArrayRef`, improving array iteration capabilities.
* Completed initial implementation of the **`PointerScope` class** (pending further testing).
* Added **macOS x86 executable build**.

### Fixed

* Resolved bug with **dimension expansion** during array initializations.
* Fixed **dimension inflation** issue in **thread-unsafe array declarations**.
* Corrected the **array copy function** to properly handle indexes.
