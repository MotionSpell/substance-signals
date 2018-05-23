<!-- START doctoc generated TOC please keep comment here to allow auto update -->
<!-- DON'T EDIT THIS SECTION, INSTEAD RE-RUN doctoc TO UPDATE -->
**Table of Contents**  *generated with [DocToc](https://github.com/thlorenz/doctoc)*

- [Coding consideration](#coding-consideration)
- [Design](#design)
- [Write an application](#write-an-application)
- [Write a module](#write-a-module)
- [Data and metadata](#data-and-metadata)
- [Debugging](#debugging)
- [Technical considerations](#technical-considerations)
- [Contributing](#contributing)

<!-- END doctoc generated TOC please keep comment here to allow auto update -->

Coding consideration
====================

You need a C++11 compiler (MSVC2013+, gcc 4.9+, clang 3.1+).

```ccache``` on unix can accelerate rebuilds: consider aliasing your CXX (e.g. ```CXX="ccache g++"```).

Before committing please execute tests (check.sh will reformat, build, and make tests for you). Test execution should be fast (less than 30s).

Design
======

Signals is layered (from bottom to top):
- utils: some light C++ helpers (strings, containers, log, profiling, clocks, etc.) and wrappers (including for FFmpeg and GPAC).
- signals: an agnostic signal/slot mechanism using C++11. May convey any type of data with any type of messaging.
- modules: an agnostic modules system. Uses signals to connect the modules.  Modules are: inputs/outputs, a clock, an allocator, a data/metadata system. Everything can be configured thru templates.
- pipeline: a pipeline of modules builder. Doesn't know anything about multimedia.
- mm (currently in media/common): multimedia consideration. Defines types for audio and video, and a lot of
- media: module implementations based on libmodules and mm (encode/decode, mux/demux, transform, stream, render, etc.).

Write an application
====================

You can connect the modules manually or use the Pipeline helper. You can find some examples in the tests.

Pipeline helper (recommended):
```
	Pipeline p;
	
	//add modules to the pipeline
	auto demux = p.addModule<Demux::LibavDemux>("data/beepbop.mp4");
	auto print = p.addModule<Out::Print>();
	for (int i = 0; i < (int)demux->getNumOutputs(); ++i) {
		p.connect(print, i, demux, i);
	}
	
	p.start();
	p.waitForCompletion();
```

Manual connections:
```
	auto demux = uptr(create<Demux::LibavDemux>("data/beepbop.mp4"));
	auto print = uptr(create<Out::Print>(std::cout));
	for (int i = 0; i < (int)demux->getNumOutputs(); ++i) {	
		ConnectOutputToInput(demux->getOutput(0), print);
	}
	demux->process(nullptr);
	demux->flush();
	print->flush();
```

Internals
=========

Signals is a data-driven framework.
It means that data is the engine of the execution (most frameworks are driven by time).
It means that:
 - Input errors tend to propagate. They may appear in some later modules of your graph or pipeline.
   Be careful to test each of your modules. You may want to write modules to rectify the signal.
 - A lack of input may stop all outputs and logs.
 - There is no such thing as real-time in the modules.
   There is a clock abstraction, and the application or Pipeline level may provide a clock.

```
class Module : public IModule, public ErrorCap, public LogCap, public InputCap {
	public:
		Module() = default;
		virtual ~Module() noexcept(false) {}

	private:
		Module(Module const&) = delete;
		Module const& operator=(Module const&) = delete;
};
```

with ErrorCap allowing in modules ```error(string);``` to raise an error.

with LogCap:
```
log(Warning, "Stream contains an irrecoverable error - leaving");
```

with InputCap:
```
	size_t getNumInputs() const;
	IInput* getInput(size_t i);
	IInput* addInput(IInput* p);

```

with IModule:
```
class IModule : public IProcessor, public virtual IInputCap, public virtual IOutputCap {
public:
	virtual ~IModule() noexcept(false) {}
	virtual void flush() {}
};
```

with IModule:
```
	virtual void process() = 0;
```

with IOutputCap:
```
	size_t getNumOutputs() const;
	IOutput* getOutput(size_t i) const;
```
and an output factory:
```
template <typename InstanceType, typename ...Args>
	InstanceType* addOutput(Args&&... args);
```

Optional:
flush()

How to declare an input:

What about dynamic inputs as e.g. required by muxers:

How to declare an output:


ADD DOXYGEN

When developping modules, you should not take care of concurrency (threads or mutex).
Signals takes care of the hard part for you. However this is tweakable.
About parallelism, take care that modules may execute blocking calls
when trying to get an output buffer (this depends on your allocator policy).
This may lead to deadlock if you run on a thread-pool with not enough threads.
This may be solved in different ways: 
  - one thread per module (```#define EXECUTOR EXECUTOR_ASYNC_THREAD``` in modules/utils/pipeline.cpp)
  - more permissive allocator 
  - blocking calls returns cooperatively to the scheduler, see #14.

Use the Pipeline namespace.

Source called only once. Stops when given nullptr on "virtual" input pin 0.

ORDER OF ALLOC

Interrupting a 


Write a module
==============

Use the Modules namespace.

Modules have an easy interface: just implement ```process(Data data)```.

You may want

Data is reference-counted. It means that you don't need to care about its lifetime because it will be released as soon as no modules use it.

To declare an input (more likely in the constructor), call ```addInput()```:
```
addInput(new Input<DataBase>(this));
```
The type of the input (here DataBase) can be:
 - DataLoose when the data type is not known (necessary for dynamic inputs (e.g. Muxers) but lazy-typing has some drawbacks.
 - DataRaw: raw data.
 - DataPcm: raw audio specialization.
 - DataPicture: raw video specialization.
 - PictureYUV420P: picture specialization for YUV420P colospace.
 - PictureYUYV422: picture specialization for YUYV422 colospace.
 - PictureNV12: picture specialization for NV12 colospace.
 - PictureRGB24: picture specialization for RGB24 colospace.
 - DataAVPacket: libav packet wrapper.
 
Constructor: use hard types.
flush() = EOS
A module should never receive or send a nullptr. nullptr is for signalling the end of the execution.
Multimedia: DTS for encoded stuff and PTS for decoded? (should always be in order)

* Inputs

In the pipeline, input modules will be called two times on void ::process(Data data): once at the beginning, and one when asked to exit on each input pin. Therefore the structure should be:
```
void module::process(Data data) {
	while (getInput(0)->tryPop(data)) {
		auto out = outputs[0]->getBuffer(0);
		[do my stuff here]
		out->setTime(time);
		output->emit(out);
	}
}
```

TODO: metadata don't propagate to the connected input yet. This easily allows modules to catch the updates.
TODO: add a test framework for modules (to prove they behave as expected by the API).

If your last input pin has the type DataLoose, you should ignore it. It means it is a fake input pin for:
 - Dynamic input pins (e.g. muxers).
 - Sources which need to receive input null data.

TODO

* Allocator
MAKE A LIST OF BASIC OBJECTS => link to doxygen
* Data: shared_ptr
* Output
...


Data and metadata
=================

Data size should have a size so that the allocator can recycle => TODO: return a Size class that can be compared or properties (resizable etc.), see #17 and allocator.hpp
ATM we set isRecyclable(). Set it to false if you make ```new``` in your allocator: the destructor will be called.


```
auto const metadata = data->getMetadata();
	if (auto video = std::dynamic_pointer_cast<const MetadataPktLibavVideo>(metadata)) {
		declareStreamVideo(video);
	} else if (auto audio = std::dynamic_pointer_cast<const MetadataPktLibavAudio>(metadata)) {
		declareStreamAudio(audio);
```

Debugging
=========

Mono-thread.


Tests
=====

Tests can also be written within .cpp implementation.


Technical considerations
========================

* Parallelism

With Pipeline, automatic over a thread pool with an ID preventing parallel execution on the same module.

only use a thread if you need the system clock
a clock can be injected in modules

* Data types

* metadata


Generating this doc
===================

Markdown: doctoc for tables of content

Doxygen: http://gpac.github.io/signals/

Contributing
============

TODO
Github, pull requests.

