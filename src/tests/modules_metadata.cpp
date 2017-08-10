#include "tests.hpp"
#include "lib_modules/modules.hpp"
#include "lib_media/common/pcm.hpp"

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
		inputs[0]->updateMetadata(inputs[0]->pop());
	}

	void setMetadata(std::shared_ptr<const IMetadata> metadata) {
		input->setMetadata(metadata);
	}

private:
	IInput *input;
};

unittest("metadata: backwarded by connection") {
	auto output = uptr(create<FakeOutput>());
	auto input  = uptr(create<FakeInput>());
	input->setMetadata(shptr(new MetadataRawAudio));
	auto o = output->getOutput(0);
	auto i = input->getInput(0);
	ConnectOutputToInput(o, i);
	ASSERT(o->getMetadata() == i->getMetadata());
}

unittest("metadata: not forwarded by connection") {
	auto output = uptr(create<FakeOutput>());
	auto input = uptr(create<FakeInput>());
	output->setMetadata(shptr(new MetadataRawAudio));
	auto o = output->getOutput(0);
	auto i = input->getInput(0);
	ConnectOutputToInput(o, i);
	ASSERT(!i->getMetadata());
}

unittest("metadata: same back and fwd by connection") {
	auto output = uptr(create<FakeOutput>());
	auto input = uptr(create<FakeInput>());
	auto meta = shptr(new MetadataRawAudio);
	input->setMetadata(meta);
	output->setMetadata(meta);
	auto o = output->getOutput(0);
	auto i = input->getInput(0);
	ConnectOutputToInput(o, i);
	ASSERT(o->getMetadata() == i->getMetadata());
}

unittest("metadata: forwarded by data") {
	auto output = uptr(create<FakeOutput>());
	auto input = uptr(create<FakeInput>());
	auto o = output->getOutput(0);
	auto i = input->getInput(0);
	ConnectOutputToInput(o, i);
	output->setMetadata(shptr(new MetadataRawAudio));
	output->process();
	ASSERT(o->getMetadata() == i->getMetadata());
}

unittest("metadata: incompatible by data") {
	bool thrown = false;
	try {
		auto output = uptr(create<FakeOutput>());
		auto input = uptr(create<FakeInput>());
		input->setMetadata(shptr(new MetadataRawAudio));
		auto o = output->getOutput(0);
		auto i = input->getInput(0);
		ConnectOutputToInput(o, i);
		output->setMetadata(shptr(new MetadataRawVideo));
		output->process();
		ASSERT(o->getMetadata() == i->getMetadata());
	} catch (std::exception const& e) {
		std::cerr << "Expected error: " << e.what() << std::endl;
		thrown = true;
	}
	ASSERT(thrown);
}

unittest("metadata: incompatible back and fwd") {
	bool thrown = false;
	try {
		auto output = uptr(create<FakeOutput>());
		auto input = uptr(create<FakeInput>());
		input->setMetadata(shptr(new MetadataRawAudio));
		output->setMetadata(shptr(new MetadataRawVideo));
		auto o = output->getOutput(0);
		auto i = input->getInput(0);
		ConnectOutputToInput(o, i);
		output->process();
		ASSERT(o->getMetadata() == i->getMetadata());
	} catch (std::exception const& e) {
		std::cerr << "Expected error: " << e.what() << std::endl;
		thrown = true;
	}
	ASSERT(thrown);
}

unittest("metadata: updated twice by data") {
	auto output = uptr(create<FakeOutput>());
	auto input = uptr(create<FakeInput>());
	auto o = output->getOutput(0);
	auto i = input->getInput(0);
	ConnectOutputToInput(o, i);
	output->setMetadata(shptr(new MetadataRawAudio));
	output->process();
	output->setMetadata(shptr(new MetadataRawAudio));
	output->process();
	ASSERT(o->getMetadata() == i->getMetadata());
}

}