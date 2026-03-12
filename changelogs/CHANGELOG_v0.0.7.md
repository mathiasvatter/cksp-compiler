# Changelog

## \[0.0.7] – 2025-09-24

This release improves developer productivity and compiler correctness, while introducing several new expressive language features.
This is a summary of noteworthy additions and changes made since the release of version **0.0.6** on 2024-09-26.
More detailed changelogs can be found in the notes of each alpha release since then.

### Added

* **New Language Features**
	* Introduced the **`Boolean` type**, with type casting for all available types via `bool()`, `int()`, `real()`.
	* Added **short-circuiting logic** for conditions with user-defined function calls
	* Added **ternary expressions** which gets lowered into KSP-compatible `if` blocks.
	* Added **namespaces** with support for **nested namespaces** and **partially qualified prefixes**.
	* Added support for declaring **global variables** outside the `on init` callback.
	* Added support for **f-strings**.
	* Added support for **compound assignments** (e.g., `+=`, `-=`, etc.) for most operators.
	* Added new **array initializer syntax** in expressions and function arguments using `[]` instead of `()`.
	* Added support for **newlines in argument lists** and **access chains** (without requiring the line continuation token `...`).
	* Introduced **wildcard syntax** (`*`) for referencing all elements in an array dimension, applied significant changes to the lowering of `search` and `sort` functions.
	* Introduced the `#pragma pass_by` directive to toggle value/reference parameter passing.
	* Introduced **new `foreach` loop syntax** with support for **initializer lists** and `pairs()` to enumerate key-value pairs.
	* Added **project-root relative imports** via `"./"` syntax.
	* Added automatic resolving of case collisions to help translate **case-sensitive cksp** code into **case-insensitive KSP**.

* **New Function Handling**: *This addition introduces changes to function handling leading to some **v0.0.6** or **sksp** codebases requiring code adjustments.*
	* Added **Parameter stack transformation** which overhauls the internal function handling completely (eagerly transforming user-defined functions into native KSP functions), simulating **call-by-value** semantics and optimizing performance and the size of the compiled code.
	* Added `ref` keyword to force **pass-by-reference** semantics for function parameters. Also added additional warnings and guidance for UI control parameters to use `ref`, resolving prior integration issues.
	* Added support for parsing **empty return** statements, enabling early function exits without return values.
	* **Return path validation**, ensuring all code paths in functions return a value.

* **CLI and Tooling**
	* Add tooling for **automated testing** of multiple files and projects.
	* Add tooling for automatically launching **Kontakt** with compiled scripts for **script error** detection.


### Changed

* Improvements to **reference counting** by covering more edge cases and bugs related to temporary constructors and return node rewrites
* Improved **constructor/destructor allocation logic** for better efficiency.
* Improvements to error reporting by offering suggestions in undeclared variable errors.
* Improved handling of **string literals**: converts all single quotes to double quotes for compatibility since ksp seems to be only able to handle double quotes.
* cksp now correctly warns about **incomplete return paths**.

### Fixed

* **Crashes and Stability**
	* Resolved critical segfault and Windows-specific issues with initialization and the windows `locale`.
	* Fixed edge cases in parameter reuse, destructors, and memory cleanup logic.
	* Addressed tokenizer, macro parsing, and import resolution bugs.
* **Compiler Behavior**
	* Fixed multiple issues in return logic, hoisting, inlining, access chains, and thread safety.
	* Corrected array dimension bugs, nested loop behavior, and for/foreach infinite loops.
	* Solved edge cases in const declarations, struct member access, and recursive function calls.
* **Regression Fixes**
	* Fixed issue #38 (arrays being misclassified as primitive types).
	* Resolved reintroduced bug related to UI control variable type checking.
