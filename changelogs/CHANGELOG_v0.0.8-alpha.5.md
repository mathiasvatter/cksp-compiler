# Changelog

## [v0.0.8-alpha.5] - 2026-01-13

### Added
* Added **new built-in constants** introduced with **Kontakt 8.3**.  
* Added **error reporting** when a **constant is declared but not initialized** ([#57](https://github.com/mathiasvatter/cksp-compiler-issues/issues/57)).  
* Added support to **declare arrays as variables without using brackets** ([#60](https://github.com/mathiasvatter/cksp-compiler-issues/issues/60)).  
* Added a **Changelog** to track compiler changes across releases.

### Changed
* Updated **error message** when the value passed to `max_callback_depth` exceeds **1,000,000** to be clearer and more actionable.  

### Fixed
* Fixed [#66](https://github.com/mathiasvatter/cksp-compiler-issues/issues/66): incorrect lowering of **f-strings using single quotes**.  
* Fixed [#63](https://github.com/mathiasvatter/cksp-compiler-issues/issues/63): **optimization pass** no longer removes assignments that are still used inside functions.  
* Fixed [#58](https://github.com/mathiasvatter/cksp-compiler-issues/issues/58): declaring **multidimensional arrays with empty brackets** no longer causes a type error.  
* Fixed [#14](https://github.com/mathiasvatter/cksp-compiler-issues/issues/14): **`.txt` files** are no longer flagged as unsupported file types.