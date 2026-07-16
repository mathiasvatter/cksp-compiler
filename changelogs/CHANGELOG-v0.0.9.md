# Changelog

## [0.0.9] - 2026-07-16

>[!NOTE]
>This stable release consolidates the complete `0.0.9-alpha` series. It focuses on more predictable array and ndarray handling, better namespace and struct support, smaller generated KSP output, stricter type validation, and a set of compiler stability fixes found during the alpha cycle.

### Added

- Added support for the unsigned right shift operator `>>>` for logical bit shifts ([#91](https://github.com/mathiasvatter/cksp-compiler/issues/91)).
- Added support for overriding functions and methods in namespaces and structs.
- Added support for namespaces as statements in the global scope.
- Added support for struct definitions nested inside namespaces.
- Added optimization for `const array` values, allowing eligible constant array references to be substituted with their initial value at the referenced index.
- Added lifetime and thread-safety analysis passes to make variable reuse more precise and reduce unnecessary dimension expansions.
- Added early removal of unused local variables to reduce generated KSP code size.
- Added an optional scalar-variable-to-array optimization that can reduce declaration statements in `on init`.
- Added compiler warnings for `exit` commands inside function definitions, with guidance to use `return` instead.
- Added compiler warnings for string literals exceeding Kontakt's 320-character string length limit ([#101](https://github.com/mathiasvatter/cksp-compiler/issues/101)).
- Added missing built-in NI constants for UI widget types ([#106](https://github.com/mathiasvatter/cksp-compiler/issues/106)).
- Added a lexer line-processing report to make compiler progress output more transparent.
- Added clearer diagnostics for unsupported tokens after `<END_USE_CODE>` ([#90](https://github.com/mathiasvatter/cksp-compiler/issues/90)).

### Changed

- Improved array and ndarray inference so dimensions can be inferred from initializer lists, array references, and ndarray references.
- Improved copying arrays and ndarrays directly on declaration ([#4](https://github.com/mathiasvatter/cksp-compiler/issues/4)).
- Improved handling of ndarray parameters in functions, including cases without explicit ndarray annotations ([#23](https://github.com/mathiasvatter/cksp-compiler/issues/23)).
- Improved lowering of `exit` in user-defined functions so inlined functions no longer terminate the outer callback unexpectedly ([#20](https://github.com/mathiasvatter/cksp-compiler/issues/20)).
- Improved return-function lowering so non-inlined return functions can use native KSP `exit` instead of a `while + break` transformation ([#29](https://github.com/mathiasvatter/cksp-compiler/issues/29)).
- Improved `foreach` lowering and iterator handling to create better optimization opportunities and smaller generated code ([#30](https://github.com/mathiasvatter/cksp-compiler/issues/30)).
- Improved function inlining and parameter transformation so cksp attempts native calls or parameter transforms more consistently.
- Improved variable lifetime analysis by considering function parameters, which improves variable reuse decisions.
- Improved type checking for numeric types. Code that previously mixed incompatible `int` and `real` values in function calls or expressions may now require explicit fixes ([#26](https://github.com/mathiasvatter/cksp-compiler/issues/26), [#14](https://github.com/mathiasvatter/cksp-compiler/issues/14), [#12](https://github.com/mathiasvatter/cksp-compiler/issues/12), [#16](https://github.com/mathiasvatter/cksp-compiler/issues/16)).
- Improved type mismatch, undeclared variable, faulty access-chain, missing function definition, and initializer-list error messages ([#16](https://github.com/mathiasvatter/cksp-compiler/issues/16), [#32](https://github.com/mathiasvatter/cksp-compiler/issues/32), [#35](https://github.com/mathiasvatter/cksp-compiler/issues/35), [#94](https://github.com/mathiasvatter/cksp-compiler/issues/94)).
- Improved compile-time performance by making several compiler passes more efficient and removing unnecessary passes.

### Fixed

- Fixed local arrays whose size depends on function parameters or other local arrays ([#5](https://github.com/mathiasvatter/cksp-compiler/issues/5)).
- Fixed local constants in function definitions incorrectly producing compiler errors ([#104](https://github.com/mathiasvatter/cksp-compiler/issues/104)).
- Fixed local constants shadowing global constants incorrectly causing `RedeclaredVariable` errors.
- Fixed namespace prefixes being incorrectly added to members inside access chains ([#21](https://github.com/mathiasvatter/cksp-compiler/issues/21)).
- Fixed case-collision resolution for references to data structures.
- Fixed function-argument detection so assignments are no longer incorrectly removed in affected cases ([#114](https://github.com/mathiasvatter/cksp-compiler/issues/114)).
- Fixed heap-size calculation when a struct is used in a static environment and contains array fields.
- Fixed `const array` references inside initializer lists causing invalid KSP output ([#107](https://github.com/mathiasvatter/cksp-compiler/issues/107)).
- Fixed type checking for composite vs. basic types in unary and binary expressions, and enforced integer array indexes ([#9](https://github.com/mathiasvatter/cksp-compiler/issues/9)).
- Fixed boolean comparison lowering that could previously generate Kontakt errors ([#24](https://github.com/mathiasvatter/cksp-compiler/issues/24)).
- Fixed printing ndarray references with wildcards ([#74](https://github.com/mathiasvatter/cksp-compiler/issues/74)).
- Fixed thread-unsafe local arrays in `foreach` loops causing compile errors from non-substituted wildcards ([#109](https://github.com/mathiasvatter/cksp-compiler/issues/109)).
- Fixed regressions around thread-unsafe function parameters that could cause Kontakt performance issues ([#110](https://github.com/mathiasvatter/cksp-compiler/issues/110), [#112](https://github.com/mathiasvatter/cksp-compiler/issues/112)).
- Fixed temporary objects created with `new` in expressions and access chains ([#111](https://github.com/mathiasvatter/cksp-compiler/issues/111)).
- Fixed parameter promotion for function calls in the global declarations block.
- Fixed use of local arrays with save/load array built-in commands by reporting a compile error ([#59](https://github.com/mathiasvatter/cksp-compiler/issues/59)).
- Fixed infinite-loop cases caused by `exit` inside function definitions ([#98](https://github.com/mathiasvatter/cksp-compiler/issues/98)).
- Fixed tokenization of line continuation syntax directly after keywords such as `and...` ([#102](https://github.com/mathiasvatter/cksp-compiler/issues/102)).
- Fixed built-in KSP preprocessor directives being removed from compiled output ([#105](https://github.com/mathiasvatter/cksp-compiler/issues/105)).
- Fixed missing output for built-in `set_condition` commands.
- Fixed incorrect lowering of `delete` for arrays of objects.
- Fixed `__decr__` generation and reference-count handling for ndarray and temporary-object cases.
- Fixed parser behavior for empty KSP programs.
- Fixed blank-line measurement in the lexer.
- Fixed several segmentation-fault and dangling-pointer cases in parallel traversals, struct inlining, and `ConstExprPropagation` with `optimize("aggressive")`.
- Fixed internal helper naming around `clip` to avoid possible `RedeclaredFunction` conflicts with user functions.

### Migration Notes

- Structs and namespaces are now processed as global statements in declaration order. If a struct method depends on a global variable, declare that variable before the affected struct or namespace. Free function definitions can still access variables declared in `on init`.
- Numeric type checking is stricter. Existing code that relies on implicit `int`/`real` mixing may need explicit type adjustments.
- `exit` inside a user-defined function exits only that function. Prefer `return` in function bodies when that is the intended control flow.
