#pragma once

#include "lib_modules/core/module.hpp"

#include "aws_sdk_instance.hpp"


/*
pour toi:
create<Mux::GPACMuxMP4>("", segmentDurationInMs, Mux::GPACMuxMP4::FragmentedSegment, Mux::GPACMuxMP4::OneFragmentPerSegment)
avec cette config, tu recois des données en mémoire
pour connaître les propriétés, il faut lire les metadata:
auto meta = safe_cast<const MetadataFile>(data->getMetadata());
tu trouveras des exemples dans le code
je m'arrete là pour ajd, je vais exploser ; )
à demain!

*/
namespace Modules {
namespace Out {

class AwsMediaStore : public ModuleS {
	public:
		AwsMediaStore(std::string const& endpoint);
		~AwsMediaStore();
		void process(Data data) override;

	private:
		Aws::SDKOptions options; std::shared_ptr<Aws::MediaStoreData::MediaStoreDataClient> mediastoreDataClient;
		AwsSdkInstance sdk;
		std::string currentFilename;

		std::vector<uint8_t> awsData;
};

}
}
