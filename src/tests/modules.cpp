#include "tests.hpp"

//TODO: logs on Error should be caught as exceptions in tests

//#define ENABLE_FAILING_TESTS

#if SIGNALS_HAS_X11 //FIXME:we should isolate tests using X11 more precisely
#include "modules_generator.cpp"
#include "modules_player.cpp"
#include "modules_render.cpp"
#endif /*SIGNALS_HAS_X11*/

#include "modules_simple.cpp"
#include "modules_converter.cpp"
#include "modules_decode.cpp"
#include "modules_demux.cpp"
#include "modules_encoder.cpp"
#include "modules_erasure.cpp"
#include "modules_metadata.cpp"
#include "modules_mux.cpp"
#include "modules_rectifier.cpp"
#include "modules_streamer.cpp"
#include "modules_timings.cpp"
#include "modules_transcoder.cpp"
