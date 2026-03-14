# Changelog

## \[0.0.7-alpha.8] - 2025-08-25

### Added

* Support for putting args, params, members on new lines without the need for `...`. This parsing support for **newlines** was added in:
  * Function call parameter lists
  * Macro and define calls
  * Initializer lists
  * Access chains
* Introduced **FreeVarCollector** analysis pass (Important for analysing side effects).
* Added **r-value hoisting** for pointers during self-assignment.
* Added **Range information** to the `Parser` and `PreprocessorParser`.
* Isolated new `JSONParser` and `JSONTokenizer` components.
* Tokenizing support for the **ternary operator**.

### Changed

* Moved **preprocessor logic** into `Compiler.h`.
* Improved **error reporting** in `ReturnPathValidator`.

### Fixed

* **Infinite loop** when using `continue` in `for`/`foreach` loops.
* Return statements in methods were incorrectly **lowered twice**.
* `ReturnPathValidator` now correctly warns about **incomplete return paths**.
* Improved **Dead Code Elimination** logic to avoid removing valid parameter and return value statements.
* Enhanced **constructor/destructor allocation logic** for better efficiency.

