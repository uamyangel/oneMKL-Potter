#include "netlist.h"
#include <queue>
#include <fstream>
#include <string>
#include <thread>
#include "utils/MTStat.h"

namespace Raw {

void Netlist::read(std::string netlist_file) 
{
    log() << "reading physical netlist..." << std::endl;
    log() << std::endl;
	parseNetlist(netlist_file);
	updateNetAndConnectionBBox();

    log(1) << "nets           : " << netNum << std::endl;
    log(1) << "multi-src nets : " << multi_src_net_num << std::endl;
    log(1) << "connections    : " << indirect_conn_num + direct_conn_num << std::endl;
    log(1) << "indirect connections: " << indirect_conn_num << std::endl;
    log(1) << "direct connections  : " << direct_conn_num << std::endl;
    log(1) << std::endl;
}

obj_idx Netlist::project_output_node_to_int_node(obj_idx src_node_idx, vector<obj_idx>& path) 
{
    if (src_node_idx == invalid_obj_idx) return invalid_obj_idx;
    int watchdog = 5;
    std::deque<obj_idx> q;
    q.push_back(src_node_idx);
    unordered_map<obj_idx, obj_idx> prev_map;
	prev_map[src_node_idx] = invalid_obj_idx;
    unordered_map<obj_idx, int> layer;
    layer[src_node_idx] = watchdog;
    while (!q.empty()) {
        obj_idx node_idx = q.front();
        auto it = layer.find(node_idx);
        int node_layer = it->second;
        q.pop_front();
        assert(device.nodeInfos[node_idx].tileType != INT);
        if (node_layer < 0) continue;
        const auto& downhills = device.get_outgoing_nodes(node_idx);
        if (downhills.empty()) continue;

        // q.clear();
        for (obj_idx downhill: downhills) {
            if (device.nodeInfos[downhill].tileType == INT) {
				obj_idx prev_node_idx = node_idx;
            	while (prev_node_idx != invalid_obj_idx) {
            	    path.insert(path.begin(), prev_node_idx);
            	    prev_node_idx = prev_map[prev_node_idx];
            	}
                return node_idx;
            }
			prev_map[downhill] = node_idx;
            q.push_back(downhill);
            layer[downhill] = node_layer - 1;
        }
    }

    return invalid_obj_idx;
};

void Netlist::extract_site_pins(std::vector<std::pair<str_idx, str_idx>>& site_pins, capnp::List<PhysicalNetlist::PhysNetlist::RouteBranch>::Reader branches) 
{
	std::queue<PhysicalNetlist::PhysNetlist::RouteBranch::Reader> queue;
	for (auto route_branch: branches) {
		queue.push(route_branch);
	}

	while (!queue.empty()) {
		auto& route_branch = queue.front();
		queue.pop();
		auto route_segment = route_branch.getRouteSegment();
		if (route_segment.which() == PhysicalNetlist::PhysNetlist::RouteBranch::RouteSegment::Which::SITE_PIN) {
			auto sp = route_segment.getSitePin();
			site_pins.emplace_back(sp.getSite(), sp.getPin());
		}
		for (auto branch: route_branch.getBranches()) {
			queue.push(branch);
		}
	}
};

void Netlist::extract_site_pins_one_by_one(std::vector<std::pair<str_idx, str_idx>>& site_pins, capnp::List<PhysicalNetlist::PhysNetlist::RouteBranch>::Reader branches) 
{
    for (auto route_branch: branches) {
        bool extracted = false;
        std::queue<PhysicalNetlist::PhysNetlist::RouteBranch::Reader> queue;
		queue.push(route_branch);
        while (!queue.empty()) {
	    	auto& route_branch = queue.front();
	    	queue.pop();
	    	auto route_segment = route_branch.getRouteSegment();
	    	if (route_segment.which() == PhysicalNetlist::PhysNetlist::RouteBranch::RouteSegment::Which::SITE_PIN) {
	    		auto sp = route_segment.getSitePin();
	    		site_pins.emplace_back(sp.getSite(), sp.getPin());
                extracted = true;
                break;
	    	}
	    	for (auto branch: route_branch.getBranches()) {
	    		queue.push(branch);
	    	}
	    }
	}
};

vector<obj_idx> Netlist::project_input_node_to_int_node(obj_idx sink_node_idx) 
{
    vector<obj_idx> path; // sink to switch box path
    unordered_map<obj_idx, obj_idx> prev_map;

    if (sink_node_idx == invalid_obj_idx) return path;

    std::deque<obj_idx> q;
    int watchdog = 1000;
    q.push_back(sink_node_idx);
    prev_map[sink_node_idx] = invalid_obj_idx;

    while (!q.empty()) {
        obj_idx node_idx = q.front();
        q.pop_front();
        if (device.nodeInfos[node_idx].tileType == INT) {
            // path.push_back(node_idx);
            while (node_idx != invalid_obj_idx) {
                path.push_back(node_idx);
                node_idx = prev_map[node_idx];
            }
            return path;
        }
        for (obj_idx uphill: device.get_incoming_nodes(node_idx)) {
            if (device.get_incoming_nodes(uphill).size() == 0) continue;
            prev_map[uphill] = node_idx;
            q.push_back(uphill);
        }
        watchdog --;
        if (watchdog < 0) break;
    }

    return path;
};

void Netlist::parseNetlist(string netlist_file)
{
    // Start reading the raw netlist file
    gzFile file = gzopen(netlist_file.c_str(), "r");
    assert(file != Z_NULL);

    vector<uint8_t> buf_data(4096);
    std::stringstream sstream(std::ios_base::in | std::ios_base::out | std::ios_base::binary);
    while (true) {
        int ret = gzread(file, buf_data.data(), buf_data.size());
        assert(ret >= 0);
        if (ret > 0) {
            sstream.write((const char*)buf_data.data(), ret);
            assert(sstream);
        } else {
            assert(ret == 0);
            int error;
            gzerror(file, &error);
            assert(error == Z_OK);
            break;
        }
    }

    assert(gzclose(file) == Z_OK);
    sstream.seekg(0);

    kj::std::StdInputStream istream(sstream);
	std::stringstream sstreamOut(sstream.str()); // copy for the use of message builder

    // Reader options
    capnp::ReaderOptions reader_options;
    reader_options.nestingLimit = std::numeric_limits<int>::max();
    reader_options.traversalLimitInWords = std::numeric_limits<uint64_t>::max();

    capnp::InputStreamMessageReader message_reader(istream, reader_options);
	// store original physical netlist to netlist_builder
	sstreamOut.seekg(0);
    kj::std::StdInputStream istream2(sstreamOut);
	capnp::readMessageCopy(istream2, netlist_builder, reader_options);

    auto netlist_reader = message_reader.getRoot<PhysicalNetlist::PhysNetlist>();
    str_list = netlist_reader.getStrList();
    phys_nets = netlist_reader.getPhysNets();

    log(1) << "str_list       : " << str_list.size() << std::endl;
    log(1) << "phys_nets      : " << phys_nets.size() << std::endl;

    std::filesystem::path netlist_filepath = netlist_file;
    netlist_filename = netlist_filepath.filename();

    connNum = 0;
    netNum = 0;
	directConnections.reserve(phys_nets.size());
	indirectConnections.reserve(phys_nets.size() * 100);
    preservedNodes.resize(device.nodeNum, false);
    nets.reserve(phys_nets.size());
    for (obj_idx net_idx = 0; net_idx < phys_nets.size(); net_idx++) {
        const auto& phys_net = phys_nets[net_idx];
        const auto& stub_nodes = phys_net.getStubNodes();
        const auto& sources = phys_net.getSources();
        const auto& stubs = phys_net.getStubs();
        const auto& type = phys_net.getType();
        assert_t(stub_nodes.size() == 0);
        vector<std::pair<str_idx, str_idx>> source_pins;
        vector<std::pair<str_idx, str_idx>> sink_pins;
        // extract all SitePins of source branch
        extract_site_pins(source_pins, sources);
    
        if (type == PhysicalNetlist::PhysNetlist::NetType::SIGNAL and stubs.size() > 0) {
            // A signal net
            // Only extract one sink SitePin from a sink branch (stub) and keep the order
            extract_site_pins_one_by_one(sink_pins, stubs);

            if (sink_pins.empty()) {
                continue;
            }
            if (source_pins.empty()) {
                // reserve_site_pins_for_net(sink_pins, net_idx);
                for (auto& sink_pin: sink_pins) {
                    obj_idx sink_node_idx = device.get_site_pin_node(str_list[sink_pin.first], str_list[sink_pin.second]);
                    if (device.nodeInfos[sink_node_idx].tileType == INT) {
						preservedNodes[sink_node_idx] = true;
                    }                    
                }
                continue;
            }
            if (source_pins.size() > 1) {
                multi_src_net_num += 1;
                // continue; // GLOBUSED_NET, SKIPPED
            }

        	nets.emplace_back(nets.size());
            string site_name;
            string pin_name;

            /**
             * The distinction between indirect/direct connections follows RWRoute's approach.
             * Indirect connections are regular connections. Their routing path begins at the source CLB, traverses through INT tiles, and ultimately terminates at the target CLB.
             * Direct connections​ are connections that do not require nodes on INT tiles (such as carry chains). As a result, they have a much smaller routing search space and do not constitute a major part of the routing process.
             */
            obj_idx src_node_idx = invalid_obj_idx;
            obj_idx src_int_node_idx = invalid_obj_idx;
            obj_idx alt_src_node_idx = invalid_obj_idx;
            obj_idx alt_src_int_node_idx = invalid_obj_idx;

            vector<obj_idx> src_node_idx_cands;
            for (std::pair<str_idx, str_idx>& src_pin: source_pins) {
                site_name = str_list[src_pin.first];
                pin_name = str_list[src_pin.second];
                // get a source node candidate from a source site pin
                obj_idx src_node_idx_cand = device.get_site_pin_node(site_name, pin_name);
                if (src_node_idx_cand != invalid_obj_idx) {
                    src_node_idx_cands.emplace_back(src_node_idx_cand);
                }
            }

            // debug ->
            if (src_node_idx_cands.size() > 2) {
                log(LOG_ERROR) << "src_node_idx_cands.size() " << src_node_idx_cands.size() << endl;
                exit(1);
            }
            // debug <-

            src_node_idx = src_node_idx_cands[0];
			vector<obj_idx> src_path;
			vector<obj_idx> alt_src_path;
            src_int_node_idx = project_output_node_to_int_node(src_node_idx, src_path);
            if (src_node_idx_cands.size() > 1) {
                alt_src_node_idx = src_node_idx_cands[1];
                alt_src_int_node_idx = project_output_node_to_int_node(alt_src_node_idx, alt_src_path);
            }

            for (std::pair<str_idx, str_idx>& sink_pin : sink_pins) {
                string site_name = str_list[sink_pin.first].cStr();
                string pin_name = str_list[sink_pin.second].cStr();
                obj_idx sink_node_idx = device.get_site_pin_node(site_name, pin_name);
                obj_idx real_src_node_idx;
                obj_idx real_sink_node_idx;
                vector<obj_idx> path;
                path.clear();
                path = project_input_node_to_int_node(sink_node_idx);
                bool is_indirect_connect = (path.size() > 0) && (src_int_node_idx != invalid_obj_idx || alt_src_int_node_idx != invalid_obj_idx);

                if (is_indirect_connect) {
                    /* only write indirect conns */
					vector<obj_idx> real_src_path;
                    if (src_int_node_idx != invalid_obj_idx) {
                        real_src_node_idx = src_int_node_idx;
						real_src_path = src_path;
                    } else {
                        if (alt_src_int_node_idx  == invalid_obj_idx) {
                            log(LOG_ERROR) << "invalid src_int_node_idx and alt_src_int_node_idx for indirect conn" << endl;
                            exit(1);
                        }
                        real_src_node_idx = alt_src_int_node_idx;
						real_src_path = alt_src_path;
                    }
                    real_sink_node_idx = path[0];
                    routingGraph.routeNodes[real_src_node_idx].setNodeType(PINFEED_O);
                    routingGraph.routeNodes[real_sink_node_idx].setNodeType(PINFEED_I);

					indirectConnections.emplace_back(
						indirect_conn_num, // TODO: remove one indirect_conn_num
						netNum, real_src_node_idx, real_sink_node_idx
					);
					indirectConnections.back().setIntToSinkPath(path);
					indirectConnections.back().setSourceToIntPath(real_src_path);

					indirectConnections.back().setSourceRNode(&routingGraph.routeNodes[real_src_node_idx]); // TODO: hide this or replace 
					indirectConnections.back().setSinkRNode(&routingGraph.routeNodes[real_sink_node_idx]);
					nets[netNum].addConns(indirect_conn_num);
					nets[netNum].setIndirectSourceRNode(&routingGraph.routeNodes[real_src_node_idx]);
					nets[netNum].addIndirectSinkRNode(&routingGraph.routeNodes[real_sink_node_idx]);
                	if (is_indirect_connect && src_int_node_idx == invalid_obj_idx)
						nets[netNum].setIndirectSourcePinRNode(&routingGraph.routeNodes[alt_src_node_idx]);
					else 
						nets[netNum].setIndirectSourcePinRNode(&routingGraph.routeNodes[src_node_idx]);
					nets[netNum].addIndirectSinkPinRNode(&routingGraph.routeNodes[sink_node_idx]);

                    connNum ++;
                    indirect_conn_num ++;
                } else {                        
					assert_t(src_node_idx != sink_node_idx);
                    real_src_node_idx = src_node_idx;
                    real_sink_node_idx = sink_node_idx;
                    routingGraph.routeNodes[real_src_node_idx].setNodeType(PINFEED_O);
                    // routingGraph.routeNodes[real_sink_node_idx].setNodeType(PINFEED_I);
                    // preservedNodes[real_src_node_idx] = true;
                    // preservedNodes[real_sink_node_idx] = true;
					directConnections.emplace_back(direct_conn_num, netNum, real_src_node_idx, real_sink_node_idx);

					directConnections.back().setSourceRNode(&routingGraph.routeNodes[real_src_node_idx]);
					directConnections.back().setSinkRNode(&routingGraph.routeNodes[real_sink_node_idx]);
					nets[netNum].addDirectConns(direct_conn_num);
					nets[netNum].setDirectSourcePinRNode(&routingGraph.routeNodes[real_src_node_idx]);
					nets[netNum].addDirectSinkPinRNode(&routingGraph.routeNodes[real_sink_node_idx]);
                    direct_conn_num ++;

                }
            }
			nets[netNum].setId(netNum);
			nets[netNum].setOriId(net_idx);
            netNum ++;
        } else {
            // preserve clk and static net
            string phys_net_name = str_list[phys_net.getName()];
            bool is_clk_net = false;
            for (auto& source_pin: source_pins) {
                string pin_name = str_list[source_pin.second];
                if (pin_name.find("CLK_OUT")  != pin_name.npos ||
                    pin_name.find("CLKOUT")   != pin_name.npos ||
                    pin_name.find("CLKFBOUT") != pin_name.npos) {
                    is_clk_net = true;    
                }
            }

            bool is_static_net = (phys_net.getType() == PhysicalNetlist::PhysNetlist::NetType::GND || phys_net.getType() == PhysicalNetlist::PhysNetlist::NetType::VCC);
            if (!is_clk_net && !is_static_net) continue;

            log() << "preserve net: " << phys_net_name << endl;

            // preserve pins
            for (auto& source_pin: source_pins) {
                obj_idx node_idx = device.get_site_pin_node(str_list[source_pin.first], str_list[source_pin.second]);
                if (device.nodeInfos[node_idx].tileType == INT) {
        			preservedNodes[node_idx] = true;
                }
            }

            for (auto& sink_pin: sink_pins) {
                obj_idx node_idx = device.get_site_pin_node(str_list[sink_pin.first], str_list[sink_pin.second]);
                if (device.nodeInfos[node_idx].tileType == INT) {
        			preservedNodes[node_idx] = true;
                }
            }
            
            // preserve PIPs
            std::queue<PhysicalNetlist::PhysNetlist::RouteBranch::Reader> q;
            for (const auto& rb_src: sources) {
                q.emplace(rb_src);
            }
            while (!q.empty()) {
                const auto& rb = q.front();
                const auto& rs = rb.getRouteSegment();
                if (rs.isPip()) {
                    string tile_name = str_list[rs.getPip().getTile()].cStr();
                    string wire_0_name = str_list[rs.getPip().getWire0()].cStr();
                    obj_idx node_0_idx = device.get_node_idx(tile_name, wire_0_name);
                    if (device.nodeInfos[node_0_idx].tileType == INT) {
        				preservedNodes[node_0_idx] = true;
                    }
                    
                    string wire_1_name = str_list[rs.getPip().getWire1()].cStr();
                    obj_idx node_1_idx = device.get_node_idx(tile_name, wire_1_name);
                    if (device.nodeInfos[node_1_idx].tileType == INT) {
        				preservedNodes[node_1_idx] = true;
                    }
                }
                
                if (rb.hasBranches()) {
                    for (const auto& child: rb.getBranches()) {
                        q.emplace(child);
                    }
                }
                q.pop();
            }
        }
    }
}

void Netlist::updateNetAndConnectionBBox() 
{
    vector<int> x_center(netNum, 0);
    vector<int> y_center(netNum, 0);
    vector<int> double_hpwl(netNum, 0);

    for (int i = 0; i < netNum; i++) {
        if (nets[i].getConnectionSize() == 0) continue;
		auto conns = nets[i].getConnections();
        int cnt = 0;
        int x_min = INT32_MAX;
        int x_max = INT32_MIN;
        int y_min = INT32_MAX;
        int y_max = INT32_MIN;
		RouteNode& src_rnode = routingGraph.routeNodes[nets[i].getIndirectSource()];
        double x_sum = src_rnode.getEndTileXCoordinate(); // add source x
        double y_sum = src_rnode.getEndTileYCoordinate();
        cnt ++;
        for (int conn_idx: conns) {
			RouteNode& snk_rnode = routingGraph.routeNodes[indirectConnections[conn_idx].getSink()];
            x_sum += snk_rnode.getEndTileXCoordinate();
            y_sum += snk_rnode.getEndTileYCoordinate();

            x_min = std::min(x_min, (int)src_rnode.getEndTileXCoordinate());
            x_min = std::min(x_min, (int)snk_rnode.getEndTileXCoordinate());

            x_max = std::max(x_max, (int)src_rnode.getEndTileXCoordinate());
            x_max = std::max(x_max, (int)snk_rnode.getEndTileXCoordinate());

            y_min = std::min(y_min, (int)src_rnode.getEndTileYCoordinate());
            y_min = std::min(y_min, (int)snk_rnode.getEndTileYCoordinate());

            y_max = std::max(y_max, (int)src_rnode.getEndTileYCoordinate());
            y_max = std::max(y_max, (int)snk_rnode.getEndTileYCoordinate());
            
            cnt ++;
        }
        x_center[i] = (int)std::ceil(x_sum / cnt);
        y_center[i] = (int)std::ceil(y_sum / cnt);
        double_hpwl[i] = std::max(0, 2 * (std::abs(y_max - y_min + 1) + std::abs(x_max - x_min + 1)));        
    }

    for (int i = 0; i < connNum; i++) {
		auto& conn = indirectConnections[i];
		int netId = conn.getNetId();
		RouteNode* src = conn.getSourceRNode();
		RouteNode* snk = conn.getSinkRNode();
		int x_min = min3(x_center[netId], src->getEndTileXCoordinate(), snk->getEndTileXCoordinate());
        if (x_min < 0) x_min = -1;
		int x_max = max3(x_center[netId], src->getEndTileXCoordinate(), snk->getEndTileXCoordinate());
		if (x_max > layout.hx()) x_max = layout.hx();
		int y_min = min3(y_center[netId], src->getEndTileYCoordinate(), snk->getEndTileYCoordinate());
        if (y_min < 0) y_min = -1;
		int y_max = max3(y_center[netId], src->getEndTileYCoordinate(), snk->getEndTileYCoordinate());
		if (y_max > layout.hy()) y_max = layout.hy();
		conn.setXMin(x_min);
		conn.setXMax(x_max);
		conn.setYMin(y_min);
		conn.setYMax(y_max);
		conn.updateBBox();
		conn.computeHPWL();

        Net& net = nets[netId];
        net.updateXMinBB(x_min);
        net.updateXMaxBB(x_max);
        net.updateYMinBB(y_min);
        net.updateYMaxBB(y_max);
    }
	for (int i = 0; i < netNum; i ++) {
    	nets[i].setCenter(x_center[i], y_center[i]);
    	nets[i].setDoubleHpwl(double_hpwl[i]);
	}
}

void Netlist::write(string netlist_file, const vector<RouteResult>& nodeRoutingResults) 
{
	utils::timer timer; timer.start();
	log() << "Dump routing solution into netlist_builder [Start]" << std::endl;
	auto netlist = netlist_builder.getRoot<PhysicalNetlist::PhysNetlist>();
	auto str_list = netlist.getStrList();
    auto phys_nets = netlist.getPhysNets();
    std::unordered_map<std::string, str_idx> string2StrIdx;
	for (int i = 0; i < str_list.size(); i++) {
        auto s = std::string(str_list[i].cStr());
        string2StrIdx[s] = i;
    }
	// new string could be added into str_list
	vector<string> new_str_list;
	unordered_map<str_idx, int> new_str_id_map;
	str_idx old_str_len = str_list.size();
	auto getStringIndex = [&old_str_len, &new_str_list, &new_str_id_map](std::pair<str_idx, string> d) {
		str_idx id = d.first;
		if (new_str_id_map.find(id) == new_str_id_map.end()) {
			new_str_list.emplace_back(d.second);
			new_str_id_map[id] = old_str_len + new_str_list.size() - 1;
		}
		return new_str_id_map[id];
	};

	int numPIPs = 0;
	int numNetFail = 0;
	for (int ni = 0; ni < netNum; ni ++) {
        if (nets[ni].getIsSubNet()) continue;

		obj_idx net_idx = nets[ni].getOriId();
        auto phys_net = phys_nets[net_idx];
        // auto sources = phys_net.getSources();
        auto stubs = phys_net.getStubs();

		// Disown all sink pins from the stubs list
		std::unordered_map<obj_idx, capnp::Orphan<capnp::List<PhysicalNetlist::PhysNetlist::RouteBranch, capnp::Kind::STRUCT>>> sinkPin2orphan;
		std::unordered_map<obj_idx, int> sinkPinStub;
		if (stubs.size() != nets[ni].getConnectionSize() + nets[ni].getDirectConnectionSize()) {
			std::cout << ni << " " << net_idx << " " << stubs.size() << " " << nets[ni].getConnectionSize() << " " << nets[ni].getDirectConnectionSize() << endl;
		}
		assert_t(stubs.size() == nets[ni].getConnectionSize() + nets[ni].getDirectConnectionSize());
		for (int i = 0; i < stubs.size(); i ++) {
			auto rs = stubs[i].getRouteSegment();
            if (rs.which() != PhysicalNetlist::PhysNetlist::RouteBranch::RouteSegment::Which::SITE_PIN) continue;
			auto sp = rs.getSitePin();
   			obj_idx nodeId = device.get_site_pin_node(str_list[sp.getSite()].cStr(), str_list[sp.getPin()].cStr());
			if (routingGraph.routeNodes[nodeId].getNodeType() != PINFEED_I) {
				std::cout << routingGraph.routeNodes[nodeId].getNodeType() << " " << PINFEED_I << std::endl;
				exit(0);
			}
			// sinkPin2orphan[nodeId] = stubs[i].disownBranches();
			sinkPinStub[nodeId] = i;
		}
		// phys_net.disownStubs();

		// Walk through all net sources until a source site pin is found
		std::queue<PhysicalNetlist::PhysNetlist::RouteBranch::Builder> sourceQueue;
		for (auto s : phys_net.getSources()) sourceQueue.push(s);
		int routedPinNum = 0;
		while (!sourceQueue.empty()) {
			auto rb = sourceQueue.front(); sourceQueue.pop();
			for (auto s : rb.getBranches()) sourceQueue.push(s);
			auto rs = rb.getRouteSegment();
			if (rs.which() != PhysicalNetlist::PhysNetlist::RouteBranch::RouteSegment::Which::SITE_PIN) 
				continue;
			auto sp = rs.getSitePin();
   			obj_idx nodeId = device.get_site_pin_node(str_list[sp.getSite()].cStr(), str_list[sp.getPin()].cStr());
			RouteNode* sourceNode = &routingGraph.routeNodes[nodeId];
			if (nodeRoutingResults[sourceNode->getId()].netId != ni) 
				// Source pin was not used by this net
				continue;
			// Starting at this source node, walk forward through the graph following the next-nodes used by this net
			std::queue<std::pair<PhysicalNetlist::PhysNetlist::RouteBranch::Builder, RouteNode*>> graphQueue;
			graphQueue.emplace(rb, sourceNode);
			while (!graphQueue.empty()) {
				auto item = graphQueue.front(); graphQueue.pop();
				PhysicalNetlist::PhysNetlist::RouteBranch::Builder rb = item.first;
				RouteNode* rnode = item.second;
				if (nodeRoutingResults[rnode->getId()].netId != ni) {
					std::cout << "Net " << nodeRoutingResults[rnode->getId()].netId << " " << ni << std::endl;
					assert_t(0);
				}
				int nodeId = rnode->getId();
				// std::set<RouteNode*> nextRNodes = rnode->getBranch();
				vector<RouteNode*> nextRNodes;
				for (RouteNode* nn: nodeRoutingResults[rnode->getId()].branches) {
					nextRNodes.emplace_back(nn);
				}
				assert_t(rb.getBranches().size() == 0);
				::capnp::List< ::PhysicalNetlist::PhysNetlist::RouteBranch,  ::capnp::Kind::STRUCT>::Builder branches;
				if (rnode->getNodeType() == PINFEED_I) {
					// This node is a sink site pin that must be present on this net: move its corresponding stub as this node's last branch
					branches = rb.initBranches(nextRNodes.size() + 1);
					// assert_t(sinkPin2orphan.find(nodeId) != sinkPin2orphan.end());
					// capnp::Orphan<capnp::List<PhysicalNetlist::PhysNetlist::RouteBranch, capnp::Kind::STRUCT>>& orphan = sinkPin2orphan[nodeId];
					// branches[branches.size() - 1].adoptBranches(kj::mv(orphan)); // TODO: verify
					auto b = branches[branches.size() - 1];
					copyBranch(stubs[sinkPinStub[nodeId]], b);
					// b.adoptBranches(kj::mv(orphan));
					// sinkPin2orphan.erase(nodeId);
					routedPinNum ++;
				} else {
					// Not a site pin, must have nextNodes
					assert(nextRNodes.size() != 0);
					branches = rb.initBranches(nextRNodes.size());
				}
				// For every edge to a next-node, recover the PIP and add it as a routing branch
				int i = 0;
				for (auto child: nextRNodes) {
					auto nextRb = branches[i++];
					auto pip = nextRb.getRouteSegment().initPip();
					const TileTypePIP& pip_ = device.getTileTypePIP(rnode->getId(), child->getId());
					obj_idx tile_id = device.nodeInfos[child->getId()].beginTileId;
					pip.setTile(getStringIndex(device.get_tile_name(tile_id)));
					pip.setWire0(getStringIndex(device.get_wire_name(tile_id, pip_.wire0_it_idx)));
					pip.setWire1(getStringIndex(device.get_wire_name(tile_id, pip_.wire1_it_idx)));
					pip.setForward(pip_.forward);
					graphQueue.emplace(nextRb, child);
					numPIPs ++;
				}
			}
		}
		if (routedPinNum != stubs.size()) {
			numNetFail ++;
			log(LOG_ERROR) << "There are unrouted pins " << stubs.size() << " vs " << routedPinNum << std::endl;
			std::cout << nets[ni].getConnectionSize() << " " << nets[ni].getDirectConnectionSize() << std::endl;
			for (auto rnode : nets[ni].getIndirectSinkPinRNodes()) {
				std::cout << nodeRoutingResults[rnode->getId()].netId << " " << ni << " | " << (rnode->getNodeType() == PINFEED_I) << std::endl;
			}
			for (auto rnode : nets[ni].getDirectSinkPinRNodes()) {
				std::cout << nodeRoutingResults[rnode->getId()].netId << " " << ni << " | " << (rnode->getNodeType() == PINFEED_I) << std::endl;
			}
			exit(0);
		}
		phys_net.disownStubs();
    }
	// Initialize a new strList entry (capnp does not support resizing an existing list).
    // Rather than copying the underlying string text, detach the pointer
    //  ("disown") them from the existing list and reference ("adopt") them in the new list.
	vector<::capnp::Orphan<::capnp::Text>> orphan_str_list; orphan_str_list.reserve(old_str_len);
	for (int i = 0; i < old_str_len; i ++) {
		orphan_str_list.emplace_back(str_list.disown(i));
	}
	netlist.disownStrList();
	::capnp::List< ::capnp::Text,  ::capnp::Kind::BLOB>::Builder  merged_str_list = netlist.initStrList(old_str_len + new_str_list.size());
	for (int i = 0; i < old_str_len; i ++) {
		merged_str_list.adopt(i, kj::mv(orphan_str_list[i]));
	}
	for (int i = 0; i < new_str_list.size(); i ++) {
        // New string that didn't exist in the unrouted design
		merged_str_list.set(i + old_str_len, (::capnp::Text::Builder) (const_cast<char*>(new_str_list[i].c_str())));
	}
	log() << "NewStrNum: " << new_str_list.size() << std::endl;

	if (numNetFail != 0) {
		log() << numNetFail << " / " << netNum << " nets have unrouted pins" << std::endl;
		exit(0);
	}
	auto cld= [this] (int tid) {
		for (int i = tid; i < 8; i += numThread) {
			if (i == 0) clearData();
			else device.clearData(i - 1);
		}
	};
	vector<std::thread> jobs;
	log() << "Dump routing solution into netlist_builder [Finish]" << std::endl;
	for (int tid = 0; tid < numThread; tid ++) jobs.emplace_back(cld, tid);
	writeToFile(netlist_file);
	std::cout << "Write time: " << std::fixed << std::setprecision(2) << timer.elapsed() << std::endl;
	for (int i = 0; i < jobs.size(); i ++)
		jobs[i].join();
}

void Netlist::copyBranch(PhysicalNetlist::PhysNetlist::RouteBranch::Builder src, PhysicalNetlist::PhysNetlist::RouteBranch::Builder tgt)
{
	auto src_rs = src.getRouteSegment();
	auto tgt_rs = tgt.getRouteSegment();
	if (src_rs.which() == PhysicalNetlist::PhysNetlist::RouteBranch::RouteSegment::Which::SITE_PIN) {
		auto src_sp = src_rs.getSitePin();
		auto tgt_sp = tgt_rs.initSitePin();
		tgt_sp.setSite(src_sp.getSite());	
		tgt_sp.setPin (src_sp.getPin());	
	} else if (src_rs.which() == PhysicalNetlist::PhysNetlist::RouteBranch::RouteSegment::Which::BEL_PIN) {
		auto src_bp = src_rs.getBelPin();
		auto tgt_bp = tgt_rs.initBelPin();
		tgt_bp.setSite(src_bp.getSite());
		tgt_bp.setBel (src_bp.getBel());
		tgt_bp.setPin (src_bp.getPin());
	} else if (src_rs.which() == PhysicalNetlist::PhysNetlist::RouteBranch::RouteSegment::Which::PIP) {
		auto src_p = src_rs.getPip();
		auto tgt_p = tgt_rs.initPip();
		tgt_p.setTile   (src_p.getTile());
		tgt_p.setWire0  (src_p.getWire0());
		tgt_p.setWire1  (src_p.getWire1());
		tgt_p.setForward(src_p.getForward());
		tgt_p.setIsFixed(src_p.getIsFixed());
		tgt_p.setNoSite (src_p.getNoSite());
		tgt_p.setSite   (src_p.getSite());
	} else {
		auto src_sp = src_rs.getSitePIP();
		auto tgt_sp = tgt_rs.initSitePIP();
		tgt_sp.setSite       (src_sp.getSite());
		tgt_sp.setBel        (src_sp.getBel());
		tgt_sp.setPin        (src_sp.getPin());
		tgt_sp.setIsFixed    (src_sp.getIsFixed());
		tgt_sp.setIsInverting(src_sp.getIsInverting());
		tgt_sp.setInverts    (src_sp.getInverts());
	}
	auto src_branches = src.getBranches();
	if (src_branches.size() > 0) {
		auto tgt_branches = tgt.initBranches(src_branches.size());
		for (int i = 0; i < src_branches.size(); i ++)
			copyBranch(src_branches[i], tgt_branches[i]);
	}
}

void Netlist::writeToFile(string netlist_file) {
	log() << "Write to file " << netlist_file << " [Start]" << std::endl;
	// Compress GZipped capnproto physical netlist file
    gzFile file = gzopen(netlist_file.c_str(), "w");
    assert(file != Z_NULL);

    std::stringstream sstream(std::ios_base::in | std::ios_base::out | std::ios_base::binary);
	kj::std::StdOutputStream ostream(sstream);
	writeMessage(ostream, netlist_builder);

	unsigned long int size = sizeof(char) * sstream.str().size();
	int compressionLevel = 0;  // Specify the desired compression level (0-9): get the max speed but the largest file
    int ret = gzsetparams(file, compressionLevel, Z_DEFAULT_STRATEGY);
    assert_t(ret == Z_OK);
	gzwrite(file, (void*) (sstream.str().data()), size);
	gzclose(file);
	log() << "Write to file [Finish]" << std::endl;
}

Netlist::~Netlist() {
    log() << "netlist destruct" << endl;
}

};
