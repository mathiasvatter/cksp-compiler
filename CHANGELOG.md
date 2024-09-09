# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Add missing nks command.
- Add documentation.
- Add new classes to tackle 'memory exhausted' error due to bison parsing stack overflow.
- Add max_block_line info.
- Add windows executable.
- Add several optimization classes: ConstExprValidator, ConstExprPropagation, DeadCodeElimination; Add SIZE constant to array lowering.
- Add NodeDelete and parse_node_delete; Update lowering example.
- Add performance improvements and fixes for the-score.
- Add __repr__ method replacement and AccessChain Lowering wip.
- Add lowering to SingleReturn Node.
- Add new node for method chaining; Update Tokenizer and Parser accordingly.
- Add new lowering examples for return functions in conditional statements.
- Add NodeNil to ast; Add parse_nil().
- Add pandoc bash script.

### Updated
- Update arm64 executable for cksp0.0.6.
- Update includes so that it works on windows; Add windows v0.0.6 executable.
- Update cksp executable.
- Update grammar and add CMakeLists.txt.
- Update bachelor.md.
- Update TypeCasting; Update LambdaLifting to not replace function parameters; Update BuildDataStructures to replace incorrectly detected Variable parameters as Arrays.
- Update various small issues.

### Fixed
- Fix killing last assignment when reference is also r_value.
- Fix Optimization of double assignments.
- Fix move_on_init_callback() method; Fix other issues.
- Fix killing succeeding assignment issue by checking if var is also in r_value.
- Fix get_ui_id issues -> move to syntax checks; Fix memory exhausted error; Todo DeadCodeElimination.
- Fix declaration and assignment parsing when parsing array initializer list (hopefully).
- Fix struct desugaring.
- Fix parameter promotion issue when nested functions; Fix renaming issue when using multiple structs.
- Fix ndarray copy and init functions; Update lowering of structs.
- Fix ReturnFunctionRewriting classes for legato ksp.

## [0.0.6] - 2024-XX-XX
### Added
- Initial release of cksp v0.0.6 with basic functionality and structure.
