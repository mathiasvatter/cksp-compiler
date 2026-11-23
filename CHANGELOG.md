# Changelog v0.1.0-alpha.8

> [!IMPORTANT]
> A small **correctness** release. Most important is a fix for a **local variable that shadows another local**: it could share its KSP variable with the outer one and overwrite it. Shadowing is now also reported as a warning. 

### Added
- Added a **warning when a local variable shadows a local** of an enclosing block. Up to the end of its block, every use of the name means the inner variable. Locals named like a global and function locals named like a local of the calling callback are not reported.

### Improved
- **Boolean literals are inferred as `bool`**, so `declare flag := true` is a `bool` (previously reported as `int`). Arithmetic and bitwise operators read a `bool` as `0` or `1`, so `flag * 128` compiles. A `bool` mixed with a `real` still needs a cast.
- **`message()` prints a boolean as `"true"` or `"false"` only when it is a direct argument** or part of a concatenation. Inside arithmetic it stays the number it stands for.
- **Arithmetic conditions** such as `if (a .and. b)` or `while (n - x)` are compared against zero, like a plain variable, instead of reaching Kontakt as invalid KSP.

### Fixed
- Fixed a **local variable that shadows an outer local** being compiled onto the same KSP variable as the outer one when the outer one reused a free variable. Assignments to the inner variable overwrote the outer one.
- Fixed [#157](https://github.com/mathiasvatter/cksp-compiler/issues/157): a **string used as a condition** compiled, and Kontakt rejected it. In `message("Test: " & 3 > 0 ? "True" : "False")`, `?:` binds weaker than `&`, so the whole concatenation was the condition. This is now an error that suggests wrapping the ternary in parentheses.

Please keep reporting regressions, confusing diagnostics, and editor-integration issues on GitHub.

Cheers,  
Mathias
