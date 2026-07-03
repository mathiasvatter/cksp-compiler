# Changelog

## [0.0.9-alpha.9]

>[!NOTE]
>This alpha release fixes a regression error from `alpha.5` and makes internal detection of **function arguments** more robust, reducing the risk of incorrect compiler optimizations in the future.

### Fixed

- Fixed [#114](https://github.com/mathiasvatter/cksp-compiler/issues/114), where cksp could fail to recognize that a reference was a **function argument**, leading to incorrectly removed variable assignments.
- Fixed incorrect **heap size calculation** when a struct was used in a **static environment** and contained **array fields**.