#pragma once

#include "lib_modules/core/buffer.hpp" // span
#include <functional>
#include <map>
#include <string>
#include <stdexcept>

typedef void NodeStartFunc(std::string, std::map<std::string, std::string>&);

void saxParse(span<const char> input, std::function<NodeStartFunc> onNodeStart) {
	using namespace std;

	auto front = [&]() -> int {
		if(input.len == 0)
			throw runtime_error("Unexpected end of file");

		return input[0];
	};

	auto accept = [&](char c) {
		if(c != front())
			return false;

		input += 1;
		return true;
	};

	auto skipSpaces = [&]() {
		while(isspace(front()))
			input += 1;
	};

	auto parseString = [&]() {
		string r;
		accept('"');
		while(front() != '"') {
			r += front();
			input += 1;
		}
		accept('"');
		return r;
	};

	auto parseIdentifier = [&]() {
		string r;
		skipSpaces();
		while(isalnum(front()) || front() == '_' || front() == '-') {
			r += front();
			input += 1;
		}
		return r;
	};

	auto parseNextTag = [&]() {
		while(input.len && front() != '<')
			input += 1;
		if(input.len == 0)
			return;
		if(accept('<')) {
			if(accept('?')) {
				// XML stuff
			} else if(accept('/')) {
				// closing tag
				parseIdentifier();
			} else {
				// opening tag
				auto id = parseIdentifier();
				skipSpaces();

				map<string, string> attr;

				while(front() != '>' && front() != '/' && front() != -1) {
					auto name = parseIdentifier();
					skipSpaces();
					if(accept('='))
						attr[name] = parseString();
				}

				onNodeStart(id, attr);
			}
		}
	};

	while(input.len)
		parseNextTag();
}


