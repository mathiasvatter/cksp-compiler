# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Add windows executable with NKS 2 stuff.
- Add NKS 2 built-in functions and engine constants.
- Add cksp ARM and x86 executables for version 0.0.3.
- Rename NodeDeadEnd to NodeDeadCode to better reflect its functionality.
- Do some CompilerError replacements to enhance error reporting.
- Add work-in-progress (wip) line markings to CompileError class.
- Add `m_pos_line` to Token struct and Tokenizer class to improve debugging and error tracking.
- Add CommandLineOptions and Pragma Preprocessor to main.cpp to enhance command line usability.
- Add FileHandler class to manage file interactions.
- Add NodeNDArray to manage multidimensional array data.
- Implement working range-based for-loops and update scope logic.

### Updated
- Update several executables across various platforms to ensure consistency and integration of new features.
- Update submodule wiki to the latest documentation.
- Update CompilerError class with new error codes and color schemes to improve readability.
- Remove outdated submodules and scripts to streamline project structure.

### Fixed
- Fix segmentation fault crash when removing unused ui_control variables.
- Fix multiple text replacement issues in macros to prevent runtime errors.
- Fix Incrementer issue where it would start at +1 and not reset, affecting loops and conditional statements.
- Fix SET_CONDITION recognition to correctly parse and execute condition setting in scripts.

### Removed
- Remove x86 executable due to incorrect output path setup, planning for a corrected re-release.

## [0.0.3] - 2024-XX-XX
### Added
- Release of version 0.0.3 with comprehensive updates including new executables for macOS, Windows, and ARM architecture.
- Add extensive updates to the PreAST processing stage, including splitting up defines and macros into separate classes for better management and incremental adjustments.

### Updated
- Update readme to reflect new features and setups introduced in version 0.0.3.
- Update NodeRangedForStatement and add NodeReturnStatement to enhance loop and return functionalities.

### Fixed
- Fix issues related to the ksp preprocessor constants, improving the accuracy and functionality of script processing.
- Resolve Macro search issues when no arguments are in call, ensuring macros are correctly parsed and executed.

