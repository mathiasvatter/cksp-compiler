# Changelog

## [0.1.0-alpha.2]

>[!IMPORTANT]
>This alpha focuses on **Windows support** for both the compiler and the language server, adds an `expose_controls` builtin for kscript interop, and introduces new diagnostics for non-ASCII string values and persistent pointer variables. 

### Added
- Added a warning when a **string value contains non-ASCII characters** ([#116](<https://github.com/mathiasvatter/cksp-compiler/issues/116>)).
- Added a warning when a variable of **pointer type is made persistent**.

### Fixed
- Fixed Windows support for **LSP message framing** and **crash logs**, so the language server and crash reporter work on Windows.
- Added missing `expose_controls` builtin command to support **kscript** integration.
- Fixed constructors called in `global_declarations` being **executed instead of inlined** ([#118](<https://github.com/mathiasvatter/cksp-compiler/issues/118>)).
- Fixed **stale LSP diagnostics** that persisted after files were removed from the workspace ([#31](<https://github.com/mathiasvatter/cksp-tools-issues/issues/31>)).
- Fixed **Windows file URIs** not round-tripping correctly in the language server.

Please keep reporting regressions, confusing diagnostics, and editor-integration issues — especially on Windows — on GitHub.

Cheers,
Mathias
