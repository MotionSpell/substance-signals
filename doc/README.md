<!-- START doctoc generated TOC please keep comment here to allow auto update -->
<!-- DON'T EDIT THIS SECTION, INSTEAD RE-RUN doctoc TO UPDATE -->
**Table of Contents**  *generated with [DocToc](https://github.com/thlorenz/doctoc)*

- [Coding consideration](#coding-consideration)
- [Design](#design)
- [Write an application](#write-an-application)
- [Write a module](#write-a-module)
- [Internals](#internals)
- [Debugging](#debugging)
- [Tests](#tests)
- [Generating this doc](#generating-this-doc)
- [Contributing](#contributing)

<!-- END doctoc generated TOC please keep comment here to allow auto update -->

Coding consideration
====================

You need a C++14 compiler (MSVC2015+, gcc 7+, clang 3.1+).

```ccache``` on unix will considerably accelerate rebuilds: consider aliasing your CXX (e.g. ```CXX="ccache g++"```).

Before committing please execute tests (check.sh will reformat, build, and run the tests). Test execution should be fast (less than 10s).

Design
======

Signals is layered (from bottom to top):
- src/lib_utils: some lightweight C++ helpers (strings, containers, log, profiling, clocks, etc.)
- src/lib_signals: an agnostic signal/slot mechanism using C++11. May convey any type of data with any type of messaging.
- src/lib_modules: an agnostic modules system. Uses signals to connect the modules.  Modules are: inputs/outputs, a clock, an allocator, a data/metadata system. Everything can be configured thru templates.
- src/lib_pipeline: a pipeline of modules builder. Doesn't know anything about multimedia.
- src/lib_media/common: multimedia consideration. Defines types for audio and video, and a lot of
- src/lib_media and src/plugins: module implementations based on lib_modules and multimedia (encode/decode, mux/demux, transform, stream, render, etc.), and wrappers (FFmpeg and GPAC).

Write an application
====================

You can create and connect the modules manually or use the Pipeline helper.

Pipeline helper (recommended):
```
	Pipeline p;
	
	//add modules to the pipeline
	DemuxConfig cfg;
	cfg.url = "data/beepbop.mp4";
	auto demux = p.add("LibavDemux", &cfg); // named module
	auto print = p.addModule<Out::Print>(); // class-based module

	//connect all the demux outputs to the printer
	for (int i = 0; i < demux->getNumOutputs(); ++i)
		p.connect(print, i, demux, i);
	
	p.start();
	p.waitForCompletion(); //optional: will wait for the
	                       //end-of-stream and flush all the data
```

Manual connections:
```
	auto demux = createModule<Demux::LibavDemux>("data/beepbop.mp4");
	auto print = createModule<Out::Print>(std::cout);
	for (int i = 0; i < demux->getNumOutputs(); ++i)
		ConnectOutputToInput(demux->getOutput(0), print);
	for(int i=0;i < 100;++i)
		demux->process();
	demux->flush();
	print->flush();
```

You can find more examples in the unit tests.


Write a module
==============

Modules are an aggregate of inputs, outputs, a process(), and an optional flush() functions. Most Module instances derive from the single input ModuleS class:
```
//single input specialized module
class ModuleS : public Module {
	public:
		ModuleS() : input(Module::addInput()) {
		}
		virtual void processOne(Data data) = 0;
		void process() override {
			Data data;
			if(input->tryPop(data))
				processOne(data);
		}

		// prevent derivatives from trying to add inputs
		void addInput(int) {}
	protected:
		KInput* const input;
};
```

Data is reference-counted. It means that you don't need to care about its lifetime because it will be released as soon as no modules use it.

Module calls are thread-safe once running.

To declare an input (more likely in the constructor), call ```addInput()```.

To declare an output, call 
 
Constructor: use hard types or pointer on a configuration classes containing pointers or PODs.

A module should never receive or send a nullptr. nullptr is for signalling the end of the execution.

Modules only deal with media times, not system time.

You can attach attributes to data. Media times are an attribute.

```
struct PrintModule : public ModuleS {
		PrintModule(KHost* host)
			: m_host(host) {
			output = addOutput();
		}

		void processOne(Data in) override {
			if(isDeclaration(in))
				return; // contains metadata but not data

			// ask for a buffer of type DataRaw
			auto out = output->allocData<DataRaw>(in->data().len);

			// copy input attributes (PTS, DTS, sync points, etc.) and metadata
			out->copyAttributes(*in);
			out->setMetadata(in->getMetadata());
			
			// print whatever trace
			m_host->log(Info, format("Received data of size %s.", in->data().len).c_str());
            
			//post the output
			output->post(out);
		}
	private:
		KHost* const m_host;
		OutputDefault* output;
};
```

Internals
=========

Signals is a data-driven framework.
It means that data is the engine of the execution (most frameworks are driven by time).
It means that:
 - Input errors tend to propagate. They may appear in some later modules of your graph or pipeline. You may want to write modules to rectify the signal.
 - Be careful to test each of your modules.
 - A lack of input may stop all outputs and logs.
 - There is no such thing as wall-clock time in the modules: the input data comes with a timestamp. For the rare modules that might need a wall-clock (e.g renderers), a clock abstraction "IClock" is provided, whose implementation is provided by the application.

Modules see the outside world using the K-prefixed classes: KInput, KOutput, KHost. The framework sees the I-prefixed classes.

KHost allows to inject facilities such as logs inside a module. These facilities are injected to keep allocation and deletion under the same runtime when using dynamic libraries.

Errors are handled by throwing ```error(const char *)```.

Optionally modules can implement an end-of stream function called ```flush()```. Note that this is different from the destructor.

How to declare an input: ```addInput()```.

What about dynamic inputs as e.g. required by muxers: derive from the ```ModuleDynI``` class.

How to declare an output: ```addOutput()```.

Modules carry metadata. Metadata is carried forward in the module chain when being transmitted through data. However it is carried backward at connection time i.e. from the input pin of the next module to the output pin of the previous module.

When developping modules, you should not take care of concurrency (threads or mutex). People implementing the executors should.

Signals takes care of the hard part for you. However this is tweakable.

About parallelism, take care that modules may execute blocking calls when trying to get an output buffer (this depends on your allocator policy). This may lead to deadlock if you run on a thread-pool with not enough threads. This may be solved in different ways: 
  - one thread per module (```#define EXECUTOR EXECUTOR_ASYNC_THREAD```,
  - more permissive allocator,
  - blocking calls returns cooperatively to the scheduler.

A source is a module with no input. Sources are enabled until they call ```m_host->activate(false);```.

Take care of the order of destruction of your modules. Data allocators are held by the outputs and may not be destroyed while data is in use. Note that Pipeline handles this for you.

Data size should have a size so that the allocator can recycle.

Data can be safely cast. Instead of using ```std::dynamic_pointer_cast<MetadataType>``` we advise you to use ```safe_cast<MetadataType>``` present in ```lib_utils/tools.hpp```

Debugging
=========

```DEBUG=1 make```.

Tests
=====

Unit tests are auto-detected when put in .cpp files in unittests/ subdirectory.

Generating this doc
===================

Markdown: doctoc for tables of content

Doxygen: http://gpac.github.io/signals/

Contributing
============

Motion Spell is a private company. Contact us at contact@motionspell.com

