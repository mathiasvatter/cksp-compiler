#include <iostream>
#include <filesystem>


//#include "Tokenizer.h"
#include "Parser.h"
#include "Preprocessor/Preprocessor.h"
#include "ASTVisitor.h"

int main() {

	// Startzeitpunkt speichern
    auto start_time = std::chrono::high_resolution_clock::now();

    auto path = "/Users/mathias/Scripting/sonu-libraries/main.ksp";
//    path = "/Users/mathias/Scripting/the-score/the-score.ksp";
//    path = "/Users/mathias/Scripting/the-score/sonu-libraries/ksp/2.0/sonulib.ksp";
	Tokenizer tokenizer(path);
    auto tokens = tokenizer.tokenize();

    Preprocessor preprocessor(tokens, path);
    preprocessor.process();
    auto preprocessed_tokens = preprocessor.get_tokens();

//    for(auto& tok: preprocessed_tokens) {
//        if(tok.type != token::COMMENT or tok.type != token::LINEBRK)
//            std::cout << tok << std::endl;
//    }
    Parser parser(std::move(preprocessed_tokens));
	auto ast = parser.parse();

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

	ASTPrinter printer;
	if (ast.is_error())
		ast.get_error().print();
	else
		ast.unwrap()->accept(printer);

    // Dauer in Millisekunden ausgeben
    std::cout << "Time measured: " << duration.count() << " ms" << std::endl;

//    std::cout << ksp_code << std::endl;
	std::cout << std::__fs::filesystem::current_path();
    return 0;
}
