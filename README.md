# CKSP Compiler

[![Release](https://img.shields.io/github/v/release/mathiasvatter/cksp-compiler)](https://github.com/mathiasvatter/cksp-compiler/releases)
[![Docs](https://img.shields.io/badge/docs-online-brightgreen)](https://mathiasvatter.github.io/cksp-compiler/)
[![Build](https://img.shields.io/github/actions/workflow/status/mathiasvatter/cksp-compiler/build-and-deploy.yml)](https://github.com/mathiasvatter/cksp-compiler/actions)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue)](LICENSE.md)
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
The compiler is in active development, but the latest version of the CLI executable can be downloaded from [Releases](https://github.com/mathiasvatter/cksp-compiler/releases/latest).

## Contributing

Contributions to the CKSP Compiler are welcome! If you have an idea for a new feature, improvement, or bug fix, please open an issue or submit a pull request on GitHub.

## Deploying a new Release

To deploy a new release, follow these steps:
1. Make sure to be on branch `development`.
2. Update the version number in `CMakeLists.txt`.
3. Run `./run_tests.sh --with-kontakt` to ensure all tests pass.
4. Run `./scripts/write_changelog.sh` to generate the raw changelog notes for the new version. The notes will be generated based on the commit messages since the last release and written into `CHANGELOG.md` in the project root.
5. Formulate a proper changelog entry from the notes into `CHANGELOG.md`.
6. Commit all changes to git and push to the `development` branch.
7. Merge `development` into `master` and push. This will trigger the GitHub Actions workflow to build macOS arm64, macOS x86_64, and Windows releases, commit the changelog snapshot to `changelogs/`, create the release tag, and publish the GitHub release in this repository.

For a local release from the current machine, run `./scripts/publish_latest_release.sh`. This builds the compiler locally, updates the current platform's binary under `_Releases/<version>`, keeps any existing binaries for other platforms in that directory, zips the full version directory, and publishes it to this repository's GitHub releases.
