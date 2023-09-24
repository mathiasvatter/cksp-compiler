#include <iostream>
#include <filesystem>


//#include "Tokenizer.h"
#include "Parser.h"
#include "Preprocessor.h"



int main() {

	// Startzeitpunkt speichern
    auto start_time = std::chrono::high_resolution_clock::now();

    auto path = "/Users/mathias/Scripting/sonu-libraries/main.ksp";
	Tokenizer tokenizer(path);
    auto tokens = tokenizer.tokenize();
	for (auto & token: tokens) {
        if (token.type != COMMENT) // && token.type != LINEBRK)
		    std::cout << token << '\n';
	}
	std::cout << std::endl;
    Preprocessor preprocessor(tokens, path);
    Parser parser(preprocessor.get_tokens());

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    // Dauer in Millisekunden ausgeben
    std::cout << "Time measured: " << duration.count() << " ms" << std::endl;

//    std::cout << ksp_code << std::endl;
	std::cout << std::__fs::filesystem::current_path();
    return 0;
}
