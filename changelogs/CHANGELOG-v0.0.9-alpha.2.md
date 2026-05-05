# Changelog

## [0.0.9-alpha.2] - 2026-05-05

### Added
- Added support for **overriding functions and methods** in namespaces and structs.
- Added compiler warning for **`exit` commands used inside function definitions**, notifying that this will always only exit the called function. The compiler will suggest using `return` statements in functions.
- Added compiler warning when **string literals exceed Kontakt’s 320-character string length limit** ([#101](https://github.com/mathiasvatter/cksp-compiler/issues/101)).
- Added a **`Lexer lines processed` report** to stdout to make compiler processing output more transparent.
- Added optional optimization pass to **reduce declaration statements** by transforming scalar variables into array references where possible.
- Added parallelization of some optimization passes, speeding up compile times slightly.

### Fixed
- Fixed [#104](https://github.com/mathiasvatter/cksp-compiler/issues/104), where using **local constants in function definitions** could incorrectly produce a compiler error.
- Fixed [#59](https://github.com/mathiasvatter/cksp-compiler/issues/59) by reporting a compile error when **local arrays are used with save/load array built-in commands**.
- Fixed [#98](https://github.com/mathiasvatter/cksp-compiler/issues/98), where using **`exit` commands inside function definitions** could cause infinite loops in some cases.
- Fixed [#102](https://github.com/mathiasvatter/cksp-compiler/issues/102), where **line continuation syntax** (`...`) directly after a keyword, such as `and...`, could incorrectly produce a token error.
- Fixed potential **segmentation faults during parallel compiler traversals**.
