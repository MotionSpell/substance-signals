#include "tests/tests.hpp"
#include "lib_modules/modules.hpp"
#include "lib_media/common/pcm.hpp"
#include "lib_media/common/metadata.hpp"
#include "lib_utils/resolution.hpp"
#include <iostream>

using namespace Tests;
using namespace Modules;

namespace {
class FakeOutput : public Module {
	public:
		FakeOutput() {
			output = addOutput<OutputPcm>();
		}
		void process() {
			auto data = output->getBuffer(0);
			output->emit(data);
		}
		void setMetadata(std::shared_ptr<const IMetadata> metadata) {
			output->setMetadata(metadata);
		}

	private:
		OutputPcm *output;
};

class FakeInput : public Module {
	public:
		FakeInput() {
			input = addInput(new Input<DataBase>(this));
		}
		void process() {
			auto data = inputs[0]->pop();
			inputs[0]->updateMetadata(data);
		}

		void setMetadata(std::shared_ptr<const IMetadata> metadata) {
			input->setMetadata(metadata);
		}

	private:
		IInput *input;
};
}

unittest("metadata: backwarded by connection") {
	auto output = create<FakeOutput>();
	auto input  = create<FakeInput>();
	input->setMetadata(make_shared<MetadataRawAudio>());
	auto o = output->getOutput(0);
	auto i = input->getInput(0);
	ConnectOutputToInput(o, i);
	ASSERT(o->getMetadata() == i->getMetadata());
}

unittest("metadata: not forwarded by connection") {
	auto output = create<FakeOutput>();
	auto input = create<FakeInput>();
	output->setMetadata(make_shared<MetadataRawAudio>());
	auto o = output->getOutput(0);
	auto i = input->getInput(0);
	ConnectOutputToInput(o, i);
	ASSERT(!i->getMetadata());
}

unittest("metadata: same back and fwd by connection") {
	auto output = create<FakeOutput>();
	auto input = create<FakeInput>();
	auto meta = make_shared<MetadataRawAudio>();
	input->setMetadata(meta);
	output->setMetadata(meta);
	auto o = output->getOutput(0);
	auto i = input->getInput(0);
	ConnectOutputToInput(o, i);
	ASSERT(o->getMetadata() == i->getMetadata());
}

unittest("metadata: forwarded by data") {
	auto output = create<FakeOutput>();
	auto input = create<FakeInput>();
	auto o = output->getOutput(0);
	auto i = input->getInput(0);
	ConnectOutputToInput(o, i);
	output->setMetadata(make_shared<MetadataRawAudio>());
	output->process();
	ASSERT(o->getMetadata() == i->getMetadata());
}

unittest("metadata: incompatible by data") {
	bool thrown = false;
	auto output = create<FakeOutput>();
	auto input = create<FakeInput>();
	input->setMetadata(make_shared<MetadataRawAudio>());
	auto o = output->getOutput(0);
	auto i = input->getInput(0);
	ConnectOutputToInput(o, i);
	try {
		output->setMetadata(make_shared<MetadataRawVideo>());
	} catch (std::exception const& e) {
		std::cerr << "Expected error: " << e.what() << std::endl;
		thrown = true;
	}
	ASSERT(thrown);
}

unittest("metadata: incompatible back and fwd") {
	bool thrown = false;
	auto output = create<FakeOutput>();
	auto input = create<FakeInput>();
	input->setMetadata(make_shared<MetadataRawAudio>());
	output->setMetadata(make_shared<MetadataRawVideo>());
	auto o = output->getOutput(0);
	auto i = input->getInput(0);
	try {
		ConnectOutputToInput(o, i);
	} catch (std::exception const& e) {
		std::cerr << "Expected error: " << e.what() << std::endl;
		thrown = true;
	}
	ASSERT(thrown);
}

unittest("metadata: updated twice by data") {
	auto output = create<FakeOutput>();
	auto input = create<FakeInput>();
	auto o = output->getOutput(0);
	auto i = input->getInput(0);
	ConnectOutputToInput(o, i);
	output->setMetadata(make_shared<MetadataRawAudio>());
	output->process();
	output->setMetadata(make_shared<MetadataRawAudio>());
	output->process();
	ASSERT(o->getMetadata() == i->getMetadata());
}

unittest("duplicating data") {
	const Resolution res(80, 60);
	auto data = make_shared<DataPcm>(0);

	Data dataCopy = make_shared<DataBaseRef>(data);
	data = nullptr;

	auto dataCopyPcm = safe_cast<const DataPcm>(dataCopy);
	ASSERT(dataCopyPcm);
}
