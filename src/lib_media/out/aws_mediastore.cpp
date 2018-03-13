#include "lib_utils/tools.hpp"
#include "aws_mediastore.hpp"


#include <aws/mediastore/model/DescribeContainerRequest.h>
#include <aws/mediastore/MediaStoreClient.h>

#include <aws/mediastore-data/MediaStoreDataClient.h>
#include <aws/mediastore-data/MediaStoreDataErrors.h>

#include <aws/mediastore-data/model/PutObjectRequest.h>
#include <aws/mediastore-data/model/PutObjectResult.h>
#include <aws/core/utils/Outcome.h>
#include <aws/core/utils/Array.h>
#include <aws/core/utils/StringUtils.h>
#include <aws/core/utils/stream/PreallocatedStreamBuf.h>

#include <fstream>

#define USE_IMPORT_EXPORT 

namespace Modules {
namespace Out {

AwsMediaStore::AwsMediaStore(std::string const& endpoint) {
	Aws::Client::ClientConfiguration objectConfig;
	objectConfig.endpointOverride = Aws::String(endpoint.c_str());
	objectConfig.region = std::getenv("AWS_DEFAULT_REGION");
	mediastoreDataClient = std::shared_ptr<Aws::MediaStoreData::MediaStoreDataClient>(new Aws::MediaStoreData::MediaStoreDataClient(objectConfig));

	addInput(new Input<DataBase>(this));
}

AwsMediaStore::~AwsMediaStore() {
}

void AwsMediaStore::flush() {
	//Not called
	return;
}

void AwsMediaStore::process(Data data_) {
	auto data = safe_cast<const DataBase>(data_);
	auto meta = safe_cast<const MetadataFile>(data->getMetadata());


	currentFilename = meta->getFilename();
	if (data->size() != 0) 
		awsData.insert(awsData.end(), data->data(), data->data() + data->size());
	
	if (meta->getEOS()) {


		Aws::MediaStoreData::Model::PutObjectRequest request;
		request.WithPath(currentFilename.c_str());
		if (currentFilename.substr(currentFilename.size() - 4, 4).compare(".mpd") == 0) {
			request.SetContentType("application/dash+xml");
		}
		else if (currentFilename.substr(currentFilename.size() - 5, 5).compare(".m3u8") == 0) {
			request.SetContentType("application/x-mpegURL");
		}
		else if (currentFilename.substr(currentFilename.size() - 4, 4).compare(".m4s") == 0 || currentFilename.substr(currentFilename.size() - 4, 4).compare(".mp4") == 0 || currentFilename.substr(currentFilename.size() - 4, 4).compare(".m4a") == 0) {
			request.SetContentType("video/mp4");
		}

		if (awsData.size() == 0) {
			std::ifstream file(currentFilename, std::ios::binary | std::ios::ate);

			if (!file.good()) {
				log(Error, "Could not open file %s", currentFilename.c_str());
				return;
			}
			auto const size = file.tellg();
			file.seekg(0, std::ios::beg);

			awsData.resize((size_t)size);
			file.read((char*)awsData.data(), size);
		}

		Aws::Utils::Array<uint8_t> array(&awsData[0], awsData.size());
		Aws::Utils::Stream::PreallocatedStreamBuf streamBuf(&array, array.GetLength());
		auto preallocatedStreamReader = Aws::MakeShared<Aws::IOStream>("preallocatedStreamReader", &streamBuf);

		request.SetBody(preallocatedStreamReader);
		auto outcome = mediastoreDataClient->PutObject(request);
		if (outcome.IsSuccess()) {
			log(Info, "%s sucessfully uploaded", currentFilename);
		}
		else {
			log(Error, "%s failed to upload %s %s", currentFilename, outcome.GetError().GetExceptionName(), outcome.GetError().GetMessage());
		}
		awsData.clear();


	}

}

}
}
