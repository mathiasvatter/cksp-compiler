# Changelog

## [0.0.9-alpha.4] - 2026-06-02

>This release further improves efficiency and reduces the generated KSP code size.

In a comparison of several of our production scripts compiled with `cksp` and `SublimeKSP`, `cksp 0.0.9-alpha.4` produced up to **47%** smaller generated code. The largest reductions appear in products that mainly use parameterized functions, such as *Time Textures Expanded* and *The Score*, because `cksp` can transform those calls to native KSP functions instead of inlining them like `SublimeKSP` does. Older, macro-heavy projects such as *Horizon Leads*, and *The Orchestra Complete 4* show smaller gains, which is expected because there is less function-call structure for the compiler to preserve.
![Generated KSP code size comparison between source files, SublimeKSP output, and cksp 0.0.9 output](https://mathiasvatter.github.io/cksp-assets/assets/code_size_chart_white.png)


### Added

- Added missing built-in **NI constants for UI widget types** ([#106](<https://github.com/mathiasvatter/cksp-compiler/issues/106>)).

### Changed

- Improved **function inlining logic** so cksp now always attempts to call or transform function parameters, even when a function appears to be called only once.

### Fixed

- Fixed an issue in **DimensionExpansion** where the right-hand side of a **thread-unsafe declaration** could be visited twice, causing dimensions to be expanded twice and leading to later compiler errors from mismatching parent types.
- Fixed **blank-line measurement** in the lexer.
- Fixed an issue where the parser could throw a **compile error for empty KSP programs**.