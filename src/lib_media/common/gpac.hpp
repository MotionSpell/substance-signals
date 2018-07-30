#pragma once

#include "lib_modules/core/database.hpp"

namespace Modules {

class DataRawGPAC : public DataBase {
	public:
		/*takes ownership*/
		DataRawGPAC(size_t /*size*/) {
		}
		~DataRawGPAC();
		void setData(uint8_t *buffer, const size_t size) {
			this->buffer = buffer;
			bufferSize = size;
		}
		Span data() override {
			return { buffer, bufferSize };
		}
		bool isRecyclable() const override {
			return false;
		}
		SpanC data() const override {
			return { buffer, bufferSize };
		}
		void resize(size_t /*size*/) override {
			throw std::runtime_error("DataRawGPAC: forbidden operation resize().");
		}

	private:
		uint8_t *buffer = nullptr;
		size_t bufferSize = 0;
};

}
