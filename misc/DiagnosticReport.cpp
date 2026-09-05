#include "DiagnosticReport.h"

#if defined(_WIN32)
#include <windows.h>
#include <VersionHelpers.h>
#elif defined(__APPLE__) || defined(__linux__)
#include <sys/utsname.h>
#endif

namespace cli {
    #if defined(__APPLE__) || defined(__linux__)
    std::string execute_command(const char* command) {
        std::array<char, 128> buffer{};
        std::string result;
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command, "r"), pclose);
        if (!pipe) return "";
        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) result += buffer.data();
        result.erase(std::remove(result.begin(), result.end(), '\n'), result.end());
        return result;
    }
    #endif
}

std::string get_os_version() {
#if defined(_WIN32)
    if (IsWindows10OrGreater()) return "Windows 10";
    if (IsWindows8Point1OrGreater()) return "Windows 8.1";
    if (IsWindows8OrGreater()) return "Windows 8";
    if (IsWindows7SP1OrGreater()) return "Windows 7 SP1";
    if (IsWindows7OrGreater()) return "Windows 7";
    return "Windows version unknown";
#elif defined(__APPLE__)
    return cli::execute_command("sw_vers -productName") + " " + cli::execute_command("sw_vers -productVersion");
#elif defined(__linux__)
    return execute_command("lsb_release -d");
#else
    return "Unknown OS";
#endif
}

/// Helps getting the current os architecture for the cli error report
std::string get_os_architecture() {
#if defined(_WIN32)
    BOOL is_wow64 = FALSE;
    IsWow64Process(GetCurrentProcess(), &is_wow64);
    return is_wow64 ? "x64" : "x86";
#elif defined(__APPLE__) || defined(__linux__)
    struct utsname buffer{};
    return uname(&buffer) == -1 ? "unknown" : buffer.machine;
#else
    return "unknown";
#endif
}
