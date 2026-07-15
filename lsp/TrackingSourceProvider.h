#pragma once

#include "ReferenceProvider.h"
#include "../cksp/Source/SourceProvider.h"

/**
 * Records the immutable documents actually read by one compiler run.
 *
 * Wraps the server's overlay provider for the duration of a single analysis: every document
 * the compiler loads is captured, so a successful reference snapshot corresponds exactly to
 * the source text that produced it, including in-memory editor overlays. ReferenceProvider
 * later compares these contents against the live buffers before serving answers that would
 * be wrong on stale positions.
 */
class TrackingSourceProvider final : public SourceProvider {
	SourceProvider& m_delegate;
	ReferenceProvider::SourceContents m_loaded_contents;

public:
	explicit TrackingSourceProvider(SourceProvider& delegate) : m_delegate(delegate) {}

	Result<SourceDocument> load(const SourceId& source) override {
		auto result = m_delegate.load(source);
		if (!result.is_error()) {
			const auto& document = result.unwrap();
			m_loaded_contents.insert_or_assign(
				FileSystemSourceProvider::normalize(document.id.value).value,
				document.text);
		}
		return result;
	}

	Result<SourceId> resolve_import(
		const SourceId& root,
		const SourceId& importer,
		const std::string_view import_path) override {
		return m_delegate.resolve_import(root, importer, import_path);
	}

	/// Hands out the recorded contents; meant to be called once after the analysis.
	[[nodiscard]] ReferenceProvider::SourceContents take_loaded_contents() {
		return std::move(m_loaded_contents);
	}
};
