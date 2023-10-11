#include <iostream>
#include <filesystem>


//#include "Tokenizer.h"
#include "Parser.h"
#include "Preprocessor/Preprocessor.h"



int main() {

	// Startzeitpunkt speichern
    auto start_time = std::chrono::high_resolution_clock::now();

    auto path = "/Users/mathias/Scripting/sonu-libraries/main.ksp";
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
	parser.parse();

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    // Dauer in Millisekunden ausgeben
    std::cout << "Time measured: " << duration.count() << " ms" << std::endl;

//    std::cout << ksp_code << std::endl;
	std::cout << std::__fs::filesystem::current_path();
    return 0;
}
