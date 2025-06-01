//
// Created by Mathias Vatter on 17.11.24.
//

#pragma once

#include <string>
#include <vector>
#include <cmath>
#include <map>
#include <unordered_set>
#include <algorithm>
#include <sstream>

#include "../Tokenizer/Tokens.h"

#include <thread>

template <typename Iterator, typename Func>
void parallel_for_each(Iterator begin, Iterator end, Func func, size_t num_threads = 0) {
	// Bestimme die Anzahl der Threads basierend auf der Hardware
	if (num_threads == 0) {
		num_threads = std::thread::hardware_concurrency();
		if (num_threads == 0) num_threads = 2; // Fallback
	}

	// Gesamtlänge berechnen
	const size_t total_elements = std::distance(begin, end);
	if (total_elements == 0) return;

	const size_t chunk_size = std::ceil(static_cast<double>(total_elements) / num_threads);

	// Threads erstellen
	std::vector<std::thread> threads;
	Iterator chunk_begin = begin;

	for (size_t t = 0; t < num_threads && chunk_begin != end; ++t) {
		Iterator chunk_end = chunk_begin;
		std::advance(chunk_end, std::min(chunk_size, static_cast<size_t>(std::distance(chunk_begin, end))));

		threads.emplace_back([chunk_begin, chunk_end, func]() {
		  for (auto it = chunk_begin; it != chunk_end; ++it) {
			  func(*it);
		  }
		});

		chunk_begin = chunk_end;
	}

	// Alle Threads beenden
	for (auto& thread : threads) {
		if (thread.joinable()) {
			thread.join();
		}
	}
}

/*
 * helper functions to deal with special characters not supported in KSP
 */

inline std::string sanitize_dots(const std::string& str) {
	std::string result = str;
	size_t pos = 0;

	// Ersetze "::" durch "__"
	while ((pos = result.find("::", pos)) != std::string::npos) {
		result.replace(pos, 2, "____");
		pos += 4;  // Gehe zur nächsten Position nach dem Ersetzen
	}

	pos = 0;
	// Ersetze "." durch "__"
	while ((pos = result.find('.', pos)) != std::string::npos) {
		result.replace(pos, 1, "__");
		pos += 2;  // Gehe zur nächsten Position nach dem Ersetzen
	}

	return result;
}

inline std::string desanitize_dots(const std::string& str) {
	std::string result = str;
	size_t pos = 0;

	// Ersetze "____" durch "::"
	while ((pos = result.find("____", pos)) != std::string::npos) {
		result.replace(pos, 4, "::");
		pos += 2;  // Nach dem Ersetzen zur nächsten Position gehen
	}

	// pos = 0;
	// // Ersetze "__" durch "."
	// while ((pos = result.find("__", pos)) != std::string::npos) {
	// 	result.replace(pos, 2, ".");
	// 	pos += 1;  // Nach dem Ersetzen zur nächsten Position gehen
	// }

	return result;
}

/// takes string and returns vector of namespaces ('.')
inline std::vector<std::string> get_namespaces(const std::string& str) {
	std::vector<std::string> namespaces;
	std::istringstream iss(str);
	std::string ns;

	while (std::getline(iss, ns, '.')) {
		namespaces.push_back(ns);
	}
	return namespaces;
}