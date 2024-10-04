# CKSP Compiler

## Overview

The **CKSP Compiler** is a compiler for the CKSP language, an extension of KSP (Kontakt Script Processor) for **Native Instruments Kontakt**. CKSP adds new features such as local variables, functions, recursive data structures, and a new type system, enabling the compilation of both the CKSP dialect and the vanilla KSP dialect.

## Features

The following features are not comprehensive when compared to vanilla KSP syntax. For a complete list and explanation of the features, please refer to the [wiki](https://github.com/mathiasvatter/ksp-compiler/wiki/Features).
- **Syntax support for CKSP**: Enhancements to KSP, including functions with multiple return values, local variables, for-loops, for-each loops, recursive data structures, pseudo-dynamic allocation of data structures, etc.
- **TypeScript-like type annotations** for improved type safety in, for example, function headers.
- **Support for vanilla-KSP**: Compiles existing `.ksp` or `.txt` files in the vanilla KSP dialect.
- **Flexible type conversions** and type checking. Adding type annotations is optional.
- **Preprocessor functions** are adjusted for simplified migration to the Sublime KSP syntax. Implemented features include: macros, defines, line incrementors, iterate_macro, literate_macro.

## Installation

The compiler is currently in active development, but the latest version of the CLI executable can be downloaded from [Releases](https://github.com/mathiasvatter/ksp-compiler/releases). To utilize existing syntax highlighting from the SublimeKSP plugin, the executable can also be used as a Sublime Build System. A Visual Studio Code language extension is also under active development.
