# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Add automatic creation of GitHub issues via URL.

### Updated
- Update function substitution and executables.
- Update v0.0.5 to address specific bug fixes and enhancements.
- Rename token enums FLOAT and INT for Windows compatibility.

### Fixed
- Fix incrementer issue and ASTGenerator indentation issue.
- Fix const block issues and improve constant folding of neutral values in additions.
- Fix issue #6 and local variable scope issue.
- Fix macro headers with text replacement attempts.
- Address errors identified during runtime such as the "time textures" error and the "#sliderName#" error.

## [0.0.5] - 2024-XX-XX
### Added
- Upload Windows v0.0.5.
- Add special array initializer and indentation in ASTGenerator.
- Add macOS ARM CKSP v0.0.5.
- Add fixes to v0.0.5 including moving function inlining into a new class.

### Updated
- Address various issues found in initial release as feedback and bugs were reported.

### Fixed
- Fix multiple code generation and formatting issues.
