//
// Created by Mathias Vatter on 19.04.24.
//

#include "PathHandler.h"
#include "CompileError.h"

PathHandler::PathHandler(Token current_token, std::string current_file)
	: m_current_token(std::move(current_token)), m_current_file(std::move(current_file)) {}

Result<std::string> PathHandler::check_valid_path(const std::string &path) {
	std::filesystem::path absPath = std::filesystem::absolute(path);
	if (std::filesystem::exists(absPath)) {
		return Result<std::string>(absPath.string());
	} else {
		return Result<std::string>(m_error);
	}
}

Result<std::string> PathHandler::generate_output_file(const std::string &absolute_path) {
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

	// Datei öffnen und schließen, um sicherzustellen, dass sie existiert
	std::ofstream outfile(absolute_path, std::ios::app); // Öffnen im Append-Modus, um keine Daten zu überschreiben
	if (!outfile) {
		m_error.m_message = "Could not open file.";
		return Result<std::string>(m_error);
	}
	outfile.close();

	return Result<std::string>(absolute_path);
}

Result<std::string> PathHandler::resolve_path(const std::string &import_path) {
	std::filesystem::path current_file(m_current_file);
	std::filesystem::path current_base = current_file.parent_path();
	std::filesystem::path rel(import_path);

    m_error.m_got = rel.string();
	// Wenn der Pfad bereits absolut ist gib ihn zurück ohne zu checken ob valid
	if (rel.is_absolute()) {
		return Result<std::string>(rel.string());
	}

	// Andernfalls mache den Pfad absolut in Bezug auf den Basispfad. Lexically normal removes ../
	std::filesystem::path combined = (current_base / rel).lexically_normal();
	std::filesystem::path absPath = std::filesystem::absolute(combined);

	// Überprüfe, ob der OrdnerPfad existiert
	if (is_directory(absPath) and std::filesystem::exists(absPath)) {
		return Result<std::string>(absPath.string());
	} else if(std::filesystem::exists(absPath.parent_path())) {
		return Result<std::string>(absPath.string());
	} else {
		auto new_absPath = resolve_overlap(current_base.string(), rel.string());
		if(new_absPath.is_error()) {
			m_error.m_message = "Could not resolve paths.";
			m_error.m_got = current_base.string() + ", " + rel.string();
			return Result<std::string>(m_error);
		}
		return Result<std::string>(new_absPath.unwrap());
	}
}

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
	// do as long as its
	while (!baseParts.empty() && !relativeParts.empty() && baseParts.back() != relativeParts.front()) {
		baseParts.pop_back();
	}

	// Überprüfen, ob keine Übereinstimmung gefunden wurde
	if (baseParts.empty() || relativeParts.empty() || baseParts.back() != relativeParts.front()) {
		m_error.m_got = base.string() + ", " + relative.string();
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
	auto absolute_path = resolve_path(import_path);
	if(absolute_path.is_error()) {
		return Result<std::string>(absolute_path.get_error());
	}
	auto valid_path = check_valid_path(absolute_path.unwrap());
	if(valid_path.is_error()) {
		return Result<std::string>(valid_path.get_error());
	}
	return Result<std::string>(valid_path.unwrap());
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

