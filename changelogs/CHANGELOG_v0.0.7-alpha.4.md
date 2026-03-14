# Changelog

## \[0.0.7-alpha.4] – 2025-07-14

### Added

* **New function handling** with parameter stack transformation, simulating call-by-value semantics.
* **Namespaces** and the ability to define variables outside the `on_init` callback.
* Source range information for all AST nodes.
* Parsing of `static` variables and full support for the new `ref` keyword.
* A `#pragma pass_by` option to toggle between reference/value parameter passing.
* New built-in restrictions around `EVENT_VELOCITY`.
* Warnings and guidance for UI control parameters to use `<ref>`, resolving prior integration issues.
* A `ParameterReuse` algorithm plus a `ParameterAssignmentTransformation` to streamline efficient parameter variable handling.
* NDArray lowering now emits explicit `NodeBlock` constructs instead of inlined functions.
* Updated engine constants package targeting version **k8.4.0**.

### Changed

* Scalar variable parameters now default to pass-by-value, with the `#pragma pass_by` directive allowing for reference passing.
* Arrays now default to pass-by-reference; removed legacy `ParameterPromotion` classes in favor of the unified reuse/assignment infrastructure.
* Reordered transformation phases: moved `MarkThreadSafe` and `DimensionExpansion` to after parameter-stack transformation and corrected lexical scoping.

### Fixed

* Edge cases in const member handling and access-chain lowering.
* Multiple **parameter-stack transformation** bugs (missing return assignments, incomplete dependency analysis).
* Restrictive built-in variable handling and UI-control parameter passing modes.
* Edge cases in **type inference** for better correctness.
* Parsing errors around the new `ref` keyword.
* Tokenizer corner-case failures.

### Removed

* Deprecated `ParameterPromotion` classes and related legacy artifacts.
