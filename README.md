# CKSP Compiler

```
      _              
  ___| | _____ _ __  
 / __| |/ / __| '_ \ 
| (__|   <\__ \ |_) |
 \___|_|\_\___/ .__/ 
              |_|    
```

## Overview

The **CKSP Compiler** is a compiler for the CKSP language, an extension of KSP (Kontakt Script Processor) for **Native Instruments Kontakt**. CKSP adds new features such as local variables, functions, recursive data structures, and a new type system, enabling the compilation of both the CKSP dialect and the vanilla KSP dialect. To learn more about the CKSP dialect, visit the [online documentation](https://mathiasvatter.github.io/cksp-compiler/).

## Features

The following features are not comprehensive when compared to vanilla KSP syntax. For a complete list and explanation of the features, please refer to the [Syntax](https://mathiasvatter.github.io/cksp-compiler/2.%20The%20CKSP%20Syntax.html) chapter of the online documentation.
- **Syntax support for CKSP**: Enhancements to KSP, including functions with multiple return values, local variables, for-loops, for-each loops, recursive data structures, pseudo-dynamic allocation of data structures, etc.
- **TypeScript-like type annotations** for improved type safety in, for example, function headers.
- **Support for vanilla-KSP**: Compiles existing `.ksp` or `.txt` files in the vanilla KSP dialect.
- **Flexible type conversions** and type checking. Adding type annotations is optional.
- **Preprocessor functions** are adjusted for simplified migration to the Sublime KSP syntax. Implemented features include: macros, defines, line incrementors, iterate_macro, literate_macro.

## Installation

The compiler is currently in active development, but the latest version of the CLI executable can be downloaded from [Releases](https://github.com/mathiasvatter/cksp-compiler-issues/releases/latest). To utilize existing syntax highlighting from the SublimeKSP plugin, the executable can also be used as a Sublime Build System. A Visual Studio Code language extension is also under active development.

## Deploying a new Release

To deploy a new release, follow these steps:
0. Make sure to be on branch `development`
1. Update the version number in `CMakeLists.txt`
2. Run `run_tests.sh --with-kontakt` to ensure all tests pass.
3. Run `./get_changelog.sh` to generate the changelog notes for the new version. This will be pushed to the git submodule `cksp-compiler-issues`.
4. Formulate a proper changelog entry from the notes into `CHANGELOG.md` and `cksp-compiler-issues/changelogs/CHANGELOG_vX.Y.Z.md`, replacing `X.Y.Z` with the new version number.
6. Commit all changes to git and push to the `development` branch.
7. Merge `development` into `main` and push. This will trigger the GitHub Actions workflow to build and deploy the new release privately.
8. When the release is built, run `publish_latest_release.sh` to publish the release publicly on GitHub and commit the final tag and release notes to `cksp-compiler-issues`.