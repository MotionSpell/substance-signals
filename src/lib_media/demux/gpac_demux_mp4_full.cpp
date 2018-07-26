#include "gpac_demux_mp4_full.hpp"
#include <string>
#include "lib_gpacpp/gpacpp.hpp"
#include "../common/metadata.hpp"

namespace Modules {
namespace Demux {

const int FIRST_TRACK = 1;

struct ISOProgressiveReader {

	void pushData(const uint8_t* buffer, size_t len) {
		//TODO: zero copy mode, or at least improve the current system
		//with allocator packet duplication
		const size_t currSize = data.size();
		data.resize(data.size() + len);
		memcpy(data.data() + currSize, buffer, len);
	}

	/* data buffer to be read by the parser */
	std::vector<u8> data;
	/* URL used to pass a buffer to the parser */
	std::string dataUrl() const {
		char buffer[256];
		sprintf(buffer, "gmem://%lld@%p", (long long)data.size(), data.data());
		return buffer;
	}
	/* The ISO file structure created for the parsing of data */
	std::unique_ptr<gpacpp::IsoFile> movie;
	/* Boolean state to indicate if the needs to be parsed */
	u32 samplesProcessed = 0;
	u32 sampleIndex = 1; /* samples are numbered starting from 1 */
	u32 sampleCount = 0;
};

GPACDemuxMP4Full::GPACDemuxMP4Full()
	: reader(new ISOProgressiveReader) {
	addInput(new Input<DataRaw>(this));
	output = addOutput<OutputDefault>();
}

GPACDemuxMP4Full::~GPACDemuxMP4Full() {
}

bool GPACDemuxMP4Full::openData() {
	/* if the file is not yet opened (no movie), open it in progressive mode (to update its data later on) */
	u64 missingBytes;
	GF_ISOFile *movie;
	GF_Err e = gf_isom_open_progressive(reader->dataUrl().c_str(), 0, 0, &movie, &missingBytes);
	if ((e != GF_OK && e != GF_ISOM_INCOMPLETE_FILE) || reader->movie) {
		log(Warning, "Error opening fragmented mp4 in progressive mode: %s (missing %s bytes)", gf_error_to_string(e), missingBytes);
		return false;
	}
	reader->movie.reset(new gpacpp::IsoFile(movie));
	reader->movie->setSingleMoofMode(true);
	return true;
}

void GPACDemuxMP4Full::updateData() {
	/* let inform the parser that the buffer has been updated with new data */
	if (reader->movie->isFragmented()) {
		if(!reader->data.empty())
			reader->movie->refreshFragmented(reader->dataUrl());
	}
}

bool GPACDemuxMP4Full::processSample() {
	try {
		return safeProcessSample();
	} catch(gpacpp::Error const& e) {
		log(Warning, "Could not get sample: %s", e.what());
		return false;
	}
}

bool GPACDemuxMP4Full::safeProcessSample() {
	if(auto desc = reader->movie->getDecoderConfig(FIRST_TRACK, 1)) {
		auto dsi = desc->decoderSpecificInfo;
		{
			auto infoString = string2hex((uint8_t*)dsi->data, dsi->dataLength);
			log(Debug, "Found decoder specific info: \"%s\"", infoString);
		}
		std::shared_ptr<MetadataPkt> meta;
		if(desc->streamType == GF_STREAM_AUDIO) {
			meta = make_shared<MetadataPkt>(AUDIO_PKT);
			meta->codec = "aac";
		} else {
			meta = make_shared<MetadataPkt>(VIDEO_PKT);
			meta->codec = "h264";
		}
		meta->codecSpecificInfo.assign(dsi->data, dsi->data+dsi->dataLength);
		output->setMetadata(meta);
	}

	/* let's see how many samples we have since the last parsed */
	auto newSampleCount = reader->movie->getSampleCount(FIRST_TRACK);
	if (newSampleCount > reader->sampleCount) {
		/* New samples have been added to the file */
		log(Debug, "Found %s new samples (total: %s)",
		    newSampleCount - reader->sampleCount,
		    newSampleCount);
		if (reader->sampleCount == 0) {
			reader->sampleCount = newSampleCount;
		}
	}
	if (reader->sampleCount == 0) {
		// no sample yet
		return false;
	}

	{
		/* let's analyze the samples we have parsed so far one by one */
		int di /*descriptor index*/;
		auto ISOSample = reader->movie->getSample(FIRST_TRACK, reader->sampleIndex, di);

		reader->samplesProcessed++;
		auto const DTSOffset = reader->movie->getDTSOffet(FIRST_TRACK);
		/*here we dump some sample info: samp->data, samp->dataLength, samp->isRAP, samp->DTS, samp->CTS_Offset */
		log(Debug, "Found sample #%s(#%s) of length %s , RAP: %s, DTS: %s, CTS: %s",
		    reader->sampleIndex, reader->samplesProcessed, ISOSample->dataLength,
		    ISOSample->IsRAP, ISOSample->DTS + DTSOffset, ISOSample->DTS + DTSOffset + ISOSample->CTS_Offset);
		reader->sampleIndex++;

		auto out = output->getBuffer(ISOSample->dataLength);
		out->resize(ISOSample->dataLength); // workaround allocator bug
		memcpy(out->data(), ISOSample->data, ISOSample->dataLength);
		out->setMediaTime(ISOSample->DTS + DTSOffset + ISOSample->CTS_Offset, reader->movie->getMediaTimescale(FIRST_TRACK));
		output->emit(out);
	}

	/* once we have read all the samples, we can release some data and force a reparse of the input buffer */
	if (reader->sampleIndex > reader->sampleCount) {

		/* update the sample count and sample index */
		reader->sampleCount = newSampleCount - reader->sampleCount;
		reader->sampleIndex = 1;

		log(Debug, "Releasing unnecessary buffers");
		/* release internal structures associated with the samples read so far */
		reader->movie->resetTables(true);

		/* release the associated input data as well */
		u64 newBufferStart = 0;
		reader->movie->resetDataOffset(newBufferStart);
		if (newBufferStart) {
			const auto offset = (size_t)newBufferStart;
			const auto newSize = reader->data.size() - offset;
			memmove(reader->data.data(), reader->data.data() + offset, newSize);
			reader->data.resize(newSize);
		}

		updateData();
	}

	return !reader->data.empty();
}

void GPACDemuxMP4Full::process(Data data) {
	reader->pushData(data->data(), (size_t)data->size());

	if (!reader->movie) {
		if (!openData()) {
			return;
		}
	} else {
		updateData();
	}

	while (processSample()) {
	}
}

}
}
