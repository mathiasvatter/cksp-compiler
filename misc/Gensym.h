//
// Created by Mathias Vatter on 10.05.24.
//

#pragma once

#include <unordered_set>
#include <string>

class Gensym {
private:
	std::unordered_map<std::string, int> m_counters; // Map für Zähler und existierende Namen
public:
	explicit Gensym() = default;
	~Gensym() = default;

	// Existierende Namen manuell hinzufügen
	inline void ingest(const std::string& name) {
		m_counters[name] = 0;
	}

	// Zurücksetzen aller Daten
	inline void refresh() {
		m_counters.clear();
	}

	// Generiert einen neuen eindeutigen Namen
	inline std::string fresh(const std::string& var_name) {
		int& counter = m_counters[var_name]; // Hole oder initialisiere den Zähler
		std::string new_name;

		do {
			new_name = var_name + std::to_string(counter);
			counter++; // Zähler hochzählen
		} while (m_counters.find(new_name) != m_counters.end());

		m_counters[new_name] = 0;
		return new_name;
	}
};