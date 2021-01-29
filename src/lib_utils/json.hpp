#pragma once

#include "lib_utils/small_map.hpp"
#include <string>
#include <vector>

namespace json {

struct Value {
		enum class Type {
			String,
			Object,
			Array,
			Integer,
			Boolean,
		};

		Type type;

		////////////////////////////////////////
		// type == Type::String
		std::string stringValue;

		operator std::string() const {
			enforceType(Type::String);
			return stringValue;
		}

		////////////////////////////////////////
		// type == Type::Object
		SmallMap<std::string, Value> objectValue;

		Value const& operator[] (const char* name) const;

		////////////////////////////////////////
		// type == Type::Array
		std::vector<Value> arrayValue;

		Value const& operator[] (int i) {
			enforceType(Type::Array);
			return arrayValue[i];
		}

		////////////////////////////////////////
		// type == Type::Boolean
		bool boolValue {};

		////////////////////////////////////////
		// type == Type::Integer
		int intValue {};

		operator int() const {
			enforceType(Type::Integer);
			return intValue;
		}

	private:
		void enforceType(Type expected) const;
};

Value parse(const std::string &s);
}

