#pragma once

#include "data.hpp"
#include "lib_utils/log.hpp"
#include <memory>
#include <typeinfo>

namespace Modules {

struct IMetadataCap {
	virtual ~IMetadataCap() noexcept(false) {}
	virtual std::shared_ptr<const IMetadata> getMetadata() const = 0;
	virtual void setMetadata(IMetadata *metadata) = 0;
	virtual void setMetadata(std::shared_ptr<const IMetadata> metadata) = 0;
};

enum StreamType {
	UNKNOWN_ST = -1,
	AUDIO_RAW,    //uncompressed audio
	VIDEO_RAW,    //uncompressed video
	AUDIO_PKT,    //compressed audio
	VIDEO_PKT,    //compressed video
	SUBTITLE_PKT, //subtitles and captions
	PLAYLIST      //playlist and adaptive streaming manifests
};

struct IMetadata {
	virtual ~IMetadata() {}
	virtual StreamType getStreamType() const = 0;
	bool isVideo() const {
		switch (getStreamType()) {
		case VIDEO_RAW: case VIDEO_PKT: return true;
		default: return false;
		}
	}
	bool isAudio() const {
		switch (getStreamType()) {
		case AUDIO_RAW: case AUDIO_PKT: return true;
		default: return false;
		}
	}
	bool isSubtitle() const {
		switch (getStreamType()) {
		case SUBTITLE_PKT: return true;
		default: return false;
		}
	}
};

static bool operator==(const IMetadata &left, const IMetadata &right) {
	return typeid(left) == typeid(right);
}

class MetadataFile : public IMetadata {
	public:
		MetadataFile(const std::string& filename, StreamType streamType, const std::string& mimeType, const std::string& codecName, uint64_t durationIn180k, uint64_t filesize, uint64_t latencyIn180k, bool startsWithRAP)
			: streamType(streamType), filename(filename), mimeType(mimeType), codecName(codecName), durationIn180k(durationIn180k), filesize(filesize), latencyIn180k(latencyIn180k), startsWithRAP(startsWithRAP) {
		}
		std::string getFilename() const {
			return filename;
		}
		std::string getMimeType() const {
			return mimeType;
		}
		std::string getCodecName() const {
			return codecName;
		}
		StreamType getStreamType() const override {
			return streamType;
		}
		uint64_t getDuration() const {
			return durationIn180k;
		}
		uint64_t getSize() const {
			return filesize;
		}
		uint64_t getLatency() const {
			return latencyIn180k;
		}
		bool getStartsWithRAP() const {
			return startsWithRAP;
		}

		union {
			unsigned int resolution[2];
			unsigned int sampleRate;
		};

	private:
		StreamType streamType;
		std::string filename, mimeType, codecName/*as per RFC6381*/;
		uint64_t durationIn180k, filesize, latencyIn180k;
		bool startsWithRAP;
};

//TODO: should be picture and Pcm and return the same fields as MetadataPkt
struct MetadataRawVideo : public IMetadata {
	StreamType getStreamType() const override {
		return VIDEO_RAW;
	}
};

struct MetadataRawAudio : public IMetadata {
	StreamType getStreamType() const override {
		return AUDIO_RAW;
	}
};

struct MetadataPkt : public IMetadata {
};

struct MetadataPktVideo : public MetadataPkt {
	StreamType getStreamType() const override {
		return VIDEO_PKT;
	}
};

struct MetadataPktAudio : public MetadataPkt {
	StreamType getStreamType() const override {
		return AUDIO_PKT;
	}
};

class MetadataCap : public virtual IMetadataCap {
	public:
		MetadataCap(IMetadata *metadata = nullptr) : m_metadata(metadata) {}
		virtual ~MetadataCap() noexcept(false) {}

		std::shared_ptr<const IMetadata> getMetadata() const override {
			return m_metadata;
		}

		//Takes ownership.
		void setMetadata(IMetadata *metadata) override {
			m_metadata = std::shared_ptr<const IMetadata>(metadata);
		}
		void setMetadata(std::shared_ptr<const IMetadata> metadata) override {
			m_metadata = metadata;
		}

		bool updateMetadata(Data data) {
			if (!data) {
				return false;
			} else {
				auto const metadata = data->getMetadata();
				if (!metadata) {
					const_cast<DataBase*>(data.get())->setMetadata(m_metadata);
					return true;
				} else if (metadata != m_metadata) {
					if (m_metadata) {
						if (*m_metadata == *metadata) {
							Log::msg(Debug, "Output: metadata not equal but comparable by value. Updating.");
							m_metadata = metadata;
						} else {
							Log::msg(Info, "Metadata update from data not supported yet: output pin and data won't carry the same metadata.");
						}
						return true;
					}
					Log::msg(Info, "Output: metadata transported by data changed. Updating.");
					if (m_metadata && (metadata->getStreamType() != m_metadata->getStreamType()))
						throw std::runtime_error(format("Metadata update: incompatible types %s for data and %s for attached", metadata->getStreamType(), m_metadata->getStreamType()));
					m_metadata = metadata;
					return true;
				} else {
					return false;
				}
			}
		}

	private:
		std::shared_ptr<const IMetadata> m_metadata;
};

}
