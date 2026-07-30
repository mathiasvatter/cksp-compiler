#pragma once

#include "../JSON/ast/JSONValue.h"

/**
 * Converts self-contained diagnostic fix data into LSP quick-fix actions.
 *
 * The provider deliberately does not know individual compiler fix kinds. Adding
 * another fix only requires the diagnostic producer to describe its title and edit.
 */
class CodeActionProvider {
public:
	[[nodiscard]] static JSONArray provide(const JSONObject* params);
};
