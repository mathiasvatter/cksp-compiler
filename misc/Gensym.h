//
// Created by Mathias Vatter on 10.05.24.
//

#pragma once

#include <set>
#include <string>

class Gensym {
private:
	std::set<std::string> m_names{};
	int m_counter = 0;
public:
	explicit Gensym() : m_counter(0) {}
	~Gensym() = default;

	inline void ingest(const std::string& name) {
		m_names.insert(name);
	}

	inline void refresh() {
		m_names.clear();
		m_counter = 0;
	}

	inline std::string fresh(const std::string &var_name) {
		std::string new_name = var_name;
		while(m_names.find(new_name) != m_names.end()) {
			new_name = var_name + std::to_string(m_counter);
			m_counter++;
		}
		m_names.insert(new_name);
		return new_name;
	}
};