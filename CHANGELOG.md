# Changelog

## [0.0.9-alpha.3] - 2026-05-20

> Note: `namespaces` and `struct` definitions are now processed in the order they appear in the global scope. Any global variables used by them must be declared first. If your `struct` methods currently rely on variables declared inside `on init`, move those variable declarations to the global scope before the affected `struct` definition.

### Added

- Added support for **struct definitions nested inside namespaces**.
- Added support for **namespaces as statements** in the global scope.

### Changed

- Updated **struct parsing and lowering** so structs are handled as global statements and inlined **in declaration order**.
- Improved compiler handling for **namespaces and structs** across parsing and compiler passes.

### Fixed

- Fixed **case collision resolution** so references to data structures are handled correctly.
- Fixed an issue where the generated **`__decr__` method** could receive an incorrect `nullptr` reference count when an **ndarray** was passed.
- Fixed incorrect lowering of **`delete` statements** when deleting an **array of objects**.
- Fixed a compiler stability issue caused by **dangling pointers** during struct inlining.
