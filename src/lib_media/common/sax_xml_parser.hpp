#pragma once

#include "lib_modules/core/buffer.hpp" // span
#include <functional>
#include <map>
#include <string>
#include <stdexcept>
#include <cassert>

typedef void NodeStartFunc(std::string, std::map<std::string, std::string>&);
typedef void NodeEndFunc(std::string);

static
void saxParse(span<const char> input, std::function<NodeStartFunc> onNodeStart, std::function<NodeEndFunc> onNodeEnd) {
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
		while(isalnum(front()) || front() == ':' || front() == '_' || front() == '-') {
			r += front();
			input += 1;
		}
		return r;
	};

	auto parseNextTag = [&]() {
		while(input.len && front() != '<' && front() != '/')
			input += 1;
		if(input.len == 0)
			return;
		if(accept('<')) {
			if(accept('?')) {
				// XML stuff
			} else if(accept('!')) {
				// comment
			} else if(accept('/')) {
				// closing tag
				auto id = parseIdentifier();
				onNodeEnd(id);
			} else {
				// opening tag
				auto id = parseIdentifier();
				skipSpaces();

				map<string, string> attr;

				while(front() != '>' && front() != '/' && front() != -1) {
					auto name = parseIdentifier();
					if(name.empty()) {
						string msg = "expected an XML attribute, got '";
						msg += front();
						msg += "'";
						throw runtime_error(msg);
					}
					skipSpaces();
					if(accept('='))
						attr[name] = parseString();
					skipSpaces();
				}

				onNodeStart(id, attr);
			}
		} else if(accept('/')) {
			if(accept('>')) {
				// closing tag
				auto id = parseIdentifier();
				onNodeEnd(id);
			}
		} else
			assert(0); //char is dispatched but not processed: this is likely a programming error
	};

	while(input.len)
		parseNextTag();
}


