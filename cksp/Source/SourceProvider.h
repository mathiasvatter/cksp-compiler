#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include "../../misc/Result.h"

/** Stable identity of one compiler input.
 *
 * The first implementation uses normalized absolute paths. Wrapping the value keeps
 * filesystem paths out of the compiler API and leaves room for URI-based sources later.
 */
struct SourceId {
    std::string value;

    SourceId() = default;
    explicit SourceId(std::string value) : value(std::move(value)) {}

    [[nodiscard]] bool empty() const noexcept { return value.empty(); }
    friend bool operator==(const SourceId&, const SourceId&) = default;
};

[[nodiscard]] SourceId source_from_uri(std::string_view uri);
[[nodiscard]] std::string uri_from_source(const SourceId& source);

/** Immutable source snapshot consumed by one compilation. */
struct SourceDocument {
    SourceId id;
    std::shared_ptr<const std::string> text;
    std::optional<int64_t> version;
};

/** Provides source text and import resolution independently of its physical storage. */
class SourceProvider {
public:
    virtual ~SourceProvider() = default;

    virtual Result<SourceDocument> load(const SourceId& source) = 0;

    virtual Result<SourceId> resolve_import(
        const SourceId& root,
        const SourceId& importer,
        std::string_view import_path) = 0;
};

/** Source provider used by the command-line compiler. */
class FileSystemSourceProvider final : public SourceProvider {
public:
    Result<SourceDocument> load(const SourceId& source) override;
    Result<SourceId> resolve_import(
        const SourceId& root,
        const SourceId& importer,
        std::string_view import_path) override;

    [[nodiscard]] static SourceId normalize(std::string_view path);
};

/**
 * In-memory overlay for language-server documents.
 *
 * Open document snapshots take precedence over the fallback provider. Imports that are
 * not open in the editor continue to resolve through the filesystem provider.
 */
class OverlaySourceProvider final : public SourceProvider {
public:
    explicit OverlaySourceProvider(SourceProvider& fallback) : m_fallback(fallback) {}

    void update(SourceId source, std::string text, int64_t version);
    void close(const SourceId& source);

    Result<SourceDocument> load(const SourceId& source) override;
    Result<SourceId> resolve_import(
        const SourceId& root,
        const SourceId& importer,
        std::string_view import_path) override;

private:
    [[nodiscard]] static std::string key(const SourceId& source);

    SourceProvider& m_fallback;
    mutable std::shared_mutex m_mutex;
    std::unordered_map<std::string, SourceDocument> m_documents;
};
