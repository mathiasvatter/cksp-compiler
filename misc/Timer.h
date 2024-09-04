//
// Created by Mathias Vatter on 10.05.24.
//

#pragma once

#include <chrono>
#include <string>
#include <unordered_map>
#include <vector>
#include <iostream>

/**
 * @class Timer
 * @brief A class for timing operations.
 *
 * This class provides methods to start and stop timers with unique names, and to report the durations of completed timers.
 * It uses high resolution clock to measure time.
 * @var m_active_timers A map from timer names to their start times. Only active timers (i.e., timers that have been started but not stopped) are stored here.
 * @var m_completed_timers A map from timer names to their durations. Each timer can be started and stopped multiple times, so each name maps to a vector of durations.
 */
class Timer {
private:
	std::unordered_map<std::string, std::chrono::high_resolution_clock::time_point> m_active_timers;
	std::unordered_map<std::string, std::vector<std::chrono::milliseconds>> m_completed_timers;
	std::vector<std::string> m_order_of_completion;
public:
	void start(const std::string& name) {
		// Saves the start time for the new timer with distinct name
		m_active_timers[name] = std::chrono::high_resolution_clock::now();
	}

	void stop(const std::string& name) {
		auto it = m_active_timers.find(name);
		if(it == m_active_timers.end()) {
			std::cerr << "Timer with name " << name << " not found." << std::endl;
			return;
		}
		// saves the end time for the last timer and calculates the duration and saves it
		auto end_time = std::chrono::high_resolution_clock::now();
		auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - it->second);
		m_completed_timers[name].push_back(duration);
		m_active_timers.erase(it);
		m_order_of_completion.push_back(name);
	}

	/**
     * @brief Generates a report of all completed timers.
     *
     * The report includes the name of each timer and its durations in milliseconds.
     * Each timer can have multiple durations if it was started and stopped multiple times.
     * @return A string containing the report.
     */
	[[nodiscard]] std::string report() const {
		std::string result;
		for (const auto& name : m_order_of_completion) {
			result += name + ": ";
			for (const auto& duration : m_completed_timers.at(name)) {
				result += std::to_string(duration.count()) + " ms, ";
			}
			result.pop_back(); // Letztes Komma entfernen
			result.pop_back(); // Leerzeichen entfernen
			result += "\n";
		}
		return result;
	}

	std::string print_timer(const std::string& name) const {
		std::string result;
		result += name + ": ";
		for (const auto& duration : m_completed_timers.at(name)) {
			result += std::to_string(duration.count()) + " ms, ";
		}
		result.pop_back(); // Letztes Komma entfernen
		result.pop_back(); // Leerzeichen entfernen
		return result;
	}
};
