#include <iostream>
#include <filesystem>

#include "Parser.h"
#include "Preprocessor/Preprocessor.h"
#include "AST/ASTVisitor.h"
#include "AST/ASTDesugar.h"
#include "AST/ASTPrinter.h"
#include "AST/ASTTypeCasting.h"
#include "Preprocessor/PreprocessorBuiltins.h"
#include "AST/ASTTypeChecking.h"
#include "AST/ASTVariables.h"
#include "Generator/ASTGenerator.h"

int main(int argc, char* argv[]) {

    Token tok = Token(INT, "0", -1, "");
    auto node_declaration = std::make_unique<NodeSingleDeclareStatement>(tok);
    auto node_variable = std::make_unique<NodeVariable>(false, "var", Mutable, tok);
    auto node_int = std::make_unique<NodeInt>(3, tok);
    auto node_int_replacement = std::make_unique<NodeInt>(15, tok);
    node_declaration->to_be_declared = std::move(node_variable);
    node_declaration->assignee = std::move(node_int);
    auto node_statement = std::make_unique<NodeStatement>(std::move(node_declaration), tok);
    node_statement->update_parents(nullptr);
    auto declare_ptr = static_cast<NodeSingleDeclareStatement*>(node_statement->statement.get());
    if(declare_ptr->assignee)
        std::cout << "no nullptr" << std::endl;
    declare_ptr->to_be_declared->parent->replace_with(std::move(node_int_replacement));
    if(declare_ptr-> assignee)
        std::cout << "that is wrong" << std::endl;

    return 0;




    std::string inputFilename;
    std::string outputFilename;

//    inputFilename = "/Users/mathias/Scripting/sonu-libraries/main.ksp";
//    inputFilename = "/Users/mathias/Scripting/the-score/the-score.ksp";
//    inputFilename = "/Users/mathias/Scripting/time-textures/time-textures.ksp";
//    inputFilename = "/Users/mathias/Scripting/legato-dev/legato.ksp";
//    inputFilename = "/Users/mathias/Scripting/ro-ki/rho_des.ksp";
//    inputFilename = "/Users/mathias/Scripting/pipe-organ/pipe-organ.ksp";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        std::string help = R"(
Usage: cksp [options] <input-file>
Options:
 -h, --help        Display usage information
 -o <file>         Set output file name (default: <input_dir>/out.txt)
 -v, --version     Display version number
)";
        std::string version = "cksp:\t\t"+COMPILER_VERSION+"\n";
        if (arg == "-h" || arg == "--help") {
            std::cout << help;
            return 0;
        } else if (arg == "-o") {
            if (i + 1 < argc) {
                outputFilename = argv[++i];
            } else {
                std::cerr << "Error: -o option requires one argument.\n";
                return 1;
            }
        } else if (arg == "-v" || arg == "--version") {
            std::cout << version;
            return 0;
        } else {
            inputFilename = arg;
        }
    }

    if (inputFilename.empty()) {
        std::cerr << "Error: No input file provided.\n";
        return 1;
    }
    if (std::filesystem::path(inputFilename).is_relative()) {
        inputFilename = std::filesystem::path(std::filesystem::current_path() / inputFilename).string();
    }
    if (outputFilename.empty()) {
        outputFilename = std::filesystem::path(std::filesystem::path(inputFilename).parent_path() / "out.txt").string();
    }

    std::cout << "Input File: " << inputFilename << std::endl;
    std::cout << "Output File: " << outputFilename << std::endl;

    // Startzeitpunkt speichern
    auto start_time = std::chrono::high_resolution_clock::now();


	Tokenizer tokenizer(inputFilename);
    auto tokens = tokenizer.tokenize();

    Preprocessor preprocessor(tokens, inputFilename);
    preprocessor.process();
    auto preprocessed_tokens = preprocessor.get_tokens();

//    for(auto& tok: preprocessed_tokens) {
//        if(tok.type != token::COMMENT or tok.type != token::LINEBRK)
//            std::cout << tok << std::endl;
//    }

    std::filesystem::path curr_path = __FILE__;
//	std::string engine_variables_file = (std::string) curr_path.parent_path() + "/Builtins/engine_variables.txt";
//	std::string engine_functions_file = (std::string) curr_path.parent_path() + "/Builtins/engine_functions.txt";
//	std::string engine_widgets_file = (std::string) curr_path.parent_path() + "/Builtins/engine_widgets.txt";;

    PreprocessorBuiltins builtins;
    builtins.process_builtins();


	auto preprocessor_time = std::chrono::high_resolution_clock::now();
	auto preprocessor_duration = std::chrono::duration_cast<std::chrono::milliseconds>(preprocessor_time - start_time);

    Parser parser(std::move(preprocessed_tokens));
	auto ast_result = parser.parse();
	if (ast_result.is_error()) {
		ast_result.get_error().exit();
	}

	auto ast = std::move(ast_result.unwrap());

    auto parsing_time = std::chrono::high_resolution_clock::now();
    auto parsing_duration = std::chrono::duration_cast<std::chrono::milliseconds>(parsing_time-preprocessor_time);

	ASTDesugar desugar(builtins.get_builtin_variables(), builtins.get_builtin_functions(), builtins.get_property_functions(), builtins.get_builtin_widgets());
	ast->accept(desugar);

    auto desugaring_time = std::chrono::high_resolution_clock::now();
    auto desugaring_duration = std::chrono::duration_cast<std::chrono::milliseconds>(desugaring_time-parsing_time);

    ASTVariables variables(builtins.get_builtin_variables(), builtins.get_builtin_functions(), builtins.get_builtin_arrays(), builtins.get_builtin_widgets(), desugar.get_function_inlines());
    ast->accept(variables);

	ASTTypeCasting typecast(builtins.get_builtin_widgets(), builtins.get_builtin_functions());
	ast->accept(typecast);

    ASTTypeChecking type_check;
    ast->accept(type_check);

//	ASTPrinter printer;
//	ast->accept(printer);

	ASTGenerator generator;
	ast->accept(generator);
//	generator.print();
	generator.generate(std::filesystem::path(curr_path.parent_path() / "test.txt").string());

    auto end_time = std::chrono::high_resolution_clock::now();
    auto type_checking_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - desugaring_time);
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    // Dauer in Millisekunden ausgeben
    std::cout << "Preprocessor Time: " << preprocessor_duration.count() << " ms, " << std::endl;
    std::cout << "Parsing Time: " << parsing_duration.count() << " ms, " << std::endl;
    std::cout << "Desugaring Time: " << desugaring_duration.count() << " ms, " << std::endl;
    std::cout << "Typechecking Time: " << type_checking_duration.count() << " ms, "<< std::endl;
    std::cout << "Time measured: " << duration.count() << " ms" << std::endl;

	std::cout << std::filesystem::current_path();
    return 0;
}
