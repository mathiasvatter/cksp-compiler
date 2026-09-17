//
// Created by Mathias Vatter on 19.04.24.
//

#include "PathHandler.h"
#include "Diagnostic.h"

PathHandler::PathHandler(Token current_token, std::string current_file, std::string root_directory)
	: m_current_token(std::move(current_token)), m_current_file(std::move(current_file)),
	m_root_directory(std::move(root_directory)) {}

Result<std::string> PathHandler::check_valid_path(const std::string &path) {
	const std::filesystem::path fs_path(path);
	m_error.actual = fs_path.string();

	// std::filesystem::absolute ist nicht nötig, da resolve_path bereits absolute Pfade liefert
	if (std::filesystem::exists(fs_path)) {
		return Result<std::string>(fs_path.string());
	} else {
		m_error.message = "File does not exist at resolved path.";
		m_error.expected = "A valid file path.";
		return Result<std::string>(m_error);
	}
}

Result<std::string> PathHandler::check_valid_output_file(const std::string &absolute_path) {
	std::filesystem::path path(absolute_path);
    m_error.actual = path.string();

	if (!path.is_absolute()) {
		m_error.message = "Given path is not absolute.";
		m_error.expected = "absolute path";
		return Result<std::string>(m_error);
	}

	// Überprüfen, ob der Pfad eine .txt-Dateiendung hat
	if (path.extension() != ".txt") {
		m_error.message = "Given path does not lead to a .txt file.";
		m_error.expected = "*.txt file";
		return Result<std::string>(m_error);
	}

	// Überprüfen, ob der übergeordnete Ordner existiert
	if (!std::filesystem::exists(path.parent_path())) {
		m_error.message = "The parent folder does not exist.";
		m_error.expected = path.parent_path().string();
		return Result<std::string>(m_error);
	}

	// // Datei öffnen und schließen, um sicherzustellen, dass sie existiert
	// std::ofstream outfile(absolute_path, std::ios::app); // Öffnen im Append-Modus, um keine Daten zu überschreiben
	// if (!outfile) {
	// 	m_error.message = "Could not open file.";
	// 	return Result<std::string>(m_error);
	// }
	// outfile.close();

	return Result<std::string>(absolute_path);
}

// Combines standard overlap resolve with 'resolve_overlap' as fallback for sksp path stuff.
Result<std::string> PathHandler::resolve_path(const std::string &import_path) {
	std::filesystem::path rel(import_path);
	m_error.actual = rel.string();

	if (rel.is_absolute()) {
		return Result<std::string>(rel.string());
	}

	const std::filesystem::path base_path = base_directory_for(import_path);

	// 1. try standard path resolution
	std::filesystem::path combined_path = (base_path / rel).lexically_normal();
	if (std::filesystem::exists(combined_path)) {
		return Result<std::string>(std::filesystem::absolute(combined_path).string());
	// if not, try parent path, maybe .txt file does not yet exist but is valid
	} else if(std::filesystem::exists(combined_path.parent_path())) {
		return Result<std::string>(combined_path.string());
	}

	// 2. try fallback 'resolve_overlap' for special cases
	auto overlap_result = resolve_overlap(base_path.string(), rel.string());
	if (!overlap_result.is_error()) {
		std::filesystem::path overlapped_path(overlap_result.unwrap());
		if (std::filesystem::exists(overlapped_path)) {
			return Result<std::string>(std::filesystem::absolute(overlapped_path).string());
		}
	}

	// Wenn beides fehlschlägt, gib einen Fehler zurück.
	// Der Fehlertext sollte den zuerst versuchten Pfad anzeigen.
	m_error.message = "Could not resolve path. File not found.";
	m_error.actual = "Tried path: " + combined_path.string();
	return Result<std::string>(m_error);
}


