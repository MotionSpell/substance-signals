#pragma once

#include "data.hpp"
#include "lib_utils/log.hpp"
#include <memory>
#include <typeinfo>

template<typename T, size_t N>
constexpr size_t NELEMENTS(T const (&array)[N]) {
	(void)array;
	return N;
}

namespace Modules {

enum StreamType {
	UNKNOWN_ST = -1,
	AUDIO_RAW,    //uncompressed audio
	VIDEO_RAW,    //uncompressed video
	AUDIO_PKT,    //compressed audio
	VIDEO_PKT,    //compressed video
	SUBTITLE_PKT, //subtitles and captions
	PLAYLIST,     //playlist and adaptive streaming manifests
	SEGMENT,      //adaptive streaming init and media segments
	SIZE_OF_ENUM_STREAM_TYPE
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


struct IMetadataCap {
	virtual ~IMetadataCap() noexcept(false) {}
	virtual std::shared_ptr<const IMetadata> getMetadata() const = 0;
	virtual void setMetadata(std::shared_ptr<const IMetadata> metadata) = 0;
};

class MetadataCap : public virtual IMetadataCap {
	public:
		MetadataCap(std::shared_ptr<const IMetadata> metadata = nullptr) : m_metadata(metadata) {}
		virtual ~MetadataCap() noexcept(false) {}

		std::shared_ptr<const IMetadata> getMetadata() const override {
			return m_metadata;
		}
		void setMetadata(std::shared_ptr<const IMetadata> metadata) override {
			if (!setMetadataInternal(metadata))
				throw std::runtime_error("Metadata could not be set.");
		}

		bool updateMetadata(Data &data) {
			if (!data) {
				return false;
			} else {
				auto const &metadata = data->getMetadata();
				if (!metadata) {
					const_cast<DataBase*>(data.get())->setMetadata(m_metadata);
					return true;
				} else {
					return setMetadataInternal(metadata);
				}
			}
		}

	private:
		bool setMetadataInternal(const std::shared_ptr<const IMetadata> &metadata) {
			if (metadata != m_metadata) {
				if (m_metadata) {
					if (metadata->getStreamType() != m_metadata->getStreamType()) {
						throw std::runtime_error(format("Metadata update: incompatible types %s for data and %s for attached", metadata->getStreamType(), m_metadata->getStreamType()));
					} else if (*m_metadata == *metadata) {
						Log::msg(Debug, "Output: metadata not equal but comparable by value. Updating.");
						m_metadata = metadata;
					} else {
						Log::msg(Info, "Metadata update from data not supported yet: output port and data won't carry the same metadata.");
					}
					return true;
				}
				Log::msg(Debug, "Output: metadata transported by data changed. Updating.");
				m_metadata = metadata;
				return true;
			} else {
				return false;
			}
		}

		std::shared_ptr<const IMetadata> m_metadata;
};

}
