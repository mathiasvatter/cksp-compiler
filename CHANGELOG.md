# Changelog

## [0.0.9-alpha.1] – 2026-04-13

>This release makes working with arrays and ndarrays more predictable, especially when declaring them without explicit sizes or copying them on declaration. It also improves how `exit` behaves inside user-defined functions and return functions, adds better optimization of `foreach`loops, resulting smaller generated KSP code.  
>In addition, there were improvements to error reporting in common failure cases such as faulty access chains or missing function definitions. A new unsigned right shift operator `>>>` has also been added for logical bit operations.


### Added

- Added **unsigned right shift operator** `>>>` for logical shifts, similar to Java and JavaScript [#91](https://github.com/mathiasvatter/cksp-compiler/issues/91)
    ```cksp
    on init
        declare x := -1
        declare y := x >>> 1
        message(y)
    end on
    ```

### Improved

- Improved handling of **arrays and ndarrays**
  - multidimensional arrays can now infer their dimensions from initializer expressions: `declare matrix: int[][] := [[1,2,3], [4,5,6]]`
  - `ndarray` parameters in functions are handled more robustly, even when not explicitly annotated [#23](https://github.com/mathiasvatter/cksp-compiler/issues/23)

- Improved **error reporting**
  - faulty access chains now emit `UndeclaredVariable` and suggestions for similar, already declared variables in more cases instead of less precise failures [#32](https://github.com/mathiasvatter/cksp-compiler/issues/32)
  - the **missing function definition** error is now reported more clearly

- Improved **control-flow lowering** and **code-size optimizations**
  - if a return function is not inlined, CKSP now uses native KSP `exit` instead of the previous `while + break` lowering, reducing generated code size [#29](https://github.com/mathiasvatter/cksp-compiler/issues/29)
  - `exit` inside user-defined functions is now handled more safely, preventing incorrect termination of outer callbacks in some cases when functions are inlined [#20](https://github.com/mathiasvatter/cksp-compiler/issues/20)
  - `foreach` loop lowering now enables better optimization opportunities and can reduce the size of generated code [#30](https://github.com/mathiasvatter/cksp-compiler/issues/30)

    ```cksp
    for i in range(0, 10)
        message(i)
    end for
    ```

- Improved **compile-time performance**
  - `RelinkGlobalScope` compiler pass is now more efficient
  - several unnecessary compiler passes were removed
  - handling of raw arrays during renaming in `UniqueParameterNamesProvider` and `VariableReuse` was improved

### Fixed

- Fixed issues related to **array declarations and copying**
  - array variables in structs are now validated more safely, preventing cases that could previously fail silently or lead to later crashes [#38](https://github.com/mathiasvatter/cksp-compiler/issues/38)
  - arrays and ndarrays declared without explicit size can now correctly infer their dimensions from initializer lists, array references, or ndarray references [#38](https://github.com/mathiasvatter/cksp-compiler/issues/38)
  - copying an **array on declaration** now works correctly [#4](https://github.com/mathiasvatter/cksp-compiler/issues/4)
  - copying an **ndarray on declaration** now works correctly

    ```cksp
    declare arr: int[] := [1,2,3,4,5]
    declare arr2: int[] := arr
    ```

- Fixed issues related to **local arrays and loop variables**
  - local function arrays whose size depends on parameters or other local arrays are now handled correctly [#5](https://github.com/mathiasvatter/cksp-compiler/issues/5)
    ```cksp
    function foo(array1: int[])
        declare tmp[num_elements(array1)]: int[]
        ...
    end function
    ```
  - redeclaring key or value variables inside a `foreach` loop body now correctly throws a redeclaration error
    ```cksp
    for i in range(10)
        message(i)
        declare i
    end for
    ```

- Fixed namespace prefixes being incorrectly added to **members inside access chains** [#21](https://github.com/mathiasvatter/cksp-compiler/issues/21)
- Fixed an issue where the **type identifier of return variables** could remain empty under certain conditions [#95](https://github.com/mathiasvatter/cksp-compiler/issues/95)
- Fixed a misleading syntax error when using an **initializer list directly in a `for` loop** [#94](https://github.com/mathiasvatter/cksp-compiler/issues/94)
    ```cksp
    for i in [1,2,3,5,6]
        message(i)
    end for
    ```