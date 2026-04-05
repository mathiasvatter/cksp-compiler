# CKSP Compiler

[![Release](https://img.shields.io/github/v/release/mathiasvatter/cksp-compiler)](https://github.com/mathiasvatter/cksp-compiler/releases)
[![Docs](https://img.shields.io/badge/docs-online-brightgreen)](https://mathiasvatter.github.io/cksp-compiler/)
[![Build](https://img.shields.io/github/actions/workflow/status/mathiasvatter/cksp-compiler/build-and-deploy.yml)](https://github.com/mathiasvatter/cksp-compiler/actions)
[![License](https://img.shields.io/github/license/mathiasvatter/cksp-compiler)](LICENSE)
[![Downloads](https://img.shields.io/github/downloads/mathiasvatter/cksp-compiler/total)](https://github.com/mathiasvatter/cksp-compiler/releases)

```
      _              
  ___| | _____ _ __  
 / __| |/ / __| '_ \ 
| (__|   <\__ \ |_) |
 \___|_|\_\___/ .__/ 
              |_|    
```

## Overview

The **CKSP Compiler** is a preprocessing compiler that extends the Kontakt Script Processor (KSP), the scripting language used in Native Instruments **Kontakt**.
Despite its industry adoption, vanilla KSP lacks modern programming abstractions and has remained largely unchanged.

CKSP retrofits these missing abstractions by introducing a higher-level language layer that compiles down to vanilla KSP. This enables developers to write structured, maintainable code while preserving full compatibility with the existing Kontakt runtime.

We describe the most significant improvements over KSP in our GPCE'25 paper [Retrofitting a Virtual Instrument DSL with Programming Abstractions](https://dl.acm.org/doi/10.1145/3742876.3742878), published in the ACM Digital Library.

Learn more in the [documentation](https://mathiasvatter.github.io/cksp-compiler/).


## Why CKSP?

KSP was not designed for large-scale software development. As Kontakt projects grow, scripts become increasingly difficult to maintain due to (not exhaustive):

- Global variable management
- Lack of modularity
- Missing abstraction mechanisms

CKSP addresses these limitations by adding features like:

- **Lexical scoping**: introduces local variables and replaces global-only variable handling
- **Functions with parameters and return values**: enable modular code
- **Recursive data structures**: allow expressive data modeling
- **Optional typing**: TypScript-like type annotations improve reliability without sacrificing flexibility
- **Support for vanilla-KSP**: Compiles existing `.ksp` or `.txt` files in the vanilla KSP dialect.
- **Preprocessor functions** are adjusted for simplified migration from the Sublime KSP syntax. Implemented features include: macros, defines, line incrementors, iterate_macro, literate_macro.

All features are compiled to standard KSP, requiring no changes to the Kontakt runtime.
The above list is not comprehensive when compared to vanilla KSP syntax. For more completeness, please refer to the [Syntax](https://mathiasvatter.github.io/cksp-compiler/2.%20The%20CKSP%20Syntax.html) chapter of the online documentation.

## Usage

Compile a `cksp` file:
```cksp
cksp input.cksp -o output.txt
```

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