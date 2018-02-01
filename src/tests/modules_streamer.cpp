#include "tests.hpp"
#include "lib_modules/modules.hpp"
#include <stdexcept>
#include "lib_media/demux/libav_demux.hpp"
#include "lib_media/decode/libav_decode.hpp"
#include "lib_media/encode/libav_encode.hpp"
#include "lib_media/mux/gpac_mux_mp4.hpp"
#include "lib_media/stream/apple_hls.hpp"
#include "lib_media/stream/mpeg_dash.hpp"
#include "lib_media/stream/ms_hss.hpp"
#include "modules_common.hpp"

using namespace Tests;
using namespace Modules;

unittest("adaptive streaming combination coverage") {
	std::vector<Meta> results, ref = {
	//FIXME: latency of 1 and 8 are provoked by wrong 25000/1 timescale retrieved in the MP4 mux
	{ "muxTSSeg_0.ts"                      , ""         , ""           , 360000, 15416, 1     , 0 },
	{ "muxTSSeg_.m3u8"                     , ""         , ""           , 0     , 0    , 1     , 0 },
	{ "hls_ts.m3u8"                        , ""         , ""           , 360000, 0    , 1     , 0 },
	{ "muxTSSeg_1.ts"                      , ""         , ""           , 360000, 18048, 1     , 0 },
	{ "muxTSSeg_.m3u8"                     , ""         , ""           , 0     , 0    , 1     , 0 },
	{ "v_0_320x180-init.m4s"               , "video/mp4", "avc1.42C00D", 0     , 0    , 8     , 1 },
	{ "v_0_320x180_10K/v_0_320x180-500.m4s", "video/mp4", "avc1.42C00D", 360000, 21550, 8     , 1 },
	{ "v_0_320x180_10K/v_0_320x180-501.m4s", "video/mp4", "avc1.42C00D", 360000, 0    , 8     , 1 },
	{ "dash.mpd"                           , ""         , ""           , 360000, 0    , 1     , 0 },
	{ "v_0_320x180_10K/v_0_320x180-501.m4s", "video/mp4", "avc1.42C00D", 360000, 23485, 8     , 1 },
	{ "v_0_320x180_10K/v_0_320x180-502.m4s", "video/mp4", "avc1.42C00D", 360000, 0    , 8     , 1 },
	{ "dash.mpd"                           , ""         , ""           , 360000, 0    , 1     , 0 },
	{ "v_0_320x180_10K/v_0_320x180-502.m4s", "video/mp4", "avc1.42C00D", 172800, 11963, 8     , 1 },
	{ "v_0_320x180_10K/v_0_320x180-503.m4s", "video/mp4", "avc1.42C00D", 172800, 0    , 8     , 1 },
	{ "dash.mpd"                           , ""         , ""           , 360000, 0    , 1     , 0 },
	{ "dash.mpd"                           , ""         , ""           , 360000, 0    , 1     , 0 },
	{ "dash.mpd"                           , ""         , ""           , 360000, 0    , 1     , 0 },
	{ "v_0_320x180-init.m4s"               , "video/mp4", "avc1.42C00D", 0     , 806  , 0     , 1 },
	{ "hls_mp4.m3u8"                       , ""         , ""           , 360000, 0    , 1     , 0 },
	{ "v_0_320x180_7K/v_0_320x180-500.m4s" , "video/mp4", "avc1.42C00D", 360000, 15734, 360000, 1 },
	{ "v_0_320x180_7K/v_0_320x180_.m3u8"   , ""         , ""           , 360000, 0    , 1     , 0 },
	{ "v_0_320x180_7K/v_0_320x180-501.m4s" , "video/mp4", "avc1.42C00D", 360000, 17469, 360000, 1 },
	{ "v_0_320x180_7K/v_0_320x180_.m3u8"   , ""         , ""           , 360000, 0    , 1     , 0 },
	{ "v_0_320x180_7K/v_0_320x180-502.m4s" , "video/mp4", "avc1.42C00D", 172800, 9083 , 172800, 1 },
	{ "v_0_320x180_7K/v_0_320x180_.m3u8"   , ""         , ""           , 360000, 0    , 1     , 0 },
	{ "v_0_320x180_7K/v_0_320x180_.m3u8"   , ""         , ""           , 360000, 0    , 1     , 0 },
	{ "v_0_320x180-init.m4s"               , "video/mp4", "avc1.42C00D", 0,        806, 0     , 1 },
	{ "v_0_320x180_7K/v_0_320x180-500.m4s" , "video/mp4", "avc1.42C00D", 360000, 15734, 360000, 1 },
	{ "v_0_320x180_7K/v_0_320x180-501.m4s" , "video/mp4", "avc1.42C00D", 360000, 0    , 360000, 1 },
	{ "dash.mpd"                           , ""         , ""           , 360000, 0    , 1     , 0 },
	{ "v_0_320x180_7K/v_0_320x180-501.m4s" , "video/mp4", "avc1.42C00D", 360000, 17469, 360000, 1 },
	{ "v_0_320x180_7K/v_0_320x180-502.m4s" , "video/mp4", "avc1.42C00D", 360000, 0    , 360000, 1 },
	{ "dash.mpd"                           , ""         , ""           , 360000, 0    , 1     , 0 },
	{ "v_0_320x180_7K/v_0_320x180-502.m4s" , "video/mp4", "avc1.42C00D", 172800, 9083 , 172800, 1 },
	{ "v_0_320x180_7K/v_0_320x180-503.m4s" , "video/mp4", "avc1.42C00D", 172800, 0    , 172800, 1 },
	{ "dash.mpd"                           , ""         , ""           , 360000, 0    , 1     , 0 },
	{ "dash.mpd"                           , ""         , ""           , 360000, 0    , 1     , 0 },
	{ "dash.mpd"                           , ""         , ""           , 360000, 0    , 1     , 0 }
	};

	auto const segmentDurationInMs = 2000;
	DataBase::absUTCOffsetInMs = 1000000;
	auto subDir = "v_0_320x180_10K/";
	if ((gf_dir_exists(subDir) == GF_FALSE) && gf_mkdir(subDir))
		throw std::runtime_error(format("couldn't create subdir \"%s\": please check you have sufficient rights", subDir));
	auto muxMP4File = create<Mux::GPACMuxMP4>(format("%sv_0_320x180", subDir), segmentDurationInMs, Mux::GPACMuxMP4::FragmentedSegment, Mux::GPACMuxMP4::OneFragmentPerFrame/*, Mux::GPACMuxMP4::SegNumStartsAtZero*/); //Romain: remove SegNumStartsAtZero
	auto muxMP4Mem = create<Mux::GPACMuxMP4>("", segmentDurationInMs, Mux::GPACMuxMP4::FragmentedSegment, Mux::GPACMuxMP4::OneFragmentPerSegment/*, Mux::GPACMuxMP4::SegNumStartsAtZero*/);
	auto muxMP4MemFlushFrags = create<Mux::GPACMuxMP4>("", segmentDurationInMs, Mux::GPACMuxMP4::FragmentedSegment, Mux::GPACMuxMP4::OneFragmentPerSegment, /*Mux::GPACMuxMP4::SegNumStartsAtZero | */Mux::GPACMuxMP4::FlushFragMemory);
	auto muxTSSeg = create<Stream::LibavMuxHLSTS>(false, segmentDurationInMs, "", "muxTSSeg_", format("-hls_time %s -hls_playlist_type event", segmentDurationInMs / 1000));

	auto clock = shptr(new Clock(0.0));
	auto hls_ts = createModule<Stream::Apple_HLS>(ALLOC_NUM_BLOCKS_DEFAULT, clock, "", "hls_ts.m3u8", Stream::AdaptiveStreamingCommon::Live, segmentDurationInMs, false, Stream::AdaptiveStreamingCommon::SegmentsNotOwned | Stream::AdaptiveStreamingCommon::PresignalNextSegment | Stream::AdaptiveStreamingCommon::ForceRealDurations);
	std::vector<std::unique_ptr<Stream::Apple_HLS>> hls_mp4;
	std::vector<std::unique_ptr<Stream::MPEG_DASH>> dash;
	for (auto i = 0; i < 3; ++i) {
		hls_mp4.push_back(createModule<Stream::Apple_HLS>(ALLOC_NUM_BLOCKS_DEFAULT, clock, "", "hls_mp4.m3u8", Stream::AdaptiveStreamingCommon::Live, segmentDurationInMs, true, Stream::AdaptiveStreamingCommon::SegmentsNotOwned | Stream::AdaptiveStreamingCommon::PresignalNextSegment | Stream::AdaptiveStreamingCommon::ForceRealDurations));
		dash.push_back(createModule<Stream::MPEG_DASH>(ALLOC_NUM_BLOCKS_DEFAULT, clock, "", "dash.mpd", Stream::AdaptiveStreamingCommon::Live, segmentDurationInMs, 0, segmentDurationInMs, 0, std::vector<std::string>(), "id", 0, Stream::AdaptiveStreamingCommon::SegmentsNotOwned | Stream::AdaptiveStreamingCommon::PresignalNextSegment | Stream::AdaptiveStreamingCommon::ForceRealDurations));
	}

	auto demux = create<Demux::LibavDemux>("data/beepbop.mp4");
	std::unique_ptr<Decode::LibavDecode> decode;
	std::unique_ptr<Encode::LibavEncode> encode;
	std::vector<std::unique_ptr<Listener>> listeners;
	for (size_t i = 0; i < demux->getNumOutputs(); ++i) {
		auto metadata = demux->getOutput(i)->getMetadata();
		if (metadata->isVideo()) {
			ConnectModules(demux.get() , i, muxMP4File.get(), 0);
			ConnectModules(demux.get() , i, muxMP4Mem.get(), 0);
			ConnectModules(demux.get() , i, muxMP4MemFlushFrags.get(), 0);
			ConnectModules(demux.get() , i, muxTSSeg.get(), 0);

			ConnectModules(muxTSSeg.get(), 0, hls_ts.get(), 0);
			listeners.push_back(create<Listener>());
			ConnectModules(muxTSSeg.get(), 0, listeners.back().get(), 0);
			ConnectModules(muxTSSeg.get(), 1, listeners.back().get(), 0);
			ConnectModules(hls_ts.get(), 0, listeners.back().get(), 0);
			ConnectModules(hls_ts.get(), 1, listeners.back().get(), 0);

			ConnectModules(muxMP4File.get(), 0, dash[0].get(), 0);
			listeners.push_back(create<Listener>());
			ConnectModules(dash[0].get(), 0, listeners.back().get(), 0);
			ConnectModules(dash[0].get(), 1, listeners.back().get(), 0);

			ConnectModules(muxMP4Mem.get(), 0, hls_mp4[1].get(), 0);
			listeners.push_back(create<Listener>());
			ConnectModules(hls_mp4[1].get(), 0, listeners.back().get(), 0);
			ConnectModules(hls_mp4[1].get(), 1, listeners.back().get(), 0);

			ConnectModules(muxMP4Mem.get(), 0, dash[1].get(), 0);
			listeners.push_back(create<Listener>());
			ConnectModules(dash[1].get(), 0, listeners.back().get(), 0);
			ConnectModules(dash[1].get(), 1, listeners.back().get(), 0);

#if 0
			ConnectModules(muxMP4MemFlushFrags.get(), 0, hls_mp4[2].get(), 0);
			listeners.push_back(create<Listener>());
			ConnectModules(hls_mp4[2].get(), 0, listeners.back().get(), 0);
			ConnectModules(hls_mp4[2].get(), 1, listeners.back().get(), 0);

			ConnectModules(muxMP4MemFlushFrags.get(), 0, dash[2].get(), 0);
			listeners.push_back(create<Listener>());
			ConnectModules(dash[2].get(), 0, listeners.back().get(), 0);
			ConnectModules(dash[2].get(), 1, listeners.back().get(), 0);
#endif

			break;
		}
	}

	demux->process(nullptr);

	muxMP4File->flush();
	muxMP4Mem->flush();
	muxMP4MemFlushFrags->flush();
	muxTSSeg->flush();
	hls_ts->flush();
	for (auto &h : hls_mp4) {
		h->flush();
	}
	for (auto &d : dash) {
		d->flush();
	}
	for (auto &l : listeners) {
		l->flush();
		for (auto &r : l->results) {
			results.push_back(r);
		}
	}

	ASSERT_EQUALS(results.size(), ref.size());
	ASSERT(std::equal(results.begin(), results.end(), ref.begin()));
}
