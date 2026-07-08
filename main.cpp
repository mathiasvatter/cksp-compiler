#include <csignal>
#include <cstdlib>
#include <cstring>

#include <execinfo.h>
#include <fcntl.h>
#include <unistd.h>

#include "misc/CommandLineOptions.h"
#include "cksp/Compiler.h"
#include "lsp/LanguageServer.h"

namespace {
void lsp_crash_handler(const int signal) {
	const int fd = ::open("/tmp/cksp-lsp-crash.log", O_CREAT | O_WRONLY | O_APPEND, 0644);
	if (fd != -1) {
		const char header[] = "\ncksp --lsp crashed\n";
		::write(fd, header, std::strlen(header));

		const char* signal_name = "signal: unknown\n";
		switch (signal) {
			case SIGSEGV: signal_name = "signal: SIGSEGV\n"; break;
			case SIGABRT: signal_name = "signal: SIGABRT\n"; break;
			case SIGBUS: signal_name = "signal: SIGBUS\n"; break;
			case SIGILL: signal_name = "signal: SIGILL\n"; break;
			default: break;
		}
		::write(fd, signal_name, std::strlen(signal_name));

		void* frames[128];
		const int frame_count = ::backtrace(frames, 128);
		::backtrace_symbols_fd(frames, frame_count, fd);
		::close(fd);
	}

	std::signal(signal, SIG_DFL);
	std::raise(signal);
}

void install_lsp_crash_handler() {
	std::signal(SIGSEGV, lsp_crash_handler);
	std::signal(SIGABRT, lsp_crash_handler);
	std::signal(SIGBUS, lsp_crash_handler);
	std::signal(SIGILL, lsp_crash_handler);
}
}



int main(int argc, char* argv[]) {
	std::locale::global(std::locale::classic());
	std::cout.imbue(std::locale::classic());
	std::cin.imbue(std::locale::classic());

	ConsoleDiagnosticSink diagnostics;
	try {
		CommandLineOptions cli_options(argc, argv);
		if (cli_options.is_lsp_mode()) {
			install_lsp_crash_handler();
			JsonRpcConnection connection(std::cin, std::cout);
			LanguageServer server(connection);
			return server.run();
		}

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
