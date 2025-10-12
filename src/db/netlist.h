#pragma once
#include "global.h"
#include "device.h"
#include "db/connection.h"
#include <zlib.h>
#include "capnp/serialize.h"
#include "capnp/serialize-packed.h"
#include "kj/std/iostream.h"
#include "PhysicalNetlist.capnp.h"
#include "db/routeNodeGraph.h"
#include "db/routeNode.h"
#include "routeResult.h"
#include <filesystem>

namespace Raw {
class Netlist {
public:
	Netlist(Device& dvc, vector<Net>& nets_, vector<Connection>& indirectConnections_, vector<Connection>& directConnections_, vector<bool>& preservedNodes_, RouteNodeGraph& routingGraph_, utils::BoxT<int> layout_) : 
		device(dvc),
		nets(nets_),
		indirectConnections(indirectConnections_),
		directConnections(directConnections_),
		preservedNodes(preservedNodes_),
		routingGraph(routingGraph_),
		layout(layout_)
		{};
    ~Netlist();
    void read(string netlist_file);
    void write(string netlist_file, const vector<RouteResult>& nodeRoutingResults);
    void writeToFile(string netlist_file);

    int connNum;
	int netNum;
	int numThread = 16;

	vector<bool>& preservedNodes;
	utils::BoxT<int> layout;

	string netlist_filename;

	void copyBranch(PhysicalNetlist::PhysNetlist::RouteBranch::Builder src, PhysicalNetlist::PhysNetlist::RouteBranch::Builder tgt);
	void clearData() {
		nets.clear();
		indirectConnections.clear();
		directConnections.clear();
		routingGraph.routeNodes.clear();
	}

private:
	void updateNetAndConnectionBBox();
	void loadFile(string netlist_file);
	void parseNetlist(string netlist_file);
	void extract_site_pins(std::vector<std::pair<str_idx, str_idx>>& site_pins, capnp::List<PhysicalNetlist::PhysNetlist::RouteBranch>::Reader branches);
    void extract_site_pins_one_by_one(std::vector<std::pair<str_idx, str_idx>>& site_pins, capnp::List<PhysicalNetlist::PhysNetlist::RouteBranch>::Reader branches);
	vector<obj_idx> project_input_node_to_int_node(obj_idx sink_node_idx);
	obj_idx project_output_node_to_int_node(obj_idx src_node_idx, vector<obj_idx>& path);
    int min3(int n1, int n2, int n3) {
        int min = n1;
        min = (n2 < min ? n2 : min);
        min = (n3 < min ? n3 : min);
        return min;
    }
    int max3(int n1, int n2, int n3) {
        int max = n1;
        max = (n2 > max ? n2 : max);
        max = (n3 > max ? n3 : max);
        return max;
    }

    bool is_clk_net(string& net_name) {
        return (net_name.find("CLK_OUT") != net_name.npos ||
                net_name.find("CLKOUT") != net_name.npos ||
                net_name.find("CLKFBOUT") != net_name.npos);
    }

    Device& device;
	vector<Net>& nets;
	vector<Connection>& indirectConnections;
	vector<Connection>& directConnections;
	RouteNodeGraph& routingGraph;
    int multi_src_net_num = 0;
    int indirect_conn_num = 0;
    int direct_conn_num = 0;


	::capnp::List< ::capnp::Text,  ::capnp::Kind::BLOB>::Reader str_list;
	::capnp::List< ::PhysicalNetlist::PhysNetlist::PhysNet,  ::capnp::Kind::STRUCT>::Reader phys_nets;

	capnp::MallocMessageBuilder netlist_builder;
};

};