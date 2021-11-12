#include "socket_input.hpp"
#include "lib_modules/utils/factory.hpp" // registerModule
#include "lib_modules/utils/helper.hpp"
#include "lib_utils/tools.hpp" // enforce
#include "lib_utils/socket.hpp"

using namespace Modules;

namespace {

struct SocketInput : Module {
	SocketInput(KHost* host, SocketInputConfig const& config)
		: m_host(host) {

		m_host->activate(true);

		m_socket = createSocket(false);

		char buffer[256];
		sprintf(buffer, "%d.%d.%d.%d", config.ipAddr[0], config.ipAddr[1], config.ipAddr[2], config.ipAddr[3]);
		m_socket->joinMulticastGroup(buffer, config.port);

		m_output = addOutput();
	}

	// must be able to receive at least 35Mbps
	void process() override {
		auto buf = m_output->allocData<DataRaw>(4096);
		auto dst = buf->buffer->data();

		auto size = m_socket->receive(dst.ptr, dst.len);
		if(size > 0) {
			buf->buffer->resize(size);
			m_output->post(buf);
		}
	}

	KHost* const m_host;
	std::unique_ptr<ISocket> m_socket;
	OutputDefault* m_output;
};

IModule* createObject(KHost* host, void* va) {
	auto config = (SocketInputConfig*)va;
	enforce(host, "SocketInput: host can't be NULL");
	enforce(config, "SocketInput: config can't be NULL");
	return createModule<SocketInput>(host, *config).release();
}

auto const registered = Factory::registerModule("SocketInput", &createObject);

}

