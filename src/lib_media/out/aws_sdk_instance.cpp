#include "aws_sdk_instance.hpp"

namespace Modules {
namespace Out {
std::shared_ptr<Aws::Client::ClientConfiguration> AwsSdkInstance::clientConfig;
Aws::SDKOptions AwsSdkInstance::options;
uint64_t AwsSdkInstance::instances=0;

AwsSdkInstance::AwsSdkInstance() {
	std::lock_guard<std::mutex> lock(mutex);
	instances++;
	if (instances == 1) {
		options.loggingOptions.logLevel = Aws::Utils::Logging::LogLevel::Info;
		Aws::InitAPI(options);
		clientConfig = std::shared_ptr<Aws::Client::ClientConfiguration>(new Aws::Client::ClientConfiguration);
	}
}
AwsSdkInstance::~AwsSdkInstance() {
	std::lock_guard<std::mutex> lock(mutex);
	instances--;
	if (instances == 0) {
		Aws::ShutdownAPI(options);
	}
}


Aws::Client::ClientConfiguration AwsSdkInstance::getClientConfig() const {
	return *clientConfig;
}

}
}