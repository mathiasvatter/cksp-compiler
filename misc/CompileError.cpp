//
// Created by Mathias Vatter on 29.08.23.
//

#include "CompileError.h"
#include "../Tokenizer/Tokenizer.h"

#include <utility>
#include <array>

#include "../utils/StringUtils.h"
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

void CompileError::print(const ErrorType err) {
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
    std::cout << error_code << ", " << "Position: ";

    std::string pos_text = m_file_name;
    if (m_line_number != -1) pos_text += ":" + std::to_string(m_line_number);
    if (m_line_position > 0) pos_text += ":" + std::to_string(m_line_position);

    // std::string uri = "file://" + StringUtils::percent_encode_uri_path(m_file_name);
    // if (m_line_number != -1) {
    //     uri += ":" + std::to_string(m_line_number);
    //     if (m_line_position > 0) uri += ":" + std::to_string(m_line_position);
    // }

    std::cout << pos_text;
    std::cout << "]\n";

    // ---- sauber formatierte Message
    // std::string msg = StringUtils::normalize_sentence(m_message);
    std::cout << m_message << "\n";

    // ---- optionale Felder, immer in neuer Zeile
    if (!m_expected.empty()) {
        std::cout << "Expected: '" << StringUtils::normalize_field(m_expected) << "'; ";
    }
    if (!m_got.empty()) {
        std::cout << "Got: '" << StringUtils::normalize_field(m_got) << "'";
    }
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

void CompileError::exit(const ErrorType err) {
    print(err);
    std::cout << ColorCode::Red << "\nSeems like the compilation exited with a failure." << std::endl;
	if(!m_file_name.empty()) {
    	std::cout << ColorCode::Reset << "To help make cksp better, please report any compiler related issues here: " << generate_github_issue_url("mathiasvatter", "cksp-compiler") << std::endl;
	}
    ::exit(EXIT_FAILURE);
}

std::string CompileError::get_line_from_file() const {
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

std::string CompileError::generate_github_issue_url(const std::string &username, const std::string &repo) const {
    std::stringstream title_stream;
    title_stream << "[BUG] " << error_type_to_string(m_type) << ": " << m_message;
    std::string issue_title = title_stream.str();

    std::stringstream description_stream;
    description_stream << m_message;
    if (!m_expected.empty()) {
        description_stream << "\nExpected: " << m_expected;
    }
    if (!m_got.empty()) {
        description_stream << "\nGot: " << m_got;
    }
    if (!m_file_name.empty()) {
        description_stream << "\nFile: " << m_file_name;
    }
    if (m_line_number != static_cast<size_t>(-1)) {
        description_stream << "\nLine: " << m_line_number;
    }

    std::stringstream reproduce_stream;
    reproduce_stream << "```cksp\n" << get_line_from_file() << "\n```";

    std::string issue_url = "https://github.com/" + username + "/" + repo + "/issues/new";
    issue_url += "?template=" + StringUtils::percent_encode_uri("bug_report.yml");
    issue_url += "&title=" + StringUtils::percent_encode_uri(issue_title);
    issue_url += "&description=" + StringUtils::percent_encode_uri(description_stream.str());
    issue_url += "&reproduce=" + StringUtils::percent_encode_uri(reproduce_stream.str());
    issue_url += "&cksp_version=" + StringUtils::percent_encode_uri(COMPILER_VERSION);
    issue_url += "&os=" + StringUtils::percent_encode_uri(get_os_version());
    issue_url += "&arch=" + StringUtils::percent_encode_uri(get_os_architecture());

    return issue_url;
}

void CompileError::set_message(const std::string &message) {
	m_message = message;
}

void CompileError::add_message(const std::string &message) {
    m_message += " "+ message;
}

void CompileError::set_expected(const std::string &expected) {
    m_expected = expected;
}

void CompileError::set_token(const Token &token) {
    m_line_number = token.line; m_file_name = token.file; m_got = token.val; m_line_position = token.pos;
    m_marker_length = token.val.length();
}

#if defined(__APPLE__) || defined(__linux__)

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
#endif
