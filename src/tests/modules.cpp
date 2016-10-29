#include "tests.hpp"
#include "lib_modules/modules.hpp"

//#define ENABLE_FAILING_TESTS

#ifdef SIGNALS_HAS_X11 //FIXME:we should isolate tests using X11 more precisely
#include "modules_generator.cpp"
#include "modules_player.cpp"
#include "modules_render.cpp"
#endif /*SIGNALS_HAS_X11*/

#include "modules_fifo.cpp"
#include "modules_simple.cpp"
#include "modules_clock.cpp"
#include "modules_converter.cpp"
#include "modules_decode.cpp"
#include "modules_demux.cpp"
#include "modules_encoder.cpp"
#include "modules_erasure.cpp"
#include "modules_mux.cpp"
#include "modules_transcoder.cpp"
#include "modules_pipeline.cpp"

using namespace Tests;
