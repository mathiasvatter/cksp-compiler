#include <iostream>
#include <fstream>
#include <sstream>

//#include "Tokenizer.h"
#include "Parser.h"

std::string read_file(const char* filename) {
    std::ifstream file(filename);
    std::stringstream buffer;
    if (file.is_open()) {
        buffer << file.rdbuf();
        file.close();
    } else {
        std::cout << "Unable to open file\n";
    }
    return buffer.str();
}


int main() {

	// Startzeitpunkt speichern
    auto start_time = std::chrono::high_resolution_clock::now();

    auto path = "../main_snippet.ksp";
	std::string ksp_code = read_file(path);
    const char * ksp_code_ptr = ksp_code.c_str();
	Tokenizer tokenizer(ksp_code_ptr);
    auto tokens = tokenizer.tokenize();

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

	for (auto & token: tokens) {
        if (token.type != COMMENT && token.type != LINEBRK)
		    std::cout << token << '\n';
	}
	std::cout << std::endl;

    // Dauer in Millisekunden ausgeben
    std::cout << "Time measured: " << duration.count() << " ms" << std::endl;

//    Parser parser(tokens);
//    std::cout << ksp_code << std::endl;
	std::cout << std::__fs::filesystem::current_path();
    return 0;
}
