
Usage: cksp [options] <input-file>

Options:
 -h, --help        Display usage information
 -o <file>         Set output file name (default: <input_dir>/out.txt)
 -v, --version     Display version number

Known Issues:
 - Incrementer cannot start in one macro definition and end outside, has to stay in one scope
 - lists can only be declared in blocks and not with <declare list ls[]> and then appended with list_add()
 - possible issues with declaring local arrays
 - possible issues with entering real numbers without .0 in real expression

Not supported (yet):
 - <import as> namespaces
 - structs
 - taskfunctions
 - concat()
 - pragma
 - iterate_post_macro(), literate_post_macro()

Preprocessor Order:
 - Tokenizer:
     - Comments are removed.
     - Line continuations <…> are joined
 - Imports are handled by recursively executing the Tokenizer
 - <USE_CODE_IF/IF_NOT> is executed
 - Preprocessor Parsing to PreAST
 - PreAST desugaring:
     - defines are substituted
     - iterate_macro and literate_macro are expanded
     - macros are substituted



Usage: cksp [options] <input-file>

Options:
 -h, --help        Display usage information
 -o <file>         Set output file name (default: <input_dir>/out.txt)
 -v, --version     Display version number

Known Issues:
 - Incrementer cannot start in one macro definition and end outside, has to stay in one scope
 - lists can only be declared in blocks and not with <declare list ls[]> and then appended with list_add()
 - possible issues with declaring local arrays
 - possible issues with entering real numbers without .0 in real expression

Not supported (yet):
 - <import as> namespaces
 - structs
 - taskfunctions
 - concat()
 - pragma
 - iterate_post_macro(), literate_post_macro()

Preprocessor Order:
 - Tokenizer:
     - Comments are removed.
     - Line continuations <…> are joined
 - Imports are handled by recursively executing the Tokenizer
 - <USE_CODE_IF/IF_NOT> is executed
 - Preprocessor Parsing to PreAST
 - PreAST desugaring:
     - defines are substituted
     - iterate_macro and literate_macro are expanded
     - macros are substituted