// Resolves the overlap between a base path and a relative path.
// It compares the components of both paths from the end and the beginning, looking for a common segment.
// If a common segment is found, it merges the remaining components of both paths to form the resolved path.
// If no common segment is found, it returns an error.
Result<std::string> PathHandler::resolve_overlap(const std::string &base_path, const std::string &relative_path) {
	std::filesystem::path base(base_path);
	std::filesystem::path relative(relative_path);

	std::vector<std::filesystem::path> baseParts;
	for (const auto& part : base) {
		baseParts.push_back(part);
	}
	std::vector<std::filesystem::path> relativeParts;
	for (const auto& part : relative) {
		relativeParts.push_back(part);
	}

	while (!baseParts.empty() && !relativeParts.empty() && baseParts.back() != relativeParts.front()) {
		baseParts.pop_back();
	}

	if (baseParts.empty() || relativeParts.empty() || baseParts.back() != relativeParts.front()) {
		m_error.actual = base.string() + ", " + relative.string();
		m_error.message = "Could not find a common path segment to resolve overlap.";
		return Result<std::string>(m_error);
	}

	baseParts.pop_back(); //erase common bit
	std::filesystem::path mergedPath;
	for (const auto& part : baseParts) {
		mergedPath /= part;
	}
	for (const auto& part : relativeParts) {
		mergedPath /= part;
	}
	return Result<std::string>(mergedPath.string());
}

// The base an import without a "./" prefix is resolved against: the folder of the file the
// import is written in. A "./" prefix asks for the project root instead, which is the folder
// of the entry file the compilation started from.
std::filesystem::path PathHandler::base_directory_for(const std::string &import_path) const {
	if (import_path.starts_with("./")) {
		return {m_root_directory};
	}
	return std::filesystem::path(m_current_file).parent_path();
}

Result<std::string> PathHandler::resolve_import_path(const std::string &import_path) {
	if (auto near_importer = resolve_path(import_path); !near_importer.is_error()) {
		// resolve_path also accepts a path whose file does not exist yet - what an output
		// path needs, and never what an import means.
		if (auto existing = check_valid_path(near_importer.unwrap()); !existing.is_error()) {
			return existing;
		}
	}

	const std::filesystem::path relative(import_path);
	const auto from_root = (std::filesystem::path(m_root_directory) / relative).lexically_normal();

	// SublimeKSP resolves every import against the main script's folder, so a ported project
	// imports a sibling of its entry file from any nesting depth. Reached only once the
	// candidate next to the importing file has failed, which keeps that one authoritative.
	if (!relative.is_absolute() && !m_root_directory.empty() && std::filesystem::exists(from_root)) {
		return Result<std::string>(std::filesystem::absolute(from_root).string());
	}

	// Both candidates belong in the message: which one was meant is exactly what the reader
	// has to decide, and neither path is visible in the import statement the error points at.
	const auto near_importer = (base_directory_for(import_path) / relative).lexically_normal();
	m_error.message = "Could not resolve path. File not found.";
	m_error.add_message(near_importer == from_root || relative.is_absolute()
		? "Tried <" + near_importer.string() + ">."
		: "Tried <" + near_importer.string() + "> and <" + from_root.string() + ">.");
	m_error.expected = "valid path";
	m_error.actual = import_path;
	return Result<std::string>(m_error);
}

Result<std::vector<std::string>> PathHandler::get_directory_files(const std::string &directory_path) {
	std::vector<std::string> file_paths;
	std::filesystem::path dir_path(directory_path);
    m_error.actual = dir_path.string();

	// Überprüfen, ob der gegebene Pfad tatsächlich ein Verzeichnis ist
	if (!std::filesystem::is_directory(dir_path)) {
		m_error.message = "Given path is not a directory";
		return Result<std::vector<std::string>>(m_error);
	}

	// Durchlaufen aller Dateien im Verzeichnis und Unterverzeichnissen
	for (const auto& entry : std::filesystem::recursive_directory_iterator(dir_path)) {
		if (!entry.is_directory()) {
			std::filesystem::path file_path = entry.path();
			// Filtern der Dateien, die die Endungen .ksp oder .cksp haben
			if (file_path.extension() == ".ksp" || file_path.extension() == ".cksp") {
				file_paths.push_back(file_path.string());
			}
		}
	}

	return Result<std::vector<std::string>>(file_paths);
}

