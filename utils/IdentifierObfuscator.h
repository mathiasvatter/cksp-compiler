//
// Created by Mathias Vatter on 13.07.26.
//

#pragma once

#include <random>
#include <string>
#include <unordered_set>

class IdentifierObfuscator {
public:
	explicit IdentifierObfuscator(uint32_t seed = 0xC0FFEE)
		: rng(seed) {}

	std::string next() {
		while (true) {
			std::string id = generate(8);

			// Avoid duplicates and keywords.
			if (used.insert(id).second)
				return id;
		}
	}

private:
	static constexpr char alphabet[] =
		"abcdefghijklmnopqrstuvwxyz";
		// Kontakt is case-insensitive -> // "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

	std::mt19937 rng;
	std::unordered_set<std::string> used;

	std::string generate(std::size_t length) {
		std::uniform_int_distribution<std::size_t> dist(
			0, sizeof(alphabet) - 2);

		std::string result;
		result.reserve(length);

		// First character must not be a digit.
		result.push_back(alphabet[dist(rng) % 52]);

		for (std::size_t i = 1; i < length; ++i)
			result.push_back(alphabet[dist(rng)]);

		return result;
	}
};