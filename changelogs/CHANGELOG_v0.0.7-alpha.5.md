# Changelog

## \[0.0.7-alpha.5] - 2025-07-17

### Added

* Initial support for **short-circuit evaluation** in boolean expressions when using inline user-defined functions.
* **F-string** parsing and desugaring for inline string interpolation.
* Parsing support for **namespaces** and the **static** keyword.
* Ability to declare **variables** outside of the `on init` callback.
* `set_range` methods in the parser and a new `SourceRange` type on `NodeAST` nodes for richer source-location metadata.
* New pragma option `pass_by` to specify **reference** versus **value** parameter passing.
* `compilerConfig` field added to the `ASTProgram` node to carry compiler configuration flags.

### Changed

* Bumped compiler version to **v0.0.7-alpha.5**.

### Fixed

* Corrected **const** member handling within **structs** during parsing.
* Fixed bugs in the **StructLowering** and **AccessChainLowering** compiler passes to ensure proper code generation.
