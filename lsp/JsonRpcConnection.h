//
// Created by Mathias Vatter on 06.07.26.
//

#pragma once
#include <iosfwd>

class JsonRpcConnection {
	std::istream& m_input;
	std::ostream& m_output;
public:
	JsonRpcConnection(std::istream& input, std::ostream& output) : m_input(input), m_output(output) {}

};
