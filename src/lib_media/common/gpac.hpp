#pragma once

#include "lib_modules/core/data.hpp"

namespace Modules {

class DataRawGPAC : public DataRaw {
	public:
		/*takes ownership*/
		DataRawGPAC(size_t /*size*/) : DataRaw(0), buffer(nullptr), bufferSize(0) {
		}
		~DataRawGPAC();
		void setData(uint8_t *buffer, const size_t size) {
			this->buffer = buffer;
			bufferSize = size;
		}
		uint8_t* data() override {
			throw std::runtime_error("DataRawGPAC: forbidden operation non-const data().");
		}
		bool isRecyclable() const override {
			return false;
		}
		const uint8_t* data() const override {
			return buffer;
		}
		uint64_t size() const override {
			return bufferSize;
		}
		void resize(size_t /*size*/) override {
			throw std::runtime_error("DataRawGPAC: forbidden operation resize().");
		}

	private:
		uint8_t *buffer;
		size_t bufferSize;
};

}
