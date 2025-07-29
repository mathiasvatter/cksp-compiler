USAGE: cksp [options] <input-file>

OPTIONS:
  -h, help 			Display usage information
  -o, output <file>		Set output file name (default: <input_dir>/out.txt)
  -v, version 			Display version number
  -O, optimize <level>		Set optimization level: none, simple, standard, aggressive
      -O0 			Equivalent to --optimize=none
      -O1 			Equivalent to --optimize=simple
      -O2 			Equivalent to --optimize=standard
      -O3 			Equivalent to --optimize=aggressive

FEATURES:
For a more extensive collection of features and an introduction into the syntax of CKSP, please refer to the online documentation:
https://mathiasvatter.github.io/cksp-compiler/

KNOWN ISSUES (in comparison to the sublime ksp fork):
 - Incrementer cannot start in one macro definition and end outside, has to stay in one scope
 - lists can only be declared in blocks and not with <declare list ls[]> and then appended with list_add()
 - <import as> namespaces
 - task functions
 - concat()
 - iterate_post_macro(), literate_post_macro()

PREPROCESSOR ORDER:
 - Tokenizer:
     - Comments are removed.
     - Line continuations <…> are joined
 - Imports are handled by recursively executing the Tokenizer
 - <USE_CODE_IF/IF_NOT> is executed
 - Preprocessor Parsing to PreAST
 - PreAST lowering:
     - defines are substituted
     - <START_INC> directive is executed
     - iterate_macro and literate_macro are expanded
     - macros are substituted


