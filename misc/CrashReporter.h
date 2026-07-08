#pragma once

#include <csignal>
#include <cstdlib>
#include <cstring>
#include <string>

#if defined(__APPLE__) || defined(__linux__)
#include <execinfo.h>
#include <fcntl.h>
#include <limits.h>
#include <unistd.h>
#endif

#include "version.h"

namespace CrashReporter {
	namespace detail {
		struct CrashState {
			std::string mode;
			std::string command_line;
			std::string cwd;
			std::string log_path;
			bool write_log_file = false;
		};

		inline CrashState& state() {
			static CrashState crash_state;
			return crash_state;
		}

		inline std::string shell_join_command_line(const int argc, char* argv[]) {
			std::string result;
			for (int i = 0; i < argc; ++i) {
				if (i > 0) {
					result += ' ';
				}
				result += argv[i] ? argv[i] : "";
			}
			return result;
		}

		inline std::string current_working_directory() {
#if defined(__APPLE__) || defined(__linux__)
			char buffer[PATH_MAX];
			if (::getcwd(buffer, sizeof(buffer))) {
				return buffer;
			}
#endif
			return "unknown";
		}

		inline std::string configured_crash_log_path() {
			if (const char* configured = std::getenv("CKSP_CRASH_LOG")) {
				if (configured[0] != '\0') {
					return configured;
				}
			}
			return "";
		}

#if defined(__APPLE__) || defined(__linux__)
		inline void write_text(const int fd, const char* text) {
			::write(fd, text, std::strlen(text));
		}

		inline void write_text(const int fd, const std::string& text) {
			::write(fd, text.data(), text.size());
		}

		inline const char* signal_name(const int signal) {
			switch (signal) {
				case SIGSEGV: return "SIGSEGV";
				case SIGABRT: return "SIGABRT";
				case SIGBUS: return "SIGBUS";
				case SIGILL: return "SIGILL";
				default: return "unknown";
			}
		}

		inline void write_crash_report(const int fd, const int signal) {
			const auto& crash_state = state();
			write_text(fd, "\ncksp crashed\n");
			write_text(fd, "mode: ");
			write_text(fd, crash_state.mode);
			write_text(fd, "\nversion: ");
			write_text(fd, COMPILER_VERSION);
			write_text(fd, "\nsignal: ");
			write_text(fd, signal_name(signal));
			write_text(fd, "\ncwd: ");
			write_text(fd, crash_state.cwd);
			write_text(fd, "\ncommand: ");
			write_text(fd, crash_state.command_line);
			write_text(fd, "\n\nstacktrace:\n");

			void* frames[128];
			const int frame_count = ::backtrace(frames, 128);
			::backtrace_symbols_fd(frames, frame_count, fd);
		}

		inline void crash_handler(const int signal) {
			const auto& crash_state = state();
			write_crash_report(STDERR_FILENO, signal);

			if (!crash_state.write_log_file) {
				write_text(STDERR_FILENO, "\nSet CKSP_CRASH_LOG=/path/to/crash.log to write this report to a file.\n");
				std::signal(signal, SIG_DFL);
				std::raise(signal);
				::_exit(128 + signal);
			}

			const int fd = ::open(crash_state.log_path.c_str(), O_CREAT | O_WRONLY | O_APPEND, 0644);
			if (fd != -1) {
				write_crash_report(fd, signal);
				::close(fd);
				write_text(STDERR_FILENO, "\nCrash report also written to:\n  ");
				write_text(STDERR_FILENO, crash_state.log_path);
				write_text(STDERR_FILENO, "\nPlease include this file when reporting the issue.\n");
			} else {
				write_text(STDERR_FILENO, "\nCould not write crash report file:\n  ");
				write_text(STDERR_FILENO, crash_state.log_path);
				write_text(STDERR_FILENO, "\n");
			}

			std::signal(signal, SIG_DFL);
			std::raise(signal);
			::_exit(128 + signal);
		}
#endif
	}

	inline void install(const char* mode, const int argc, char* argv[]) {
		auto& crash_state = detail::state();
		crash_state.mode = mode ? mode : "unknown";
		crash_state.command_line = detail::shell_join_command_line(argc, argv);
		crash_state.cwd = detail::current_working_directory();
		crash_state.log_path = detail::configured_crash_log_path();
		crash_state.write_log_file = !crash_state.log_path.empty();

#if defined(__APPLE__) || defined(__linux__)
		std::signal(SIGSEGV, detail::crash_handler);
		std::signal(SIGABRT, detail::crash_handler);
		std::signal(SIGBUS, detail::crash_handler);
		std::signal(SIGILL, detail::crash_handler);
#endif
	}

	[[nodiscard]] inline const std::string& crash_log_path() {
		return detail::state().log_path;
	}
}
