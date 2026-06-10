# Changelog

## [0.0.9-alpha.5]

>This release fixes a number of **long-standing compiler bugs** and includes efficiency improvements to the way that cksp reuses variables.
>The type checker is now **stricter with numeric types**. Existing codebases that mix **integers** and **reals** in function calls or expressions may need a few small type conversion adjustments. These checks help catch problems earlier instead of letting them become harder-to-debug Kontakt errors later.

### Added
- Added **optimization for `const array` values**, allowing cksp to substitute constant array references with their initial value at the referenced index where possible.
    Now code like this:
    ```cksp
    declare const arr[2] := (4, 5)
    declare arr2[3] := (arr[0] * 3, arr[1] + 3, 0)
    ```
    will get optimized to this:
    ```cksp
    declare %arr2[5] := (12, 8, 0)
    ```
- Added improved **lifetime analysis** to better understand variable lifetimes and support more efficient variable reuse.
- Added a new **thread safe analysis** pass to more efficiently detect thread-unsafe variables and reduce unnecessary dimension expansions. Local variables whose lifetime has already ended before any asynchronous builtin calls are no longer treated as thread-unsafe.
- Added early removal of **unused local variables**, reducing dead storage statements and saving lines in generated KSP output.
- Added additional error information for the **`<END_USE_CODE>` directive** when unsupported tokens appear after it ([#90](https://github.com/mathiasvatter/cksp-compiler/issues/90)).

### Changed
- Made the **type checker stricter for numeric types** in function parameters. Code that previously mixed incompatible `int` and `real` parameters may now require small but necessary type fixes ([#26](https://github.com/mathiasvatter/cksp-compiler/issues/26), [#14](https://github.com/mathiasvatter/cksp-compiler/issues/14), [#12](https://github.com/mathiasvatter/cksp-compiler/issues/12), [#16](https://github.com/mathiasvatter/cksp-compiler/issues/16))

### Fixed
- Fixed [#107](https://github.com/mathiasvatter/cksp-compiler/issues/107), where **`const array` references inside initializer lists** could prevent cksp from splitting initializer lists correctly, resulting in Kontakt errors.
- Fixed [#9](https://github.com/mathiasvatter/cksp-compiler/issues/9), where the **type checker** did not correctly distinguish between composite and basic types in unary and binary expressions, and did not enforce array indexes to be of type **`Integer`**.
- Fixed [#16](https://github.com/mathiasvatter/cksp-compiler/issues/16) by improving **type mismatch error reporting**.
- Fixed [#24](https://github.com/mathiasvatter/cksp-compiler/issues/24), where cksp could generate incorrect boolean comparisons or allow comparisons that later caused Kontakt errors.
- Fixed [#74](https://github.com/mathiasvatter/cksp-compiler/issues/74), where printing **ndarray references with wildcards** could fail because of an incorrect `__repr__` parameter type.
- Fixed [#35](https://github.com/mathiasvatter/cksp-compiler/issues/35), where referencing an **undeclared variable inside an initializer list** produced a cryptic compiler error.
- Fixed [#109](https://github.com/mathiasvatter/cksp-compiler/issues/109), where using **thread-unsafe local arrays in `foreach` loops** could cause compile errors because wildcards were not substituted.
- Fixed a **dangling pointer issue** in `ConstExprPropagation` that could cause segmentation faults when **`optimize("aggressive")`** was enabled.

As always, please report any regressions or confusing compiler errors on GitHub.

Cheers,

Mathias
