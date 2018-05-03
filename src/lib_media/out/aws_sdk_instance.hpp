#pragma once


#include <aws/core/Aws.h>
#include <aws/mediastore-data/MediaStoreDataClient.h>
#include <mutex>

namespace Modules {
namespace Out {


class AwsSdkInstance {
	public:
		AwsSdkInstance();
		~AwsSdkInstance();
		Aws::Client::ClientConfiguration getClientConfig() const;

	private:
		std::mutex mutex;
		static Aws::SDKOptions options;
		static std::shared_ptr<Aws::Client::ClientConfiguration> clientConfig;
		static uint64_t instances;
};
}
}