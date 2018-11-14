#include "socket.hpp"
#include <stdexcept>

#include <winsock2.h>
#include <ws2tcpip.h>

using namespace std;

struct Socket : ISocket {
	Socket() {
		WSADATA wsaData;
		auto res = WSAStartup(MAKEWORD(2, 2), &wsaData);

		if(res)
			throw runtime_error("WSAStartup failed");

		m_socket = socket(AF_INET, SOCK_DGRAM, 0);

		if(m_socket == INVALID_SOCKET)
			throw runtime_error("socket failed");

		// share port number
		int one = 1;

		if(setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&one, sizeof one) < 0)
			throw runtime_error("setsockopt failed");

		// set non-blocking
		unsigned long Yes = TRUE;
		ioctlsocket(m_socket, FIONBIO, &Yes);
	}

	~Socket() {
		closesocket(m_socket);
		WSACleanup();
	}

	void joinMulticastGroup(const char* ipAddr, int port) {
		sockaddr_in dstAddr {};
		dstAddr.sin_family = AF_INET;
		dstAddr.sin_addr.s_addr = INADDR_ANY;
		dstAddr.sin_port = htons(port);

		if(bind(m_socket, (sockaddr*)&dstAddr, sizeof(dstAddr)) == SOCKET_ERROR)
			throw runtime_error("bind failed");

		// send IGMP join request
		ip_mreq mreq {};
		mreq.imr_multiaddr.s_addr = inet_addr(ipAddr);
		mreq.imr_interface.s_addr = htonl(INADDR_ANY);

		if(setsockopt(m_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&mreq, sizeof(mreq)) < 0)
			throw runtime_error("can't join multicast address");
	}

	size_t receive(uint8_t* buffer, size_t dstlen) {
		sockaddr_in addr;
		auto addrSize = (int)sizeof(addr);

		auto len = recvfrom(m_socket,
		        (char*)buffer, dstlen,
		        0,
		        (sockaddr*)&addr, &addrSize);

		if(len == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK)
			return {}; // no data available yet

		if(len < 0)
			throw runtime_error("recvfrom failed");

		return len;
	}

	SOCKET m_socket = INVALID_SOCKET;
};

std::unique_ptr<ISocket> createSocket() {
	return make_unique<Socket>();
}

