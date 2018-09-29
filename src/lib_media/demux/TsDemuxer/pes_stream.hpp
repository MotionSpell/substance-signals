// A stream parsing PES data.
// Outputs the payload of PES packets.
#pragma once

#include "stream.hpp"
#include "lib_utils/format.hpp"
#include <vector>
#include <string.h> // memcpy

Metadata createMetadata(int mpegStreamType) {
	auto make = [](Modules::StreamType majorType, const char* codecName) {
		auto meta = make_shared<MetadataPkt>(majorType);
		meta->codec = codecName;
		return meta;
	};

	switch(mpegStreamType) {
	case 0x02: return make(VIDEO_PKT, "m2v");
	case 0x04: return make(AUDIO_PKT, "m2a");
	case 0x1b: return make(VIDEO_PKT, "h264");
	case 0x24: return make(VIDEO_PKT, "hevc");
	default: return nullptr; // unknown stream type
	}
}

struct PesStream : Stream {
		PesStream(int pid_, int type_, IModuleHost* host, OutputDefault* output_) : Stream(pid_, host), type(type_), m_output(output_) {
		}

		void push(SpanC data) override {
			for(auto b : data)
				pesBuffer.push_back(b);
		}

		void flush() override {
			if(pesBuffer.empty())
				return;

			BitReader r = {SpanC(pesBuffer.data(), pesBuffer.size())};

			auto const start_code_prefix = r.u(24);
			if(start_code_prefix != 0x000001) {
				m_host->log(Error, format("invalid PES start code (%s)", start_code_prefix).c_str());
				return;
			}

			/*auto const stream_id =*/ r.u(8);
			/*auto const pes_packet_length =*/ r.u(16);

			// optional PES header
			auto const markerBits = r.u(2);
			if(markerBits != 0x2) {
				m_host->log(Error, "invalid PES header");
				return;
			}

			auto const scramblingControl = r.u(2); //	00 implies not scrambled
			/*auto const Priority =*/ r.u(1);
			/*auto const Data_alignment_indicator =*/ r.u(1);
			/*auto const copyrighted =*/ r.u(1);
			/*auto const original =*/ r.u(1);
			/*auto const PTS_DTS_indicator =*/ r.u(2); 	// 11 = both present, 01 is forbidden, 10 = only PTS, 00 = no PTS or DTS
			/*auto const ESCR_flag =*/ r.u(1);
			/*auto const ES_rate_flag =*/ r.u(1);
			/*auto const DSM_trick_mode_flag =*/ r.u(1);
			/*auto const Additional_copy_info_flag =*/ r.u(1);
			/*auto const CRC_flag =*/ r.u(1);
			/*auto const extension_flag =*/ r.u(1);
			auto const PES_header_length = r.u(8);

			// skip extra remaining headers
			if(PES_header_length > r.remaining()) {
				m_host->log(Error, "Invalid PES_header_length");
				return;
			}
			for(int i=0; i < PES_header_length; ++i)
				r.u(8);

			if(scramblingControl) {
				m_host->log(Warning, "discarding scrambled PES packet");
				return;
			}

			auto pesPayloadSize = pesBuffer.size() - r.byteOffset();
			auto buf = m_output->getBuffer(pesPayloadSize);
			memcpy(buf->data().ptr, pesBuffer.data()+r.byteOffset(),pesPayloadSize);
			m_output->emit(buf);

			pesBuffer.clear();
		}

		bool setType(int mpegStreamType) {
			auto meta = createMetadata(mpegStreamType);
			if(!meta)
				return false;

			m_output->setMetadata(meta);
			return true;
		}

		int type;
	private:
		OutputDefault* m_output = nullptr;
		vector<uint8_t> pesBuffer;
};

