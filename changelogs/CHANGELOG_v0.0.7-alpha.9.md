# Changelog

## \[0.0.7-alpha.9] - 2025-09-18

### Added

* Introduced support for the **ternary operator**:
  * Includes proper **tokenizing and parsing**.
  * Lowered into KSP-compatible `if` statements.
  * Implements **short-circuiting** behavior for condition evaluation.
* Extended **type casting support**:
  * `int()` and `real()` now work on `object`, `real`, `int` and `bool`.
* **Initial implementation of a `Boolean` type**:
  * Internally represented as an `int`.
  * Supports boolean expression lowering via integer functions.
* Introduced `bool()` **typecasting function** to convert values to boolean.
* Added **condition sanitization**:
  * Transforms literals or variable references in `if`/`while` conditions into KSP-compatible comparisons (e.g. `while(true)` → `while(1 # 0)`).
* Added support for the **string concatenation operator `<&>`** in compound assignment expressions.

### Changed

* **Ternary expressions** are lowered by substituting them with a function call, where the original expression is converted into an `if` statement.
* Boolean variables used in conditions are now **automatically converted into comparisons** to ensure **KSP compatibility**.
* **Function inlining strategy** optimized:
  * Call/parameter stack transformations now apply only if a function has more than one call site.
* Improved `for` loop **range evaluation**:
  * Range parameters are now **evaluated once** and stored in **local variables**—just like in Python.

### Fixed

* **Undeclared variable errors** in expressions where functions were used as `l_value` in declarations.
* **Undeclared variable errors** for `pgs` variables.
* Fixed **issue #38**:
  * Arrays used as function arguments no longer incorrectly receive primitive types.
  * Also addressed a reintroduced issue with **UI control variables**, which are now handled correctly by deferring type checks until after `get_ui_id()` wrapping.
