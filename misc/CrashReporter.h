#pragma once

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#if defined(__APPLE__) || defined(__linux__)
#include <execinfo.h>
#include <fcntl.h>
#include <limits.h>
#include <unistd.h>
#elif defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
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
#elif defined(_WIN32)
			const DWORD required = ::GetCurrentDirectoryA(0, nullptr);
			if (required > 0) {
				std::string buffer(required, '\0');
				const DWORD written = ::GetCurrentDirectoryA(required, buffer.data());
				if (written > 0 && written < required) {
					buffer.resize(written);
					return buffer;
				}
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
#elif defined(_WIN32)
		inline void write_text(const HANDLE output, const char* text) {
			if (!output || output == INVALID_HANDLE_VALUE || !text) return;
			DWORD written = 0;
			::WriteFile(output, text, static_cast<DWORD>(std::strlen(text)), &written, nullptr);
		}

		inline void write_text(const HANDLE output, const std::string& text) {
			if (!output || output == INVALID_HANDLE_VALUE || text.empty()) return;
			DWORD written = 0;
			::WriteFile(output, text.data(), static_cast<DWORD>(text.size()), &written, nullptr);
		}

		inline const char* signal_name(const int signal) {
			switch (signal) {
				case SIGSEGV: return "SIGSEGV";
				case SIGABRT: return "SIGABRT";
				case SIGILL: return "SIGILL";
				case SIGFPE: return "SIGFPE";
				default: return "unknown";
			}
		}

		inline void write_stack_addresses(const HANDLE output) {
			void* frames[64]{};
			const USHORT frame_count = ::CaptureStackBackTrace(0, 64, frames, nullptr);
			char line[64];
			for (USHORT index = 0; index < frame_count; ++index) {
				const int length = std::snprintf(
					line, sizeof(line), "  #%u %p\n", static_cast<unsigned>(index), frames[index]);
				if (length > 0) write_text(output, std::string(line, static_cast<size_t>(length)));
			}
		}

		inline void write_crash_report(
			const HANDLE output,
			const char* reason,
			const DWORD exception_code = 0,
			const void* exception_address = nullptr) {
			const auto& crash_state = state();
			write_text(output, "\ncksp crashed\nmode: ");
			write_text(output, crash_state.mode);
			write_text(output, "\nversion: ");
			write_text(output, COMPILER_VERSION);
			write_text(output, "\nreason: ");
			write_text(output, reason ? reason : "unknown");
			if (exception_code != 0) {
				char exception[96];
				const int length = std::snprintf(
					exception,
					sizeof(exception),
					"\nexception code: 0x%08lX\nexception address: %p",
					static_cast<unsigned long>(exception_code),
					exception_address);
				if (length > 0) write_text(output, std::string(exception, static_cast<size_t>(length)));
			}
			write_text(output, "\ncwd: ");
			write_text(output, crash_state.cwd);
			write_text(output, "\ncommand: ");
			write_text(output, crash_state.command_line);
			write_text(output, "\n\nstack addresses:\n");
			write_stack_addresses(output);
		}

		inline HANDLE open_crash_log() {
			const auto& crash_state = state();
			if (!crash_state.write_log_file) return INVALID_HANDLE_VALUE;
			return ::CreateFileA(
				crash_state.log_path.c_str(),
				FILE_APPEND_DATA,
				FILE_SHARE_READ | FILE_SHARE_WRITE,
				nullptr,
				OPEN_ALWAYS,
				FILE_ATTRIBUTE_NORMAL,
				nullptr);
		}

		inline void write_windows_reports(
			const char* reason,
			const DWORD exception_code = 0,
			const void* exception_address = nullptr) {
			const HANDLE standard_error = ::GetStdHandle(STD_ERROR_HANDLE);
			write_crash_report(standard_error, reason, exception_code, exception_address);

			const HANDLE crash_log = open_crash_log();
			if (crash_log != INVALID_HANDLE_VALUE) {
				write_crash_report(crash_log, reason, exception_code, exception_address);
				::CloseHandle(crash_log);
				write_text(standard_error, "\nCrash report also written to:\n  ");
				write_text(standard_error, state().log_path);
				write_text(standard_error, "\nPlease include this file when reporting the issue.\n");
			} else if (state().write_log_file) {
				write_text(standard_error, "\nCould not write crash report file:\n  ");
				write_text(standard_error, state().log_path);
				write_text(standard_error, "\n");
			} else {
				write_text(standard_error, "\nSet CKSP_CRASH_LOG to write this report to a file.\n");
			}
		}

		inline volatile LONG& crash_in_progress() {
			static volatile LONG value = 0;
			return value;
		}

		inline LONG WINAPI unhandled_exception_handler(EXCEPTION_POINTERS* exception) {
			if (::InterlockedExchange(&crash_in_progress(), 1) != 0) {
				return EXCEPTION_CONTINUE_SEARCH;
			}
			const auto* record = exception ? exception->ExceptionRecord : nullptr;
			write_windows_reports(
				"unhandled Windows exception",
				record ? record->ExceptionCode : 0,
				record ? record->ExceptionAddress : nullptr);
			return EXCEPTION_EXECUTE_HANDLER;
		}

		inline void crash_handler(const int signal) {
			if (::InterlockedExchange(&crash_in_progress(), 1) == 0) {
				write_windows_reports(signal_name(signal));
			}
			::ExitProcess(static_cast<UINT>(128 + signal));
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
#elif defined(_WIN32)
		::SetUnhandledExceptionFilter(detail::unhandled_exception_handler);
		std::signal(SIGSEGV, detail::crash_handler);
		std::signal(SIGABRT, detail::crash_handler);
		std::signal(SIGILL, detail::crash_handler);
		std::signal(SIGFPE, detail::crash_handler);
#endif
	}

	[[nodiscard]] inline const std::string& crash_log_path() {
		return detail::state().log_path;
	}
}
