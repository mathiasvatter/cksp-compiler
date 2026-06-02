## [0.0.9-alpha.4] - 2026-06-02

### Added

- Added built-in **NI constants for UI widget types** ([#106](https://github.com/mathiasvatter/cksp-compiler/issues/106)).

### Changed

- Improved **function inlining logic** so cksp now always attempts to call or transform function parameters, even when a function appears to be called only once. This reduces the size of the resulting codebase by up to 46x as shown underneath.

### Fixed

- Fixed an issue in **DimensionExpansion** where the right-hand side of a **thread-unsafe declaration** could be visited twice, causing dimensions to be expanded twice and leading to later compiler errors from mismatching parent types.