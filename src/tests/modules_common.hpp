#pragma once

#include "lib_modules/modules.hpp"
#include <list>
#include <iostream> // std::cout

namespace {

struct Meta {
	bool operator==(const Meta& rhs) const {
		return this->filename == rhs.filename
			&& this->mimeType == rhs.mimeType
			&& this->codecName == rhs.codecName
			&& this->durationIn180k == rhs.durationIn180k
			//FIXME: sizes are sometimes different with FFmpeg on different platforms: && this->filesize == rhs.filesize
			&& this->latencyIn180k == rhs.latencyIn180k
			&& this->startsWithRAP == rhs.startsWithRAP
			&& this->eos == rhs.eos;
	}
	size_t internalTestIdx;
	std::string filename, mimeType, codecName;
	uint64_t durationIn180k, filesize, latencyIn180k;
	bool startsWithRAP, eos;
};

struct Listener : public Modules::ModuleS {
	Listener(size_t testIdx) : internalTestIdx(testIdx) {
		addInput(new Modules::Input<Modules::DataBase>(this));
	}
	void process(Modules::Data data) override {
		auto const &m = safe_cast<const Modules::MetadataFile>(data->getMetadata());
		results.push_back({ internalTestIdx, m->getFilename(), m->getMimeType(), m->getCodecName(),
			m->getDuration(), m->getSize(), m->getLatency(), m->getStartsWithRAP(), m->getEOS() });
	}
	void print() { //used for generating reference results
		for (auto &r : results) {
			std::cout << "{ " << r.internalTestIdx << ", \"" << r.filename << "\", \"" << r.mimeType << "\", \"" << r.codecName << "\", " << r.durationIn180k << ", " << r.filesize << ", " << r.latencyIn180k << ", " << r.startsWithRAP << ", " << r.eos << " }," << std::endl;
		}
	}

	std::list<Meta> results;
	size_t internalTestIdx;
};

}
