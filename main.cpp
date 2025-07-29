

#include "misc/CommandLineOptions.h"
#include "Compiler.h"




int main(int argc, char* argv[]) {
	std::locale::global(std::locale::classic());
	std::cout.imbue(std::locale::classic());
	std::cin.imbue(std::locale::classic());

	// std::locale::global(std::locale("C"));
    CommandLineOptions cli_options(argc, argv);
	auto config = cli_options.get_compiler_config();
	Compiler compiler(config);
	compiler.compile();

    return 0;
}
