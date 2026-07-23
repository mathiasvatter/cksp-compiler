#include <cstdlib>

#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#endif

#include "misc/CommandLineOptions.h"
#include "misc/CrashReporter.h"
#include "cksp/Compiler.h"
#include "lsp/LanguageServer.h"

int main(int argc, char* argv[]) {
	std::locale::global(std::locale::classic());
	std::cout.imbue(std::locale::classic());
	std::cin.imbue(std::locale::classic());

	ConsoleDiagnosticSink diagnostics;
	try {
		CommandLineOptions cli_options(argc, argv);
		if (cli_options.is_lsp_mode()) {
#if defined(_WIN32)
			// LSP Content-Length framing is byte-oriented. The Windows CRT defaults
			// stdio to text mode, which would expand every '\n' written in the
			// explicit "\r\n" headers to another "\r\n" and corrupt the protocol.
			if (_setmode(_fileno(stdin), _O_BINARY) == -1
				|| _setmode(_fileno(stdout), _O_BINARY) == -1) {
				std::cerr << "Unable to put LSP stdio into binary mode.\n";
				return EXIT_FAILURE;
			}
#endif
			CrashReporter::install("lsp", argc, argv);
			JsonRpcConnection connection(std::cin, std::cout);
			LanguageServer server(connection);
			return server.run();
		}

		CrashReporter::install("cli", argc, argv);
		auto config = cli_options.get_compiler_config();
		Compiler compiler(std::move(config));
		const auto result = compiler.compile(diagnostics);
		return result.success ? EXIT_SUCCESS : EXIT_FAILURE;
	} catch (const CompilationAborted& aborted) {
		// Command-line validation and compiler construction happen before Compiler::compile().
		diagnostics.report(aborted.diagnostic());
		return EXIT_FAILURE;
	}
}
