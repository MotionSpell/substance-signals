#include "gpac_demux_mp4_full.hpp"
#include <string>
#include "lib_gpacpp/gpacpp.hpp"

namespace Modules {
namespace Demux {

struct ISOProgressiveReader {
	/* data buffer to be read by the parser */
	std::vector<u8> data;
	/* URL used to pass a buffer to the parser */
	std::string dataUrl;
	/* The ISO file structure created for the parsing of data */
	std::unique_ptr<gpacpp::IsoFile> movie;
	/* Boolean state to indicate if the needs to be parsed */
	u32 samplesProcessed = 0;
	u32 sampleIndex = 1; /* samples are numbered starting from 1 */
	u32 sampleCount = 0;
	int trackNumber = 1; //TODO: multi-tracks
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
	GF_Err e = gf_isom_open_progressive(reader->dataUrl.c_str(), 0, 0, &movie, &missingBytes);
	if ((e != GF_OK && e != GF_ISOM_INCOMPLETE_FILE) || reader->movie) {
		log(Warning, "Error opening fragmented mp4 in progressive mode: %s (missing %s bytes)", gf_error_to_string(e), missingBytes);
		return false;
	}
	reader->movie.reset(new gpacpp::IsoFile(movie));
	reader->movie->setSingleMoofMode(true);
	return true;
}

bool GPACDemuxMP4Full::updateData() {
	/* let inform the parser that the buffer has been updated with new data */
	uint64_t missingBytes;
	reader->movie->refreshFragmented(missingBytes, reader->dataUrl);
	return true;
}

bool GPACDemuxMP4Full::processSample() {
	try {
		return safeProcessSample();
	} catch(gpacpp::Error const& e) {
		log(Warning, "Could not get sample: %s", gf_error_to_string(e.error_));
		return false;
	}
}

bool GPACDemuxMP4Full::safeProcessSample() {
	/* only if we have the track number can we try to get the sample data */
	if (reader->trackNumber == 0)
		return true;

	/* let's see how many samples we have since the last parsed */
	auto newSampleCount = reader->movie->getSampleCount(reader->trackNumber);
	if (newSampleCount > reader->sampleCount) {
		/* New samples have been added to the file */
		log(Info, "Found %s new samples (total: %s)",
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
		auto ISOSample = reader->movie->getSample(reader->trackNumber, reader->sampleIndex, di);
		/* if you want the sample description data, you can call:
		   GF_Descriptor *desc = movie->getDecoderConfig(reader->track_handle, di);
		   */

		reader->samplesProcessed++;
		auto const DTSOffset = reader->movie->getDTSOffet(reader->trackNumber);
		/*here we dump some sample info: samp->data, samp->dataLength, samp->isRAP, samp->DTS, samp->CTS_Offset */
		log(Debug, "Found sample #%s(#%s) of length %s , RAP: %s, DTS: %s, CTS: %s",
		    reader->sampleIndex, reader->samplesProcessed, ISOSample->dataLength,
		    ISOSample->IsRAP, ISOSample->DTS + DTSOffset, ISOSample->DTS + DTSOffset + ISOSample->CTS_Offset);
		reader->sampleIndex++;

		auto out = output->getBuffer(ISOSample->dataLength);
		memcpy(out->data(), ISOSample->data, ISOSample->dataLength);
		out->setMediaTime(ISOSample->DTS + DTSOffset, reader->movie->getMediaTimescale(reader->trackNumber));
		output->emit(out);
	}

	/* once we have read all the samples, we can release some data and force a reparse of the input buffer */
	if (reader->sampleIndex > reader->sampleCount) {
		u64 newBufferStart = 0;
		u64 missingBytes;

		log(Debug, "Releasing unnecessary buffers");
		/* release internal structures associated with the samples read so far */
		reader->movie->resetTables(true);

		/* release the associated input data as well */
		reader->movie->resetDataOffset(newBufferStart);
		if (newBufferStart) {
			u32 offset = (u32)newBufferStart;
			const size_t newSize = reader->data.size() - offset;
			memmove(reader->data.data(), reader->data.data() + offset, newSize);
			reader->data.resize(newSize);
		}
		reader->dataUrl = format("gmem://%s@%s", reader->data.size(), (void*)reader->data.data());
		reader->movie->refreshFragmented(missingBytes, reader->dataUrl);

		/* update the sample count and sample index */
		reader->sampleCount = newSampleCount - reader->sampleCount;
		reader->sampleIndex = 1;
	}

	return true;
}

void GPACDemuxMP4Full::processData() {
	bool res = processSample();
	if (!res) {
		return;
	}
	while (processSample()) {
	}
}

void GPACDemuxMP4Full::process(Data data_) {
#if 0 //TODO: zero copy mode, or at least improve the current system with allocator packet duplication
	reader->validDataSize = reader->dataSize = data->size();
	reader->data.data() = data->data();
#else
	auto data = safe_cast<const DataBase>(data_);
	const size_t currSize = reader->data.size();
	reader->data.resize(reader->data.size() + (size_t)data->size());
	memcpy(reader->data.data() + currSize, data->data(), (size_t)data->size());
#endif
	reader->dataUrl = format("gmem://%s@%s", reader->data.size(), (void*)reader->data.data());

	if (!reader->movie) {
		if (!openData()) {
			return;
		}
	} else {
		if (!updateData()) {
			return;
		}
	}

	processData();
}

}
}
