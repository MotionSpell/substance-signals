#pragma once

#include <algorithm>
#include <cassert>
#include <list>

namespace Pipelines {

struct IPipelinedModule;

struct Graph {
	struct Node {
		typedef IPipelinedModule* NodeId;
		Node(NodeId id) : id(id) {}
		NodeId id;
	};

	struct Connection {
		Connection(Node src, int srcPort, Node dst, int dstPort)
			: src(src), dst(dst), srcPort(srcPort), dstPort(dstPort) {}
		Node src, dst;
		int srcPort, dstPort;
	};

	Node& nodeFromId(Node::NodeId &id) {
		auto i_node = std::find_if(nodes.begin(), nodes.end(), [id](Pipelines::Graph::Node const& n) {
			return n.id == id;
		});
		assert(i_node != nodes.end());
		return *i_node;
	}

	std::list<Node> nodes;
	std::list<Connection> connections;
};

}
