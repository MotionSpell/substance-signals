#include "socket.hpp"
#include <stdexcept>

#include <fcntl.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

using namespace std;

namespace {
struct Socket : IOutputSocket {
	Socket(const char* address, int port) {
		m_socket = socket(AF_INET, SOCK_DGRAM, 0);

		if(m_socket < 0)
			throw runtime_error("socket failed");

		m_dstAddr.sin_family = AF_INET;
		m_dstAddr.sin_addr.s_addr = inet_addr(address);
		m_dstAddr.sin_port = htons(port);
	}

	~Socket() {
		close(m_socket);
	}

	void send(const uint8_t* buffer, size_t len) override {
		if(::sendto(m_socket, buffer, len, 0, (sockaddr*)&m_dstAddr, sizeof(m_dstAddr)) < 0)
			throw runtime_error("UDP send failed");
	}

	int m_socket = -1;
	sockaddr_in m_dstAddr {};
};
}

std::unique_ptr<IOutputSocket> createOutputSocket(const char* address, int port) {
	return make_unique<Socket>(address, port);
}

