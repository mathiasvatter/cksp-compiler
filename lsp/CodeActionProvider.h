#pragma once

#include "DiagnosticPublisher.h"

#include "../JSON/ast/JSONValue.h"

/**
 * Converts self-contained diagnostic fix data into LSP code actions.
 *
 * The provider deliberately does not know individual compiler fix kinds. Adding
 * another fix only requires the diagnostic producer to describe its title and edit.
 *
 * Quick fixes are built from the diagnostics the request carries, one action each. The
 * `source.fixAll` action instead reads every diagnostic published for the document, because a
 * client sends only those intersecting the requested range and that range is not the whole
 * document for every trigger.
 */
class CodeActionProvider {
public:
	[[nodiscard]] static JSONArray provide(
		const JSONObject* params,
		const DiagnosticPublisher& publisher
	);
};
