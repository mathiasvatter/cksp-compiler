# Changelog

## \[0.0.7-alpha.6] - 2025-08-14

### Added

* Support for parsing **empty** return statements, enabling early function exits without return values.
* Introduced `resolve_case_collisions` utility to help translate **case-sensitive cksp** code into **case-insensitive KSP**.
* New workflow for **automated testing** of `.txt` files using the Kontakt interpreter looking for script errors.
* Added `"./"` syntax support in import paths to denote **project root imports**.
* Introduced **NodeString validation** for better macro handling and earlier error reporting when substituting strings with strings.
* Improved handling of **string literals**: converts all single quotes to double quotes for compatibility since ksp seems to be only able to handle double quotes.

### Changed

* Improved error messages and diagnostics in the preprocessor: added `Token` field to `PreNodeAST` for enhanced **preprocessor error diagnostics**.
* Preprocessor order now applies **incrementer after macro substitution**, improving **sksp compatibility**.
* Switched to `std::locale::classic` to avoid potential **Windows runtime errors**.
* Changed return type of **PreNode visitor methods** for better type safety.
* Improved **ArrayInitialization** by restoring important logic.
* Adjusted **visiting order** in `NodeSingleDeclaration` to better catch edge cases like self assignments.
* Modified `PathHandler` behavior to support more robust import resolution.

### Fixed

* Fixed **segfault** when using `return` with pointers.
* Parsing of **empty `struct` and `family` blocks** is now handled correctly.
* Corrected **literate macro** parsing issue where ranges could not be interpreted.
* Prevented crash in **nested iterate\_macro calls** (Nordic spheres bug).
* Rollback of error message normalization to preserve correct formatting for **dotted variable names**.
* Improved **undeclared variable error reporting**, including **suggestions**.
* Improved **tokenizer error handling** for invalid characters.
* Fixed issue in `BuiltinsProcessor` where **variables and constants were swapped**.
* Improved **Preprocessor error handling** for more robust diagnostics.
* Fixed import guard logic for better modularity.
* Added check for dot in method definitions to catch malformed syntax.
