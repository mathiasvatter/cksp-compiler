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

The **CKSP Compiler** is a compiler/transpiler for the CKSP language, an extension of KSP (Kontakt Script Processor) for **Native Instruments Kontakt**. CKSP adds new features such as local variables, functions, recursive data structures, and a new type system, enabling the compilation of both the CKSP dialect and the vanilla KSP dialect. 
We describe the most significant improvements over KSP in our GPCE'25 paper [Retrofitting a Virtual Instrument DSL with Programming Abstractions](https://dl.acm.org/doi/10.1145/3742876.3742878), published in the ACM Digital Library.
To learn more about the CKSP dialect, visit the [online documentation](https://mathiasvatter.github.io/cksp-compiler/).

## Features

The following features are not comprehensive when compared to vanilla KSP syntax. For a complete list and explanation of the features, please refer to the [Syntax](https://mathiasvatter.github.io/cksp-compiler/2.%20The%20CKSP%20Syntax.html) chapter of the online documentation.
- **Syntax support for CKSP**: Enhancements to KSP, including functions with (multiple) return statements, local variables, for-loops, for-each loops, recursive data structures with pseudo-dynamic allocation, namespaces, etc.
- **TypeScript-like type annotations** for improved type safety of, for example, function parameters.
- **Support for vanilla-KSP**: Compiles existing `.ksp` or `.txt` files in the vanilla KSP dialect.
- **Flexible type conversions** and type checking. Adding type annotations is optional.
- **Preprocessor functions** are adjusted for simplified migration from the Sublime KSP syntax. Implemented features include: macros, defines, line incrementors, iterate_macro, literate_macro.

## Installation

Refer to the [Installation](https://mathiasvatter.github.io/cksp-compiler/5.%20Installation.html) chapter for detailed installation instructions.
With the [CKSP Tools](https://mathiasvatter.github.io/cksp-compiler/6.%20VS%20Code%20Extension.html) extension for Visual Studio Code, you can compile your CKSP/KSP files directly from the editor. The extension will automatically download the latest version of the compiler executable and use it to compile your files. 
The compiler is in active development, but the latest version of the CLI executable can be downloaded from [Releases](https://github.com/mathiasvatter/cksp-compiler-issues/releases/latest).

## Contributing

Contributions to the CKSP Compiler are welcome! If you have an idea for a new feature, improvement, or bug fix, please open an issue or submit a pull request on GitHub.

## Deploying a new Release

To deploy a new release, follow these steps:
0. Make sure to be on branch `development`
1. Update the version number in `CMakeLists.txt`
2. Run `run_tests.sh --with-kontakt` to ensure all tests pass.
3. Run `./scripts/write_changelog.sh` to generate the raw changelog notes for the new version. The notes will be generated based on the commit messages since the last release and written into `CHANGELOG.md` in thte project root.
4. Formulate a proper changelog entry from the notes into `CHANGELOG.md`.
6. Commit all changes to git and push to the `development` branch.
7. Merge `development` into `main` and push. This will trigger the GitHub Actions workflow to build and deploy the new release privately. The workflow will build the releases for macos arm and x86, and windows x86, push the `CHANGELOG.md` update to the `cksp-compiler-issues` submodule (branch `cksp-releases`) and create a new release on GitHub (privately on the `cksp-compiler` repository) with the new version number and the changelog entry as release notes. A tag will be created in `cksp-compiler-issues` and `cksp-compiler`.
8. When the release is built, run `publish_latest_release.sh` to publish the release publicly on GitHub and commit the final tag and release notes to `cksp-compiler-issues`.