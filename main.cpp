#include <iostream>
#include <filesystem>


//#include "Tokenizer.h"
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
//#include "AST/ASTMacros.h"

int main() {

	// Startzeitpunkt speichern
    auto start_time = std::chrono::high_resolution_clock::now();

    auto path = "/Users/mathias/Scripting/sonu-libraries/main.ksp";
    path = "/Users/mathias/Scripting/the-score/the-score.ksp";
//    path = "/Users/mathias/Scripting/time-textures/time-textures.ksp";
	Tokenizer tokenizer(path);
    auto tokens = tokenizer.tokenize();

    Preprocessor preprocessor(tokens, path);
    preprocessor.process();
    auto preprocessed_tokens = preprocessor.get_tokens();

//    for(auto& tok: preprocessed_tokens) {
//        if(tok.type != token::COMMENT or tok.type != token::LINEBRK)
//            std::cout << tok << std::endl;
//    }

    std::filesystem::path curr_path = __FILE__;
	std::string engine_variables_file = (std::string) curr_path.parent_path() + "/Builtins/engine_variables.txt";
	std::string engine_functions_file = (std::string) curr_path.parent_path() + "/Builtins/engine_functions.txt";
	std::string engine_widgets_file = (std::string) curr_path.parent_path() + "/Builtins/engine_widgets.txt";;

    PreprocessorBuiltins builtins(engine_variables_file, engine_functions_file, engine_widgets_file);
    builtins.process_builtins();


	auto preprocessor_time = std::chrono::high_resolution_clock::now();
	auto preprocessor_duration = std::chrono::duration_cast<std::chrono::milliseconds>(preprocessor_time - start_time);

    Parser parser(std::move(preprocessed_tokens));
	auto ast_result = parser.parse();
	if (ast_result.is_error()) {
		ast_result.get_error().print();
		exit(EXIT_FAILURE);
	}

	auto ast = std::move(ast_result.unwrap());

    auto parsing_time = std::chrono::high_resolution_clock::now();
    auto parsing_duration = std::chrono::duration_cast<std::chrono::milliseconds>(parsing_time-preprocessor_time);

//    ASTMacros macro_processing;
//    ast->accept(macro_processing);

	ASTDesugar desugar(builtins.get_builtin_variables(), builtins.get_builtin_functions(), builtins.get_property_functions(), builtins.get_builtin_widgets());
	ast->accept(desugar);

    auto desugaring_time = std::chrono::high_resolution_clock::now();
    auto desugaring_duration = std::chrono::duration_cast<std::chrono::milliseconds>(desugaring_time-parsing_time);

    ASTVariables variables(builtins.get_builtin_variables(), builtins.get_builtin_functions(), builtins.get_builtin_arrays(), builtins.get_builtin_widgets());
    ast->accept(variables);

	ASTTypeCasting typecast(builtins.get_builtin_widgets());
	ast->accept(typecast);

    ASTTypeChecking type_check;
    ast->accept(type_check);

    auto end_time = std::chrono::high_resolution_clock::now();
    auto type_checking_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - desugaring_time);
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

//	ASTPrinter printer;
//	ast->accept(printer);

	ASTGenerator generator;
	ast->accept(generator);
//	generator.print();
	generator.generate((std::string) curr_path.parent_path()+"/test.txt");
    // Dauer in Millisekunden ausgeben
    std::cout << "Preprocessor Time: " << preprocessor_duration.count() << " ms, " << std::endl;
    std::cout << "Parsing Time: " << parsing_duration.count() << " ms, " << std::endl;
    std::cout << "Desugaring Time: " << desugaring_duration.count() << " ms, " << std::endl;
    std::cout << "Typechecking Time: " << type_checking_duration.count() << " ms, "<< std::endl;
    std::cout << "Time measured: " << duration.count() << " ms" << std::endl;

//    std::cout << ksp_code << std::endl;
	std::cout << std::__fs::filesystem::current_path();
    return 0;
}
