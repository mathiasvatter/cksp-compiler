#include <iostream>
#include <filesystem>


//#include "Tokenizer.h"
#include "Parser.h"
#include "Preprocessor/Preprocessor.h"
#include "AST/ASTVisitor.h"
#include "AST/ASTDesugar.h"
#include "AST/ASTPrinter.h"
#include "AST/ASTTypeCasting.h"
#include "AST/ASTMacros.h"

int main() {

	// Startzeitpunkt speichern
    auto start_time = std::chrono::high_resolution_clock::now();

    auto path = "/Users/mathias/Scripting/sonu-libraries/main.ksp";
//    path = "/Users/mathias/Scripting/the-score/the-score.ksp";
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
	auto preprocessor_time = std::chrono::high_resolution_clock::now();
	auto preprocessor_duration = std::chrono::duration_cast<std::chrono::milliseconds>(preprocessor_time - start_time);

    Parser parser(std::move(preprocessed_tokens), preprocessor.get_macro_definitions());
	auto ast_result = parser.parse();
	if (ast_result.is_error()) {
		ast_result.get_error().print();
		exit(EXIT_FAILURE);
	}

	auto ast = std::move(ast_result.unwrap());

    ASTMacros macro_processing;
    ast->accept(macro_processing);

	ASTDesugar desugar;
	ast->accept(desugar);

	ASTTypeCasting typecast;
	ast->accept(typecast);

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

	ASTPrinter printer;
	ast->accept(printer);

    // Dauer in Millisekunden ausgeben
    std::cout << "Preprocessor Time: " << preprocessor_duration.count() << " ms, Time measured: " << duration.count() << " ms" << std::endl;

//    std::cout << ksp_code << std::endl;
	std::cout << std::__fs::filesystem::current_path();
    return 0;
}
