# Changelog

## [0.0.9-alpha.7]

>[!IMPORTANT]
>This alpha release fixes bugs introduced in earlier versions that could cause performance issues in Kontakt.

### Changed

- Improved **variable lifetime analysis** by taking **function parameters** into account improving efficiency of variable reuse.

### Fixed

- Fixed regression issue [#110](https://github.com/mathiasvatter/cksp-compiler/issues/110), where function parameters were incorrectly marked as **thread-safe by default**. This was introduced by 0.0.9-alpha.4.
- Fixed [#112](https://github.com/mathiasvatter/cksp-compiler/issues/112), where functions with **thread-unsafe parameters** were transformed instead of being inlined.
- Fixed [#111](https://github.com/mathiasvatter/cksp-compiler/issues/111), where **temporary objects created with `new`** did not work correctly in expressions and access chains.
- Fixed an issue where temporary objects could cause type errors in some cases.
- Fixed an issue where **parameter promotion** did not work for function calls in the global declarations block.