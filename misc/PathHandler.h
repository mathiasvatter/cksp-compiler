//
// Created by Mathias Vatter on 19.04.24.
//

#pragma once

#include "../Tokenizer/Tokenizer.h"


class PathHandler {
public:
	PathHandler(Token current_token, std::string current_file, std::string root_directory);
	~PathHandler() = default;

private:
	Token m_current_token;
    CompileError m_error = CompileError(ErrorType::FileError, "Found incorrect path.", "valid path", m_current_token);
	std::string m_current_file;
	std::string m_root_directory;
public:

	/**
	 * @brief Checks if given file path exists
	 * @param path
	 * @param token
	 * @param curr_file
	 * @return
	 */
	Result<std::string> check_valid_path(const std::string& path);

	/**
	 * @brief Generates an output file with the given path if the path is valid.
	 * @param path
	 * @return
	 */
	Result<std::string> generate_output_file(const std::string& absolute_path);;

	/**
	 * @brief Resolves the import path to an absolute path.
	 *
	 * @param import_path The path provided in the import statement.
	 * @return A Result object containing the resolved path as a string if successful, or a CompileError if unsuccessful.
	 */
	Result<std::string> resolve_import_path(const std::string& import_path);;

	/**
	 * @brief Returns the absolute path of a given path in combination with the current file.
	 * Does not check if the path exists.
	 *
	 * @param import_path The path provided in the import statement.
	 * @param token The token representing the import statement.
	 * @param curr_file The current file that contains the import statement.
	 * @return A Result object containing the resolved path as a string if successful, or a CompileError if unsuccessful.
	 */
	Result<std::string> resolve_path(const std::string& import_path);

	/**
	 * @brief Resolves overlapping paths.
	 *
	 * This function takes a base path and a relative path as input. It splits both paths into their components and compares them from the end and the beginning respectively.
	 * It continues to do so until it finds a common component or until one of the paths is exhausted. It then merges the remaining components of both paths to form the resolved path.
	 *
	 * @param base_path The base path.
	 * @param relative_path The relative path to be resolved.
	 * @return A Result object containing the resolved path as a string if successful, or a CompileError if unsuccessful.
	 */
	Result<std::string> resolve_overlap(const std::string& base_path, const std::string& relative_path);

	/**
	 * @brief Returns a vector of all files in a given directory.
	 *
	 * @param directory_path The path of the directory.
	 * @return A Result object containing the vector of file paths if successful, or a CompileError if unsuccessful.
	 */
	Result<std::vector<std::string>> get_directory_files(const std::string& directory_path);

};

