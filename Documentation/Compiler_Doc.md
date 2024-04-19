
## AST Reihenfolge:

1. Desugar
   - foreach loops -> for loops
   - for loops -> while loops
   - family block -> variable declarations
   - const block -> variable declarations
   - mark scopes?

   - find variables that are arrays by name -> get sizes
2. Definition Checker
   - user defined Variable/Array scope checker -> definition pointer
   - builtin constant checker -> definition pointer
   - function call checker -> definition pointer
   - builtin function checker -> no definition pointer

3. Type Inferencer
   - declaration statements types
   - assignment statements types
   - builtin function types

4. Type Checker
   - declaration statements types
   - assignment statements types
   - builtin function types
   - types in ui_control callbacks

5. Datastructure Lowering
    - multidimensional arrays -> arrays
    - lists -> arrays

6. Optimization
    - kontakt specific optimizations (array declaration etc)
    - constant folding
    - constant propagation
    - inline functions
    - global scope -> local scope
    - dead code elimination
    - (loop unrolling)

7. Code Generation