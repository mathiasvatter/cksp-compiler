//
// Created by Mathias Vatter on 29.08.23.
//

#include "CompileError.h"
#include "../Tokenizer/Tokenizer.h"

#include <utility>
#include <format>
#include <array>
#if defined(_WIN32)
    #include <windows.h>
    #include <VersionHelpers.h>
#elif defined(__APPLE__) || defined(__linux__)
    #include <sys/utsname.h>
#endif

CompileError::CompileError(ErrorType type, std::string message, std::string expected, const Token &token)
        : m_type(type), m_message(std::move(message)), m_expected(std::move(expected)) {
    m_line_number = token.line; m_file_name = token.file; m_got = token.val; m_line_position = token.pos;
	m_marker_length = token.val.length();
}

CompileError::CompileError(ErrorType type, std::string message, size_t lineNumber, std::string expected,
                           std::string got, std::string fileName)
        : m_type(type), m_message(std::move(message)), m_expected(std::move(expected)), m_got(std::move(got)),
          m_line_number(lineNumber), m_file_name(std::move(fileName)) {
	m_line_position = 0;
	m_marker_length = 0;
}

void CompileError::print(ErrorType err) {
    if (m_got == "\n") {
        m_got = "linebreak";
    }
    auto error_code = ColorCode::Red;
    if(err == ErrorType::CompileError)
        error_code = ColorCode::Red;
    if(err == ErrorType::CompileWarning)
        error_code = ColorCode::Yellow;
    std::cout << error_code << ColorCode::Bold << error_type_to_string(err) << ColorCode::Reset;
    std::cout << error_code << " [Type: " << ColorCode::Bold << error_type_to_string(m_type) << ColorCode::Reset;
    std::cout << error_code << ", " << "Position: " << m_file_name;
    if(m_line_number != -1)
        std::cout << ":" << m_line_number;
    if(m_line_position > 0)
        std::cout << ":" << m_line_position;
    std::cout << "]" << std::endl;
    std::cout << m_message << " ";
    if(!m_expected.empty())
        std::cout << "Expected: '" << m_expected << "', ";
    if(!m_got.empty())
        std::cout << "got: '" << m_got << "'";
    std::cout << ColorCode::Reset << std::endl;

    // Zeige die entsprechende Zeile aus der Datei
    if(m_line_number != -1) {
        std::string in_line = "In line " + std::to_string(m_line_number) + ": ";
        std::cout << ColorCode::Bold << in_line << ColorCode::Reset;
        std::string line = replace_tabs_with_spaces(get_line_from_file(), 1);
        std::cout << line << std::endl;

        if(m_line_position > 0) {
			std::cout << error_code;
			auto start_pos = m_line_position - 1 + in_line.length();
			auto end_pos = m_line_position - 1 + in_line.length() + m_marker_length-1;
            for (int i = 0; i < (in_line.length() + line.length()); i++) {
                if (i >= start_pos && i <= end_pos) {
                    std::cout << "^";
                } else {
                    std::cout << " ";
                }
            }
            std::cout << ColorCode::Reset << std::endl;
        }
    }
}

void CompileError::exit(ErrorType err) {
    print(err);
    std::cout << ColorCode::Red << "\nSeems like the compilation exited with a failure." << std::endl;
    std::cout << ColorCode::Reset << "To help make cksp better, please report any compiler related issues here: " << generate_github_issue_url("mathiasvatter", "cksp-compiler-issues") << std::endl;
    ::exit(EXIT_FAILURE);
}

std::string CompileError::get_line_from_file() {
    std::ifstream file(m_file_name);
    std::string line;
    if (file.is_open()) {
        for (size_t i = 0; i < m_line_number && std::getline(file, line); ++i) {
            // Nichts tun, nur bis zur gewünschten Zeile lesen
        }
        file.close();
        return line;
    }
    CompileError(ErrorType::FileError, "Unable to open file.", -1, "valid path/valid *.ksp file", m_file_name, "").exit();
    return "";
}

