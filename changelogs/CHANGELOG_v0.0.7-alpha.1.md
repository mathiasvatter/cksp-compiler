# Changelog  

## \[0.0.7-alpha.1] – 2024-12-06

### Added  
- **New AST Nodes and Features**: Introduced `NodeFunctionVar` and `NodeFunctionVarRef` nodes, enabling support for higher-order functions and more dynamic programming paradigms.  
- **Advanced Memory Management**: Implemented advanced reference counting and destructor functions, including recursive structure analysis and enhanced deletion algorithms.  
- **Wildcard Syntax and Search Features**: Added support for wildcard syntax in function arguments and improved array search logic with `NodeSearch`.  
- **Inflation Logic**: Integrated dimension inflation for arrays, enhancing the flexibility of `NodeDataStruct` and related AST transformations.  
- **Additional Desugaring Features**: Implemented classes like `DesugarParamList` and `DesugarFunctionCall`, providing better handling for complex parameter lists and multiple return values.  
- **Enhanced Documentation**: Updated and expanded documentation, including improved reference counting guides and AST explanations.  

### Changed  
- **Global Scope and Function Handling**: Restructured global scope management and function inlining for better performance and modularity.  
- **Lowering Logic**: Refined transformation logic for array sizes, default parameters, and struct member access to improve compilation reliability.  
- **Error Handling**: Fixed issues in type inference and expression evaluation, ensuring robust compilation for complex function headers and nested expressions.  

### Fixed  
- **Crash Issues**: Resolved a crash caused by incorrect `node_self` pointer references and improved stability for recursive function calls.  
- **Compilation Bugs**: Fixed several bugs, including incorrect array size declarations and nested function usage, ensuring that test cases like `main.cksp` compile correctly.  
- **Desugaring Problems**: Addressed problems in handling `for-each` statements, struct member access, and reference substitution.  

### Removed  
- **Legacy Code**: Cleaned up outdated and redundant features, such as `ksp-compiler.wiki` submodules and unnecessary constant declarations.  
