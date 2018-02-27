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

void AwsMediaStore::process(Data data_) {
	auto data = safe_cast<const DataBase>(data_);
	auto meta = safe_cast<const MetadataFile>(data->getMetadata());
	if (data->size() == 0) { //flush
		if (awsData.empty()) {
			return;
		}

		Aws::Utils::Array<uint8_t> array(&awsData[0], awsData.size());
		Aws::Utils::Stream::PreallocatedStreamBuf streamBuf(&array, array.GetLength());
		auto preallocatedStreamReader = Aws::MakeShared<Aws::IOStream>("preallocatedStreamReader", &streamBuf);

		Aws::MediaStoreData::Model::PutObjectRequest request;
		request.WithPath(meta->getFilename().c_str());
		std::string filename = meta->getFilename().c_str();
		if (filename.substr(filename.size() - 4, 4).compare(".mpd") == 0) {
			//request.SetContentType("application/dash+xml"); // => makes AWS SDK unhappy
		}
		else if (filename.substr(filename.size() - 5, 5).compare(".m3u8") == 0) {
			request.SetContentType("application/x-mpegURL");
		}
		else if (filename.substr(filename.size() - 4, 4).compare(".m4s") == 0) {
			request.SetContentType("video/mp4");
		}
		request.SetBody(preallocatedStreamReader);
		auto outcome = mediastoreDataClient->PutObject(request);
		if (outcome.IsSuccess()) {
			log(Info, "%s sucessfully uploaded", meta->getFilename());
		}
		else {
			log(Error, "%s failed to upload %s %s", meta->getFilename(), outcome.GetError().GetExceptionName(), outcome.GetError().GetMessage());
		}
		awsData.clear();
	}
	else {
		//growing vector, pretty inefficient for now
		awsData.insert(awsData.end(), data->data(), data->data() + data->size());
	}
	
}

}
}
