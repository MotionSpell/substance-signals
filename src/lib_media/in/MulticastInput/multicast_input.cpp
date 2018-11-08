#include "multicast_input.hpp"
#include "lib_modules/utils/factory.hpp" // registerModule
#include "lib_modules/utils/helper.hpp"
#include "socket.hpp"

using namespace Modules;

namespace {

struct MulticastInput : ActiveModule {
	MulticastInput(KHost* host, MulticastInputConfig const& config)
		: m_host(host) {
		m_socket = createSocket();

		char buffer[256];
		sprintf(buffer, "%d.%d.%d.%d", config.ipAddr[0], config.ipAddr[1], config.ipAddr[2], config.ipAddr[3]);
		m_socket->joinMulticastGroup(buffer, config.port);

		m_output = addOutput<OutputDefault>();
	}

	// must be able to receive at least 35Mbps
	bool work() override {
		auto buf = m_output->getBuffer(4096);

		auto size = m_socket->receive(buf->data().ptr, buf->data().len);
		if(size > 0) {
			buf->resize(size);
			m_output->emit(buf);
		}

		return true;
	}

	KHost* const m_host;
	std::unique_ptr<ISocket> m_socket;
	OutputDefault* m_output;
};

Modules::IModule* createObject(KHost* host, va_list va) {
	auto config = va_arg(va, MulticastInputConfig*);
	enforce(host, "MulticastInput: host can't be NULL");
	enforce(config, "MulticastInput: config can't be NULL");
	return Modules::create<MulticastInput>(host, *config).release();
}

}

auto const registered = Factory::registerModule("MulticastInput", &createObject);

