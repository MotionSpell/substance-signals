// A stream parsing PSI tables (i.e PAT, PMT)
#pragma once

#include "stream.hpp"
#include <vector>

auto const TABLE_ID_PAT = 0;
auto const TABLE_ID_PMT = 2;

struct PsiStream : Stream {

		struct EsInfo {
			int pid, mpegStreamType;
		};

		struct Listener {
			virtual void onPat(span<int> pmtPids) = 0;
			virtual void onPmt(span<EsInfo> esInfo) = 0;
		};

		PsiStream(int pid_, IModuleHost* host, Listener* listener_) : Stream(pid_, host), listener(listener_) {
		}

		void push(SpanC data) override {

			BitReader r = {data};
			if(/*FIXME: payloadUnitStartIndicator*/1) {
				int pointerField = r.u(8);
				if(pointerField > r.remaining()) {
					m_host->log(Error, "Invalid pointer_field before PSI section");
					return;
				}
				for(int i=0; i < pointerField; ++i)
					r.u(8);
			}

			r = BitReader{r.payload()};

			auto const PSI_HEADER_SIZE = 8;
			if(r.remaining() < PSI_HEADER_SIZE) {
				m_host->log(Error, "Truncated PSI header");
				return;
			}

			auto const table_id = r.u(8);
			/*auto const section_syntax_indicator =*/ r.u(1);
			/*auto const private_bit =*/ r.u(1);
			/*auto const reserved1 =*/ r.u(2);
			auto const section_length = r.u(12);

			auto sectionStart = r.byteOffset();
			if(r.remaining() < section_length) {
				m_host->log(Error, "Invalid section_length in PSI header");
				return;
			}

			/*auto const table_id_extension =*/ r.u(16);
			/*auto const reserved2 =*/ r.u(2);
			/*auto const version_number =*/ r.u(5);
			/*auto const current_next_indicator =*/ r.u(1);
			/*auto const section_number =*/ r.u(8);
			/*auto const last_section_number =*/ r.u(8);

			assert(PSI_HEADER_SIZE == r.byteOffset());

			switch(table_id) {
			case TABLE_ID_PAT: {
				// actual PAT data
				/*auto const program_number =*/ r.u(16);
				/*auto const reserved3 =*/ r.u(3);
				auto const program_map_pid = r.u(13);

				int pids[] = { program_map_pid };
				listener->onPat(pids);
				break;
			}
			case TABLE_ID_PMT: {
				/*auto const reserved3 =*/ r.u(3);
				/*auto const pcr_pid =*/ r.u(13);
				/*auto const reserved4 =*/ r.u(4);
				auto const program_info_length = r.u(12);

				// skip program descriptors
				{
					if(r.remaining() < program_info_length) {
						m_host->log(Error, "Invalid program_info_length in PSI header");
						return;
					}
					for(int i=0; i < program_info_length; ++i)
						r.u(8);
				}

				vector<EsInfo> info;

				while(r.byteOffset() < sectionStart + section_length) {
					// Elementary stream info
					auto const stream_type = r.u(8);
					/*auto const reserved5 =*/ r.u(3);
					auto const pid = r.u(13);
					/*auto const reserved6 =*/ r.u(4);
					auto const es_info_length = r.u(12);

					// skip es_info
					for(int i=0; i < es_info_length; ++i)
						r.u(8);

					info.push_back({ pid, stream_type });
				}

				listener->onPmt({info.data(), info.size()});
				break;
			}
			break;
			default:
				break;
			}
		}

		void flush() override {
		}

	private:
		Listener* const listener;
};