std::string CompileError::replace_tabs_with_spaces(const std::string &input, int spacesPerTab) {
    std::string output;
    for (char c : input) {
        if (c == '\t') {
            // Fügt spacesPerTab Leerzeichen hinzu, wenn ein Tab gefunden wird
            output += std::string(spacesPerTab, ' ');
        } else {
            // Fügt das aktuelle Zeichen hinzu, wenn es kein Tab ist
            output += c;
        }
    }
    return output;
}

std::string CompileError::get_os_version() {
    std::string os_version;
    #if defined(_WIN32)
        if (IsWindows10OrGreater()) {
            os_version = "Windows 10";
        } else if (IsWindows8Point1OrGreater()) {
            os_version = "Windows 8.1";
        } else if (IsWindows8OrGreater()) {
            os_version = "Windows 8";
        } else if (IsWindows7SP1OrGreater()) {
            os_version = "Windows 7 SP1";
        } else if (IsWindows7OrGreater()) {
            os_version = "Windows 7";
        } else {
            os_version = "Windows version unknown";
        }
    #elif defined(__APPLE__)
        try {
            os_version = exec("sw_vers -productName");
            os_version += " ";
            os_version += exec("sw_vers -productVersion");
        } catch (const std::runtime_error& e) {
            os_version = "Failed to get macOS version";
        }
    #elif defined(__linux__)
        try {
            os_version = exec("lsb_release -d");
            os_version.erase(0, os_version.find(":") + 1); // Entfernt "Description:" Teil der Ausgabe
            os_version.erase(0, os_version.find_first_not_of(" \t")); // Trimmt führende Leerzeichen
        } catch (const std::runtime_error& e) {
            os_version = "Failed to get Linux distribution version";
        }
    #endif
    return os_version;
}

std::string CompileError::get_os_architecture() {
    std::string os_architecture;
#if defined(_WIN32)
//    os_architecture = "x86";
    BOOL isWow64 = FALSE;
        IsWow64Process(GetCurrentProcess(), &isWow64);
        if (isWow64) {
            os_architecture = "x64";
        } else {
            os_architecture = "x86";
        }
#elif defined(__APPLE__) || defined(__linux__)
    struct utsname buffer{};
    if (uname(&buffer) != -1) {
        os_architecture = buffer.machine;
    }
#endif
    return os_architecture;
}

std::string CompileError::generate_github_issue_url(const std::string &username, const std::string &repo) {
    // Fehlermeldung und Code-Snippet für das Issue vorbereiten
    std::stringstream ss; // Added this line
    ss << error_type_to_string(m_type) << ": " << m_message; // Added this line
    std::string issue_title = ss.str(); // Changed this line

    std::stringstream issue_body; // Added this line
    issue_body << "**Error Message**\n\n" << m_message << "\n\n"
               << "**Code Snippet**\n\n```\n" << get_line_from_file() << "\n```\n\n"
               << "**Actual Behavior**\n\nGot: " << m_got << "\n\n**Expected Behavior**\n\nExpected: " << m_expected << "\n\n"
               << "**CKSP Version**\n\nv" << COMPILER_VERSION << "\n\n"
               << "**Environment**\n\n- OS Version: " << get_os_version() << "\n- Architecture: " << get_os_architecture() << "\n\n";

    std::string encoded_title = url_encode(issue_title);
    std::string encoded_body = url_encode(issue_body.str()); // Changed this line
    std::string encoded_labels = url_encode("bug");

    std::string issueUrl = "https://github.com/" + username + "/" + repo + "/issues/new?labels=" + encoded_labels + "&title=" + encoded_title + "&body=" + encoded_body; // Changed this line
    return issueUrl;
}

std::string CompileError::url_encode(const std::string &value) {
    std::ostringstream encoded;
    encoded.fill('0');
    encoded << std::hex;

    for (char c : value) {
        // Behalte alphanumerische Zeichen und einige andere intakt
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded << c;
        } else {
            // Prozent-encode andere Zeichen
            encoded << std::uppercase;
            encoded << '%' << std::setw(2) << int((unsigned char) c);
            encoded << std::nouppercase;
        }
    }

    return encoded.str();
}

void CompileError::set_message(const std::string &message) {
	CompileError::m_message = message;
}

std::string exec(const char *cmd) {
    std::array<char, 128> buffer{};
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        return "";
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    result.erase(std::remove(result.begin(), result.end(), '\n'), result.end());
    return result;
}
