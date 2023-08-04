#include <iostream>
#include <fstream>
#include <sstream>

#include "Lexer.h"

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

    auto path = "../main.ksp";
	std::string ksp_code = read_file(path);
    const char * ksp_code_ptr = ksp_code.c_str();
	Lexer lex(ksp_code_ptr);
//    std::cout << ksp_code << std::endl;
	std::cout << std::__fs::filesystem::current_path();
    return 0;
}
