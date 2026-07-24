#include "SourceProvider.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_set>

#include "../../misc/PathHandler.h"
#include "../../utils/StringUtils.h"

namespace {
Diagnostic source_error(
    const SourceId& source,
    std::string message,
    std::string expected,
    std::string actual) {
    return Diagnostic(
        ErrorType::FileError,
        std::move(message),
        static_cast<size_t>(-1),
        std::move(expected),
        std::move(actual),
        source.value);
}

bool has_supported_extension(const std::filesystem::path& path) {
    static const std::unordered_set<std::string> extensions = {
        ".ksp", ".cksp", ".nckp", ".txt"
    };
    return extensions.contains(path.extension().string());
}

std::string percent_decode_uri(std::string_view value) {
    std::string decoded;
    decoded.reserve(value.size());

    for (size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '%' && i + 2 < value.size()
            && std::isxdigit(static_cast<unsigned char>(value[i + 1]))
            && std::isxdigit(static_cast<unsigned char>(value[i + 2]))) {
            const std::string hex{value.substr(i + 1, 2)};
            decoded.push_back(static_cast<char>(std::strtol(hex.c_str(), nullptr, 16)));
            i += 2;
        } else {
            decoded.push_back(value[i]);
        }
    }

    return decoded;
}
}

SourceId source_from_uri(const std::string_view uri) {
    static constexpr std::string_view file_scheme = "file://";
    if (uri.starts_with(file_scheme)) {
        auto path = percent_decode_uri(uri.substr(file_scheme.size()));
#if defined(_WIN32)
        // A local Windows file URI has the form file:///C:/path. The slash before
        // the drive letter belongs to the URI syntax, but makes std::filesystem
        // treat the decoded value as a rooted path on the current drive.
        if (path.size() >= 3 && path[0] == '/'
            && std::isalpha(static_cast<unsigned char>(path[1]))
            && path[2] == ':') {
            path.erase(path.begin());
        } else if (!path.empty() && path[0] != '/') {
            // A non-empty file URI authority denotes a UNC host.
            path.insert(0, "//");
        }
#endif
        return FileSystemSourceProvider::normalize(path);
    }
    return FileSystemSourceProvider::normalize(uri);
}

std::string uri_from_source(const SourceId& source) {
    auto path = FileSystemSourceProvider::normalize(source.value).value;
#if defined(_WIN32)
    std::ranges::replace(path, '\\', '/');
    const auto encoded = StringUtils::percent_encode_uri_path(path);
    if (path.starts_with("//")) {
        return "file:" + encoded;
    }
    return "file:///" + encoded;
#else
    return "file://" + StringUtils::percent_encode_uri_path(path);
#endif
}

SourceId FileSystemSourceProvider::normalize(const std::string_view path) {
    std::filesystem::path value(path);
    if (value.is_relative()) value = std::filesystem::absolute(value);

    std::error_code error;
    const auto canonical = std::filesystem::weakly_canonical(value, error);
    return SourceId((error ? value.lexically_normal() : canonical).string());
}

Result<SourceDocument> FileSystemSourceProvider::load(const SourceId& source) {
    const auto normalized = normalize(source.value);
    const std::filesystem::path path(normalized.value);
    if (!has_supported_extension(path)) {
        return Result<SourceDocument>(source_error(
            normalized,
            "Unable to open source. Not a valid file type.",
            "*.ksp, *.cksp, *.nckp or *.txt",
            path.extension().string()));
    }

    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        return Result<SourceDocument>(source_error(
            normalized,
            "Unable to open source.",
            "A readable source document",
            normalized.value));
    }

    std::ostringstream contents;
    contents << input.rdbuf();
    return Result<SourceDocument>(SourceDocument{
        normalized,
        std::make_shared<const std::string>(std::move(contents).str()),
        std::nullopt
    });
}

Result<SourceId> FileSystemSourceProvider::resolve_import(const SourceId& root, const SourceId& importer, const std::string_view import_path) {
    Token token;
    token.file = importer.value;
    PathHandler resolver(
        std::move(token),
        importer.value,
        std::filesystem::path(root.value).parent_path().string());
    auto result = resolver.resolve_import_path(std::string(import_path));
    if (result.is_error()) return Result<SourceId>(result.get_error());
    return Result<SourceId>(normalize(result.unwrap()));
}

std::string OverlaySourceProvider::key(const SourceId& source) {
    return FileSystemSourceProvider::normalize(source.value).value;
}

void OverlaySourceProvider::update(SourceId source, std::string text, const int64_t version) {
    source = FileSystemSourceProvider::normalize(source.value);
    SourceDocument document{
        source,
        std::make_shared<const std::string>(std::move(text)),
        version
    };
    std::unique_lock lock(m_mutex);
    m_documents.insert_or_assign(source.value, std::move(document));
}

void OverlaySourceProvider::close(const SourceId& source) {
    std::unique_lock lock(m_mutex);
    m_documents.erase(key(source));
}

Result<SourceDocument> OverlaySourceProvider::load(const SourceId& source) {
    {
        std::shared_lock lock(m_mutex);
        if (const auto found = m_documents.find(key(source)); found != m_documents.end()) {
            return Result<SourceDocument>(found->second);
        }
    }
    return m_fallback.load(source);
}

Result<SourceId> OverlaySourceProvider::resolve_import(
    const SourceId& root,
    const SourceId& importer,
    const std::string_view import_path) {
    auto resolved = m_fallback.resolve_import(root, importer, import_path);
    if (!resolved.is_error()) return resolved;

    const std::filesystem::path relative(import_path);
    const auto base = import_path.starts_with("./")
        ? std::filesystem::path(root.value).parent_path()
        : std::filesystem::path(importer.value).parent_path();
    const auto candidate = FileSystemSourceProvider::normalize(
        relative.is_absolute() ? relative.string() : (base / relative).string());
    {
        std::shared_lock lock(m_mutex);
        if (m_documents.contains(candidate.value)) return Result<SourceId>(candidate);
    }
    return resolved;
}
