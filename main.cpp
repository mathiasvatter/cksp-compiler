#include <iostream>
#include <fstream>
#include <sstream>

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



    auto path = "/Users/mathias/CLionProjects/Compiler/main.ksp";
    std::cout << read_file(path) << std::endl;

    return 0;
}
