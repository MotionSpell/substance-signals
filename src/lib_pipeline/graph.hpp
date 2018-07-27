#pragma once

#include <algorithm>
#include <cassert>
#include <vector>

namespace Pipelines {

struct IPipelinedModule;

struct Graph {
	struct Node {
		typedef IPipelinedModule* NodeId;
		NodeId id;
	};

	struct Connection {
		Node src;
		int srcPort;
		Node dst;
		int dstPort;
	};

	Node& nodeFromId(Node::NodeId &id) {
		auto i_node = std::find_if(nodes.begin(), nodes.end(), [id](Pipelines::Graph::Node const& n) {
			return n.id == id;
		});
		assert(i_node != nodes.end());
		return *i_node;
	}

	std::vector<Node> nodes;
	std::vector<Connection> connections;
};

}
