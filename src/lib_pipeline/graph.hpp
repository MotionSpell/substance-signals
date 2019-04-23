#pragma once

#include <algorithm>
#include <cassert>
#include <vector>
#include <string>

namespace Pipelines {

struct Graph {
	struct Node {
		typedef void* NodeId;
		NodeId id;
		std::string caption;
	};

	struct Connection {
		Node src;
		int srcPort;
		Node dst;
		int dstPort;
	};

	Node& nodeFromId(Node::NodeId id) {
		auto i_node = std::find_if(nodes.begin(), nodes.end(), [id](Pipelines::Graph::Node const& n) {
			return n.id == id;
		});
		if (i_node == nodes.end()) {
			nodes.push_back({ id, "Unknown" });
			return nodes.back();
		}

		return *i_node;
	}

	std::vector<Node> nodes;
	std::vector<Connection> connections;
};

}
