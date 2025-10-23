//
// Created by Mathias Vatter on 19.04.24.
//

#include "PathHandler.h"
#include "CompileError.h"

PathHandler::PathHandler(Token current_token, std::string current_file, std::string root_directory)
	: m_current_token(std::move(current_token)), m_current_file(std::move(current_file)),
	m_root_directory(std::move(root_directory)) {}

Result<std::string> PathHandler::check_valid_path(const std::string &path) {
	const std::filesystem::path fs_path(path);
	m_error.m_got = fs_path.string();

	// std::filesystem::absolute ist nicht nötig, da resolve_path bereits absolute Pfade liefert
	if (std::filesystem::exists(fs_path)) {
		return Result<std::string>(fs_path.string());
	} else {
		m_error.m_message = "File does not exist at resolved path.";
		m_error.m_expected = "A valid file path.";
		return Result<std::string>(m_error);
	}
}

Result<std::string> PathHandler::check_valid_output_file(const std::string &absolute_path) {
	std::filesystem::path path(absolute_path);
    m_error.m_got = path.string();

	if (!path.is_absolute()) {
		m_error.m_message = "Given path is not absolute.";
		m_error.m_expected = "absolute path";
		return Result<std::string>(m_error);
	}

	// Überprüfen, ob der Pfad eine .txt-Dateiendung hat
	if (path.extension() != ".txt") {
		m_error.m_message = "Given path does not lead to a .txt file.";
		m_error.m_expected = "*.txt file";
		return Result<std::string>(m_error);
	}

	// Überprüfen, ob der übergeordnete Ordner existiert
	if (!std::filesystem::exists(path.parent_path())) {
		m_error.m_message = "The parent folder does not exist.";
		m_error.m_expected = path.parent_path().string();
		return Result<std::string>(m_error);
	}

	// // Datei öffnen und schließen, um sicherzustellen, dass sie existiert
	// std::ofstream outfile(absolute_path, std::ios::app); // Öffnen im Append-Modus, um keine Daten zu überschreiben
	// if (!outfile) {
	// 	m_error.m_message = "Could not open file.";
	// 	return Result<std::string>(m_error);
	// }
	// outfile.close();

	return Result<std::string>(absolute_path);
}

// Combines standard overlap resolve with 'resolve_overlap' as fallback for sksp path stuff.
Result<std::string> PathHandler::resolve_path(const std::string &import_path) {
	std::filesystem::path rel(import_path);
	m_error.m_got = rel.string();

	if (rel.is_absolute()) {
		return Result<std::string>(rel.string());
	}

	std::filesystem::path base_path;
	// check if import_path starts with "./" to determine the base path
	if (!import_path.empty() && import_path.rfind("./", 0) == 0) {
		base_path = m_root_directory;
	} else {
		std::filesystem::path current_file(m_current_file);
		base_path = current_file.parent_path();
	}

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
	m_error.m_message = "Could not resolve path. File not found.";
	m_error.m_got = "Tried path: " + combined_path.string();
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
		m_error.m_got = base.string() + ", " + relative.string();
		m_error.m_message = "Could not find a common path segment to resolve overlap.";
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

Result<std::string> PathHandler::resolve_import_path(const std::string &import_path) {
	auto absolute_path_result = resolve_path(import_path);
	if(absolute_path_result.is_error()) {
		return absolute_path_result;
	}

	// check_valid_path prüft, ob die aufgelöste Datei tatsächlich existiert.
	return check_valid_path(absolute_path_result.unwrap());
}

Result<std::vector<std::string>> PathHandler::get_directory_files(const std::string &directory_path) {
	std::vector<std::string> file_paths;
	std::filesystem::path dir_path(directory_path);
    m_error.m_got = dir_path.string();

	// Überprüfen, ob der gegebene Pfad tatsächlich ein Verzeichnis ist
	if (!std::filesystem::is_directory(dir_path)) {
		m_error.m_message = "Given path is not a directory";
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

