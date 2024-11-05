# Bachelor cksp TODO List

## TODOS:
- [ ] implement string representations of ndarrays and arrays (message(arr))
- [x] implement datastructure safe replace function (that updates the declaration pointer in all corresponding references)
- [x] implement reference safe replace function (that updates the set in the corresponding datastructure)
- [x] implement new function type to introduce higher-order functions to cksp
- [ ] add multiple return types to function type
- [ ] rewrite algorithm description of global scope without parameter promotion and add better examples
- [ ] check functionality of struct declaration and usage of methods more thoroughly
- [x] implement initializer list so that unnamed arrays can be passed to functions -> implemented as separate function inlining
- [x] add removal of throwaway variables to dead code elimination pass
- [x] add *.txt to valid file types
- [ ] add indexes to functions with multiple return values
- [x] implement parsing of ksp builtin functions as operators e.g. sh_left, sh_right, pow
- [ ] when const like array.SIZE are declared locally, they go into global scope where there might be name clashes ->
desugar .SIZE constants to num_elements nodes in ASTDesugaring
- [ ] add working functions with default parameters and add them with all possible mutations into the function lookup table
- [ ] add thread-safety features via dimension inflation (?)
- [ ] add thread-safety warning to constructors? try to ensure thread-safety when using datastructures
- [ ] implement tail call recursion optimization
- [ ] implement missing reference counting assignments 
- [ ] rework param promotion algorithm to promote local vars directly to global vars under specific circumstances (when function is called multiple times)
