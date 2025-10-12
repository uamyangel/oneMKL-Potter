#include <zlib.h>
#include <unordered_set>
#include "device.h"

#include <fstream>

namespace Raw {

vector<obj_idx> Device::get_outgoing_nodes(obj_idx node_idx) {
    vector<obj_idx> outgoing_nodes;
    for (Wire& wire: node_to_wires[node_idx]) {
        if (wire.tile_type_idx == NULL_TILE) continue;
        for (obj_idx child_wire_it_idx: tile_type_outgoing_wires[wire.tile_type_idx][wire.wire_in_tile_idx]) {
            obj_idx child_idx = tile_wire_to_node[wire.tile_idx][child_wire_it_idx];
            if (child_idx != invalid_obj_idx) {
                outgoing_nodes.emplace_back(child_idx);
            }
        }
    }
    return outgoing_nodes;
}

vector<obj_idx> Device::get_incoming_nodes(obj_idx node_idx) {
    vector<obj_idx> incoming_nodes;
    for (obj_idx wire_idx = 0; wire_idx < node_to_wires[node_idx].size(); wire_idx++) {
        Wire& wire = node_to_wires[node_idx][wire_idx];
        for (obj_idx parent_wire_it_idx: tile_type_incoming_wires[wire.tile_type_idx][wire.wire_in_tile_idx]) {
            obj_idx parent_idx = tile_wire_to_node[wire.tile_idx][parent_wire_it_idx];
            if (parent_idx != invalid_obj_idx) {
                incoming_nodes.emplace_back(parent_idx);
                // if (wire_idx != 0) {
                //     log() << node_names[parent_idx] << " -> " << node_names[node_idx] << " using the " << wire_idx << "-th wire of " << node_names[node_idx] << endl;
                // }
            }
        }
    }
    return incoming_nodes;
}

void Device::add_tile_type_pip(obj_idx tile_type_idx, obj_idx tile_type_wire_0_idx, obj_idx tile_type_wire_1_idx, bool directional)
{
    tile_type_outgoing_wires[tile_type_idx][tile_type_wire_0_idx].push_back(tile_type_wire_1_idx);
    tile_type_incoming_wires[tile_type_idx][tile_type_wire_1_idx].push_back(tile_type_wire_0_idx);
	TileTypePIP pip(tile_type_wire_0_idx, tile_type_wire_1_idx, true);
    tile_type_pip_list[tile_type_idx].emplace_back(pip);
    auto uid = utils::ints2long(tile_type_wire_0_idx, tile_type_wire_1_idx);
    assert_t(tile_type_node_pair_to_pip_idx[tile_type_idx].find(uid) == tile_type_node_pair_to_pip_idx[tile_type_idx].end());
	tile_type_node_pair_to_pip_idx[tile_type_idx][uid] = tile_type_pip_list[tile_type_idx].size() - 1;
    
    if (!directional) {
        tile_type_outgoing_wires[tile_type_idx][tile_type_wire_1_idx].push_back(tile_type_wire_0_idx);
        tile_type_incoming_wires[tile_type_idx][tile_type_wire_0_idx].push_back(tile_type_wire_1_idx);
        TileTypePIP pip(tile_type_wire_0_idx, tile_type_wire_1_idx, false);
        tile_type_pip_list[tile_type_idx].emplace_back(pip);
        uid = utils::ints2long(tile_type_wire_1_idx, tile_type_wire_0_idx);
        assert_t(tile_type_node_pair_to_pip_idx[tile_type_idx].find(uid) == tile_type_node_pair_to_pip_idx[tile_type_idx].end());
        tile_type_node_pair_to_pip_idx[tile_type_idx][uid] = tile_type_pip_list[tile_type_idx].size() - 1;
	}
}

obj_idx Device::get_site_pin_node(string site_name, string pin_name) const {
    auto it = site_name_to_idx.find(site_name);
    assert(it != site_name_to_idx.end());
    obj_idx site_idx = it->second;
    const Site& site = sites[site_idx];
    it = site_type_pin_name_to_idx[site.site_type_idx].find(pin_name);
    assert(it != site_type_pin_name_to_idx[site.site_type_idx].end());
    obj_idx pin_idx = it->second;
    obj_idx tile_type_wire_idx = 
        tile_type_site_pin_to_wire_idx[site.tile_type_idx][site.in_tile_site_idx][pin_idx];
    obj_idx node_idx = tile_wire_to_node[site.tile_idx][tile_type_wire_idx];
    return node_idx;
}


void Device::read(std::string device_file) 
{
    log() << "Didn't find device cache. Start reading." << endl;
    log() << std::endl;
    gzFile file = gzopen(device_file.c_str(), "r");
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

    // reader options
    capnp::ReaderOptions reader_options;
    reader_options.nestingLimit = std::numeric_limits<int>::max();
    reader_options.traversalLimitInWords = std::numeric_limits<uint64_t>::max();

    capnp::InputStreamMessageReader message_reader(istream, reader_options);
    auto device_reader = message_reader.getRoot<DeviceResources::Device>();
    
	// raw data
	const auto& str_list = device_reader.getStrList();
	const auto& tile_list = device_reader.getTileList();
	const auto& tile_type_list = device_reader.getTileTypeList();
	const auto& site_type_list = device_reader.getSiteTypeList();
	const auto& wire_list = device_reader.getWires();
	const auto& wire_type_list = device_reader.getWireTypes();
	const auto& node_list = device_reader.getNodes();

    log(1) << "device name    : " << string(device_reader.getName()) << std::endl;
    log(1) << "str_list       : " << str_list.size() << std::endl;
    log(1) << "tile_list      : " << tile_list.size() << std::endl;
    log(1) << "tile_type_list : " << tile_type_list.size() << std::endl;
    log(1) << "site_type_list : " << site_type_list.size() << std::endl;
    log(1) << "wire_list      : " << wire_list.size() << std::endl;
    log(1) << "wire_type_list : " << wire_type_list.size() << std::endl;
    log(1) << "node_list      : " << node_list.size() << std::endl;
    // create routing graph
    const int node_num = node_list.size();

    nodeNum = node_list.size();

    log() << "Allocating memory for " << nodeNum << " nodes..." << std::endl;

    try {
        routingGraph.routeNodes.resize(nodeNum);
        log() << "Successfully allocated routeNodes" << std::endl;

        nodeInfos.resize(nodeNum);
        log() << "Successfully allocated nodeInfos" << std::endl;
    } catch (const std::bad_alloc& e) {
        log() << "ERROR: Failed to allocate memory for " << nodeNum << " nodes" << std::endl;
        log() << "Each RouteNode requires ~64 bytes, total needed: ~" << (nodeNum * 64 / 1024 / 1024) << " MB" << std::endl;
        log() << "Consider using a smaller design or increasing system memory" << std::endl;
        throw;
    }
	log() << "Starting to set node IDs..." << std::endl;
	for (int i = 0; i < nodeNum; i ++) {
		if (i % 10000000 == 0) {
			log() << "Processing node " << i << " of " << nodeNum << std::endl;
		}
		routingGraph.routeNodes[i].setId(i);
	}
	log() << "Finished setting node IDs" << std::endl;

    int wire_num = 0;

    log() << "About to check memory peak" << std::endl;
    check_memory_peak(0);
    log() << "begin build string mapping" << endl;
    string_list.reserve(str_list.size());
    log() << "Reserved string_list for " << str_list.size() << " strings" << endl;
    for (str_idx i = 0; i < str_list.size(); i++) {
        if (i % 100000 == 0) {
            log() << "Processing string " << i << " of " << str_list.size() << std::endl;
        }
        string_list.emplace_back(str_list[i].cStr());
        string_to_idx[str_list[i].cStr()] = i;
    }
    log() << "end build string mapping" << endl;
    check_memory_peak(1);

    // vector<unordered_map<str_idx, obj_idx>> tile_type_wire_str_to_idx(tile_type_list.size());
    tile_type_wire_to_name_idx.resize(tile_type_list.size());
    tile_type_wire_str_to_idx.resize(tile_type_list.size());
    for (obj_idx tile_type_idx = 0; tile_type_idx < tile_type_list.size(); tile_type_idx++) {
        const auto& tile_type = tile_type_list[tile_type_idx];
        const auto& tile_type_wires = tile_type.getWires();
        tile_type_wire_to_name_idx[tile_type_idx].resize(tile_type_wires.size());
        for (obj_idx wire_idx = 0; wire_idx < tile_type_wires.size(); wire_idx++) {
            str_idx wire_str = tile_type_wires[wire_idx];
            tile_type_wire_str_to_idx[tile_type_idx][wire_str] = wire_idx;
            tile_type_wire_to_name_idx[tile_type_idx][wire_idx] = wire_str;
        }
    }

    tile_wire_to_node.resize(tile_list.size());

    x_max = 0;
    y_max = 0;
    tile_x_coor.resize(tile_list.size());
    tile_y_coor.resize(tile_list.size());
    tile_to_name_idx.resize(tile_list.size());
    tile_to_type.resize(tile_list.size());
    for (obj_idx tile_idx = 0; tile_idx < tile_list.size(); tile_idx++) {
        const auto& tile = tile_list[tile_idx];
        tile_to_name_idx[tile_idx] = tile.getName();
        obj_idx tile_type_idx = tile.getType();
        tile_to_type[tile_idx] = tile_type_idx;
        if (tile_type_idx == NULL_TILE) continue;
        tile_str_to_idx[tile.getName()] = tile_idx;
        const auto& tile_type = tile_type_list[tile_type_idx];
        string tile_type_str = str_list[tile_type.getName()].cStr();
        const auto& tile_wires = tile_type.getWires();
        tile_wire_to_node[tile_idx].assign(tile_wires.size(), invalid_obj_idx);
        std::string tile_str = str_list[tile.getName()].cStr();
        tile_x_coor[tile_idx] = getTileX(tile_str);
        tile_y_coor[tile_idx] = getTileY(tile_str);
        if (tile_x_coor[tile_idx] < 0) std::cout << tile_x_coor[tile_idx] << std::endl;
        if (tile_y_coor[tile_idx] < 0) std::cout << tile_y_coor[tile_idx] << std::endl;
        x_max = std::max(x_max, tile_x_coor[tile_idx]);
        y_max = std::max(y_max, tile_y_coor[tile_idx]);
    }
    log(1) << "x_max, y_max   : " << x_max << ", " << y_max << std::endl;

    uint32_t max_degree = 7;
    vector<int> node_degree_cnt(max_degree + 1, 0);
    int node_end_tile_num = 0;

    site_type_pin_name_to_idx.resize(site_type_list.size());
    for (obj_idx site_type_idx = 0; site_type_idx < site_type_list.size(); site_type_idx++) {
        unordered_map<string, obj_idx>& pin_name_to_idx = site_type_pin_name_to_idx[site_type_idx];
        const auto& site_type = site_type_list[site_type_idx];
        const auto& pins = site_type.getPins();
        for (obj_idx pin_idx = 0; pin_idx < pins.size(); pin_idx++) {
            const auto& pin = pins[pin_idx];
            string pin_name = str_list[pin.getName()];
            pin_name_to_idx.emplace(pin_name, pin_idx);
        }
    }
    
    site_name_to_idx.reserve(tile_list.size());
    sites.reserve(tile_list.size());
    for (obj_idx tile_idx = 0; tile_idx < tile_list.size(); tile_idx++) {
        const auto& tile = tile_list[tile_idx];
        obj_idx tile_type_idx = tile.getType();
        const auto& tile_type = tile_type_list[tile_type_idx];
        const auto& tile_type_site_types = tile_type.getSiteTypes();
        const auto& tile_sites = tile.getSites();
        for (int in_tile_site_idx = 0; in_tile_site_idx < tile_sites.size(); in_tile_site_idx++) {
            const auto& tile_site = tile_sites[in_tile_site_idx];
            obj_idx site_idx = sites.size();
            obj_idx tile_site_type_idx = tile_type_site_types[in_tile_site_idx].getPrimaryType();
            string tile_site_name = str_list[tile_site.getName()];
            sites.emplace_back(tile_idx, tile_type_idx, in_tile_site_idx, tile_site_type_idx);
            site_name_to_idx.emplace(tile_site_name, site_idx);
        }
    }
    log(1) << "sites          : " << sites.size() << std::endl;
    
    tile_type_site_pin_to_wire_idx.resize(tile_type_list.size());
    // build site pin to node mapping
    for (obj_idx tile_type_idx = 0; tile_type_idx < tile_type_list.size(); tile_type_idx++) {
        vector<vector<obj_idx>>& site_pin_to_wire_idx = tile_type_site_pin_to_wire_idx[tile_type_idx];
        const auto& tile_type = tile_type_list[tile_type_idx];
        const auto& site_types = tile_type.getSiteTypes();
        site_pin_to_wire_idx.resize(site_types.size());
        for (obj_idx i = 0; i < site_types.size(); i++) {
            vector<obj_idx>& pin_to_wire_idx = site_pin_to_wire_idx[i];
            const auto& site_type = site_types[i];
            const auto& pins = site_type_list[site_type.getPrimaryType()].getPins();
            const auto& pin_to_tile_wire = site_type.getPrimaryPinsToTileWires();
            pin_to_wire_idx.reserve(pins.size());
            for (obj_idx pin_idx = 0; pin_idx < pins.size(); pin_idx++) {
                const str_idx pin_str = pins[pin_idx].getName();
                const str_idx tile_type_wire_str = pin_to_tile_wire[pin_idx];
                const obj_idx tile_type_wire_idx = tile_type_wire_str_to_idx[tile_type_idx][tile_type_wire_str];
                pin_to_wire_idx.emplace_back(tile_type_wire_idx);
            }
        }
    }

    // tile & wire str -> idx mapping
    for (obj_idx tile_idx = 0; tile_idx < tile_list.size(); tile_idx++) {
        const auto& tile = tile_list[tile_idx];
        if (tile.getType() != NULL_TILE) {
            string tile_name = str_list[tile.getName()];
            tile_name_to_idx[tile_name] = tile_idx;
            tile_name_to_type[tile_name] = tile.getType();
        }
    }
    wire_name_to_idx_in_tile_type.resize(tile_type_list.size());
    for (obj_idx tile_type_idx = 0; tile_type_idx < tile_type_list.size(); tile_type_idx++) {
        const auto& tile_type = tile_type_list[tile_type_idx];
        const auto& tile_type_wires = tile_type.getWires();
        for (obj_idx wire_idx = 0; wire_idx < tile_type_wires.size(); wire_idx ++) {
            string wire_name = str_list[tile_type_wires[wire_idx]];
            wire_name_to_idx_in_tile_type[tile_type_idx][wire_name] = wire_idx;
        }
    }

    node_to_wires.resize(node_list.size());
    
    for (obj_idx node_idx = 0; node_idx < node_list.size(); node_idx++) {
        const auto& node_wires = node_list[node_idx].getWires();
        wire_num += node_wires.size();
        obj_idx end_tile_idx = -1;

        obj_idx begin_wire_idx = node_wires[0];
        const auto& begin_wire = wire_list[begin_wire_idx];
        str_idx begin_tile_str_idx = begin_wire.getTile();
        str_idx begin_wire_str_idx = begin_wire.getWire();

        auto it = tile_str_to_idx.find(begin_tile_str_idx);
        assert(it != tile_str_to_idx.end());
        obj_idx begin_tile_idx = it->second;
        const auto& begin_tile = tile_list[begin_tile_idx];

        nodeInfos[node_idx].beginTileId = begin_tile_idx;
        routingGraph.routeNodes[node_idx].setBeginTileXCoordinate(tile_x_coor[begin_tile_idx]);
        routingGraph.routeNodes[node_idx].setBeginTileYCoordinate(tile_y_coor[begin_tile_idx]);
        nodeInfos[node_idx].intentCode = begin_wire.getType();
        nodeInfos[node_idx].tileType = begin_tile.getType();

		int d = tile_type_wire_str_to_idx[begin_tile.getType()][begin_wire_str_idx];
		bool v = (d >= 51 && d <= 82) || (d >= 179 && d <= 338); // Hard code, used in routingGraph
        routingGraph.routeNodes[node_idx].setIsAccesibleWire(v);
        routingGraph.routeNodes[node_idx].setIsNodePinBounce((nodeInfos[node_idx].intentCode == NODE_PINBOUNCE) ? 1 : 0);

        // default settings
        // nodeAttrPtrs[CONSIDERED][node_idx] = 0; // false
        nodeInfos[node_idx].laguna = 0; // false

        bool finding_end_tile = true;
        for (obj_idx wire_idx : node_wires) {
            const auto& wire = wire_list[wire_idx];
            str_idx tile_str_idx = wire.getTile();
            str_idx wire_str_idx = wire.getWire();
            obj_idx tile_idx = tile_str_to_idx[tile_str_idx];
            const auto& tile = tile_list[tile_idx];
            obj_idx tile_type_idx = tile.getType();
            obj_idx tile_type_wire_idx = tile_type_wire_str_to_idx[tile_type_idx][wire_str_idx];
            tile_wire_to_node[tile_idx][tile_type_wire_idx] = node_idx;
            Wire wire_(tile_idx, tile_type_idx, tile_type_wire_idx);
            node_to_wires[node_idx].emplace_back(wire_);

            if ((tile_type_idx == INT || tile_type_idx == begin_tile.getType()) and finding_end_tile) {
                bool end_tile_was_not_null = (end_tile_idx != -1);
                end_tile_idx = tile_idx;
                if (end_tile_was_not_null) {
                    finding_end_tile = false; // stop finding if it is the second INT tile
                }
            }
        }
        node_degree_cnt[std::min(max_degree, node_wires.size())] += 1;

        if (end_tile_idx != -1) {
            nodeInfos[node_idx].endTileId = end_tile_idx;
            routingGraph.routeNodes[node_idx].setEndTileXCoordinate(tile_x_coor[end_tile_idx]);
            routingGraph.routeNodes[node_idx].setEndTileYCoordinate(tile_y_coor[end_tile_idx]);
            node_end_tile_num += 1;
        } else {
            nodeInfos[node_idx].endTileId = begin_tile_idx;
            routingGraph.routeNodes[node_idx].setEndTileXCoordinate(tile_x_coor[begin_tile_idx]);
            routingGraph.routeNodes[node_idx].setEndTileYCoordinate(tile_y_coor[begin_tile_idx]);
        }
    }
    log(2) << std::endl;
    log(2) << "node end tiles : " << node_end_tile_num << std::endl;
    log(2) << "node wires     : " << wire_num << std::endl;
    log(2) << std::endl;
    log(2) << "node degree" << "   " << " ocurrance  " << std::endl;
    for (uint32_t i = 0; i <= max_degree; i++) {
        if (node_degree_cnt[i] != 0) {
            if (i != max_degree) {
                log(2) << std::setw(12) << std::left << i;
            } else {
                log(2) << ">=" << std::setw(10) << std::left << i;
            }
            std::cout << "   " << std::setw(12) << std::left  << node_degree_cnt[i] << std::endl;
        }
    }
    log(2) << std::endl;

    check_memory_peak(2);
    // outgoing_nodes.resize(node_list.size());
    // incoming_nodes.resize(node_list.size());
    int pip_num = 0;
    edgeNum = 0;
	// unordered_map<str_idx, obj_idx> tile_name_2_tile_idx;
    // for (obj_idx tile_idx = 0; tile_idx < tile_list.size(); tile_idx++)
	// 	tile_name_2_tile_idx[tile_list[tile_idx].getName()] = tile_idx;
	auto isNodeIncluded= [&] (obj_idx node_idx) {
		const auto& wires = node_list[node_idx].getWires();
		obj_idx tile_idx = tile_str_to_idx[wire_list[wires[0]].getTile()];
		auto tile_type = tile_list[tile_idx].getType();
		return tile_type == INT || tile_type == LAG_LAG;
	};
	node_in_allowed_tile.resize(node_num, false);
	for (obj_idx i = 0; i < node_num; i ++)
		node_in_allowed_tile[i] = isNodeIncluded(i);
	nodes_in_graph.resize(node_num, false);

    for (obj_idx tile_idx = 0; tile_idx < tile_list.size(); tile_idx++) {
        const auto& tile = tile_list[tile_idx];
        auto tile_type_idx = tile.getType();
        if (tile_type_idx == NULL_TILE) continue;
        const auto& tile_type = tile_type_list[tile_type_idx];
        const auto& tile_wires = tile_type.getWires();
        const vector<obj_idx> wire_to_node = tile_wire_to_node[tile_idx];
    }
    check_memory_peak(3);
    log() << "build tile_type pip list begin" << endl;
    tile_type_outgoing_wires.resize(tile_type_list.size());       
    tile_type_incoming_wires.resize(tile_type_list.size());
    tile_type_pip_list.resize(tile_type_list.size()); 
    tile_type_node_pair_to_pip_idx.resize(tile_type_list.size());    
    for (obj_idx tile_type_idx = 0; tile_type_idx < tile_type_list.size(); tile_type_idx++) {
        if (tile_type_idx == NULL_TILE) continue;
        const auto& tile_type = tile_type_list[tile_type_idx];
        const auto& pips = tile_type.getPips();
        tile_type_outgoing_wires[tile_type_idx].resize(tile_type.getWires().size());
        tile_type_incoming_wires[tile_type_idx].resize(tile_type.getWires().size());
        // tile_type_pip_list[tile_type_idx].resize(pips.size());
        for (obj_idx pip_idx = 0; pip_idx < pips.size(); pip_idx ++) {
            const auto& pip = pips[pip_idx];
            obj_idx w0_in_tile_idx = pip.getWire0();
            obj_idx w1_in_tile_idx = pip.getWire1();
            add_tile_type_pip(tile_type_idx, w0_in_tile_idx, w1_in_tile_idx, pip.getDirectional());
        }
    }
    log() << "build tile_type pip list end" << endl;
    
    // log(1) << "pips           : " << pip_num << std::endl;
    // log(1) << "edges          : " << edgeNum << std::endl;
    log(1) << std::endl;

    check_memory_peak(4);

    int ecnt = 0;
    int ind_ecnt = 0;
    int dir_ecnt = 0;
    std::ofstream ofs;
    for (obj_idx node_idx = 0; node_idx < nodeNum; node_idx++) {
        vector<obj_idx> children = get_outgoing_nodes(node_idx);
        for (obj_idx child_idx: children) {
            if ((nodeInfos[child_idx].tileType == INT || nodeInfos[child_idx].tileType == LAG_LAG) && node_in_allowed_tile[node_idx] && node_in_allowed_tile[child_idx]) {
                // routingGraph.routeNodes[node_idx].addChildren(&routingGraph.routeNodes[child_idx]);
				nodes_in_graph[node_idx] = true;
				nodes_in_graph[child_idx] = true;
                ind_ecnt ++;
            } else {
                // routingGraph.routeNodes[node_idx].addDirectChildren(&routingGraph.routeNodes[child_idx]);
                dir_ecnt ++;
            }
        }
        ecnt += children.size();    
    }
    log(1) << "edges          : " << ecnt << std::endl;
    log(1) << "indirect edges : " << ind_ecnt << std::endl;
    log(1) << "direct edges   : " << dir_ecnt << std::endl;
    check_memory_peak(5);

    log() << "Building LAGUNA_I mapping" << std::endl;
    // lagunaI from rwr, index by [y][x]
    vector<vector<int>> laguna_tile_indices(y_max + 1, vector<int>(x_max + 1, -1));
    
    obj_idx col_max = 0;
    for (obj_idx tile_idx = 0; tile_idx < tile_list.size(); tile_idx++) {
        const auto& tile = tile_list[tile_idx];
        auto col = tile.getCol();
        col_max = col > col_max ? col : col_max;

        auto tile_type_idx = tile.getType();
        if (tile_type_idx == LAG_LAG) {
            laguna_tile_indices[tile_y_coor[tile_idx]][tile_x_coor[tile_idx]] = tile_idx;
        }
    }
    
    const auto& lag_tile_type = tile_type_list[LAG_LAG];
    const auto& lag_tile_type_wires = lag_tile_type.getWires();
    auto int_tile_wire_cnt = tile_type_list[INT].getWires().size();
    
    vector<bool> lag_tile_wire_UBUMP(lag_tile_type_wires.size());
    for (obj_idx wire_idx = 0; wire_idx < lag_tile_type_wires.size(); wire_idx++) {
        string wire_str = str_list[lag_tile_type_wires[wire_idx]];
        lag_tile_wire_UBUMP[wire_idx] = (wire_str.find("UBUMP") == 0);
    }

    if (laguna_tile_indices.size() > 0) {
        vector<int> next_laguna_column(col_max, INT_MAX);
        vector<int> prev_laguna_column(col_max, INT_MIN);
        // debug ->
        int lag_cnt = 0;
        // debug <-
        for (int y = 0; y < laguna_tile_indices.size(); y ++) {
            for (int x = 0; x < laguna_tile_indices[y].size(); x ++) {
                int tile_idx = laguna_tile_indices[y][x];
                if (tile_idx == -1) continue;
                const auto& tile = tile_list[tile_idx];
                if (y == 0) {
                    assert(x == tile_x_coor[tile_idx]);
                    int int_tile_x_coor = x + 1;

                    for (int i = int_tile_x_coor; i >= 0; i--) {
                        if (next_laguna_column[i] != INT_MAX) {
                            break;
                        }
                        next_laguna_column[i] = int_tile_x_coor;
                    }
                    for (int i = int_tile_x_coor; i < prev_laguna_column.size(); i++) {
                        prev_laguna_column[i] = int_tile_x_coor;
                    }
                }
                // Examine all wires in Laguna tile. Record those uphill of a Super Long Line
                // that originates in an INT tile (and thus must be a NODE_PINFEED).
                obj_idx int_tile_idx = invalid_obj_idx;
                for (obj_idx wire_idx = 0; wire_idx < lag_tile_type_wires.size(); wire_idx ++) {
                    // PRW-contest ->
                    
                    if (lag_tile_wire_UBUMP[wire_idx] == false) {
                        continue;
                    }
                    obj_idx sll_node_idx = tile_wire_to_node[tile_idx][wire_idx];
                    for (obj_idx uphill1_idx: get_incoming_nodes(sll_node_idx)) {
                        for (obj_idx uphill2_idx: get_incoming_nodes(uphill1_idx)) {
                            if (nodeInfos[uphill2_idx].tileType != INT) continue;
                            assert(nodeInfos[uphill2_idx].intentCode == NODE_PINFEED);
                            nodeInfos[uphill2_idx].laguna = 1;
                            lag_cnt ++;
                        }
                    }

                    // PRW-contest <-

                    // cufr-rwroute ->
                    // obj_idx node_idx = tile_wire_to_node[tile_idx][wire_idx];
                    // if (node_idx != invalid_obj_idx && nodeInfos[node_idx].intentCode == NODE_PINFEED) {
                    //     assert(tile_list[tile_idx].getType() == INT);
                    //     if (int_tile_idx == invalid_obj_idx) {
                    //         int_tile_idx = tile_idx;
                    //     } else {
                    //         assert(int_tile_idx == tile_idx);
                    //     }
                    //     nodeInfos[node_idx].laguna = 1;
                    // }
                    // cufr-rwroute <-
                }
            }
        }
        log() << "lag_cnt: " << lag_cnt << endl;
    }

    log() << "LAGUNA_I mapping built" << std::endl;

    log() << "Begin get node type" << std::endl;

    auto startsWith = [](string str, string sub) {
        return (str.find(sub) == 0);
    };
    
    auto endsWith = [](string str, string sub) {
        int idx = str.rfind(sub);
        return (idx == (str.length() - sub.length())) && (idx != str.npos);
    };

    auto get_node_type = [&](obj_idx node_idx) {
        // NOTE: IntentCode is device-dependent
        int ic = nodeInfos[node_idx].intentCode;
        const auto& node = node_list[node_idx];
        const auto& base_wire = wire_list[node.getWires()[0]];
        string base_wire_name = str_list[base_wire.getWire()].cStr();
        switch (ic) {
            case NODE_PINBOUNCE:
                return PINBOUNCE;

            case NODE_PINFEED:
                // BitSet bs = (lagunaI != null) ? lagunaI.get(node.getTile()) : null;
                if (nodeInfos[node_idx].laguna) {
                    return LAGUNA_I;
                } 
                // else {
                //     return PINFEED_I;
                // }
                break;

            case NODE_LAGUNA_OUTPUT: // UltraScale+ only
                assert(nodeInfos[node_idx].tileType == LAG_LAG);
                // log() << "base_wire_name: " << base_wire_name << " laguna_I: " << endsWith(base_wire_name, "_TXOUT") << endl;
                if (endsWith(base_wire_name, "_TXOUT")) {
                    return LAGUNA_I;
                }
                break;

            case NODE_LAGUNA_DATA: // UltraScale+ only
                assert(nodeInfos[node_idx].tileType == LAG_LAG);
                if (nodeInfos[node_idx].beginTileId != nodeInfos[node_idx].endTileId) {
                    return SUPER_LONG_LINE;
                }

                // U-turn node at the boundary of the device
                break;
        }

        return WIRE;
    };

    for (obj_idx node_idx = 0; node_idx < node_list.size(); node_idx++) {
        routingGraph.routeNodes[node_idx].setNodeType(get_node_type(node_idx));      
    }

    log() << "Begin update coordinate info" << std::endl;

    auto getEndTileXCoordinate = [&](obj_idx node_idx) {
        int base_type = nodeInfos[node_idx].tileType;
        int node_type = routingGraph.routeNodes[node_idx].getNodeType();
        int end_tile_X = routingGraph.routeNodes[node_idx].getEndTileXCoordinate();
        int ic = nodeInfos[node_idx].intentCode;
        if (base_type == LAG_LAG) {
            // UltraScale+ only
            // Correct the X coordinate of all Laguna nodes since they are accessed by the INT
            // tile to its right, yet the LAG tile has a tile X coordinate one less than this.
            // Do not apply this correction for NODE_LAGUNA_OUTPUT nodes (which the fanin and
            // fanout nodes of the SLL are marked as) unless it is a fanin (LAGUNA_I)
            // (i.e. do not apply it to the fanout nodes).
            // Nor apply it to VCC_WIREs since their end tiles are INT tiles.
            // if ((ic != NODE_LAGUNA_OUTPUT || node_type == LAGUNA_I_ROUTENODETYPE) && !node.isTiedToVcc()) {
            
            /* TODO: deal with TiedVcc*/
            const auto& base_wire = wire_list[node_list[node_idx].getWires()[0]];
            string base_wire_name = str_list[base_wire.getWire()].cStr();
            if ((ic != NODE_LAGUNA_OUTPUT || node_type == LAGUNA) && !startsWith(base_wire_name, "VCC") && !startsWith(base_wire_name, "RXD")) {
                end_tile_X++;
            }
        }
        return end_tile_X;
    };

    auto getEndTileYCoordinate = [&](obj_idx node_idx) {
        bool reverseSLL = (routingGraph.routeNodes[node_idx].getNodeType() == SUPER_LONG_LINE);
                // prev != null &&
                // prev.endTileYCoordinate == endTileYCoordinate);
        return reverseSLL ? routingGraph.routeNodes[node_idx].getBeginTileYCoordinate() : routingGraph.routeNodes[node_idx].getEndTileYCoordinate();
    };

    auto getBeginTileXCoordinate = [&](obj_idx node_idx) {
        // For US+ Laguna tiles, use end tile coordinate as that's already been corrected
        // (see RouteNodeInfo.getEndTileXCoordinate())
        return (nodeInfos[node_idx].tileType == LAG_LAG) ? routingGraph.routeNodes[node_idx].getEndTileXCoordinate() : routingGraph.routeNodes[node_idx].getBeginTileXCoordinate();
    };

    for (obj_idx node_idx = 0; node_idx < node_list.size(); node_idx++) {
        routingGraph.routeNodes[node_idx].setEndTileXCoordinate(getEndTileXCoordinate(node_idx));
        routingGraph.routeNodes[node_idx].setEndTileYCoordinate(getEndTileYCoordinate(node_idx));
        routingGraph.routeNodes[node_idx].setBeginTileXCoordinate(getBeginTileXCoordinate(node_idx));
    }

    log() << "Begin get node length" << std::endl;

    auto get_abs = [](int a) {
        return (a >= 0 ? a : -a);
    };
    auto get_length = [&](obj_idx node_idx) {
        int node_type = routingGraph.routeNodes[node_idx].getNodeType();
        int tile_type = nodeInfos[node_idx].tileType;
        // short length = (short) Math.abs(endTileYCoordinate - baseTile.getTileYCoordinate());
        int length = get_abs(routingGraph.routeNodes[node_idx].getBeginTileYCoordinate() - routingGraph.routeNodes[node_idx].getEndTileYCoordinate());
        if (tile_type != LAG_LAG) {
            length += get_abs(routingGraph.routeNodes[node_idx].getEndTileXCoordinate() - routingGraph.routeNodes[node_idx].getBeginTileXCoordinate());
        }
        return length;
    };

    for (obj_idx node_idx = 0; node_idx < node_list.size(); node_idx++) {
        routingGraph.routeNodes[node_idx].setLength(get_length(node_idx));      
    }
    
    
    auto get_node_base_cost = [&](obj_idx node_idx) {
        int type = routingGraph.routeNodes[node_idx].getNodeType();
        int ic = nodeInfos[node_idx].intentCode;
        int length = routingGraph.routeNodes[node_idx].getLength();
        int end_tile_X = routingGraph.routeNodes[node_idx].getEndTileXCoordinate();
        int beg_tile_X = routingGraph.routeNodes[node_idx].getBeginTileXCoordinate();
        int base_cost = 40;
        switch (type) {
            case LAGUNA_I:
                // Make all approaches to SLLs zero-cost to encourage exploration
                // Assigning a base cost of zero would normally break congestion resolution
                // (since RWroute.getNodeCost() would return zero) but doing it here should be
                // okay because this node only leads to a SLL which will have a non-zero base cost
                base_cost = 0;
                break;
            case SUPER_LONG_LINE:
                // assert(length == SUPER_LONG_LINE_LENGTH_IN_TILES);
                base_cost = 30 * length;
                break;
            case WIRE:
                // NOTE: IntentCode is device-dependent
                switch(ic) {
                    case NODE_OUTPUT:       // LUT route-thru
                    case NODE_CLE_OUTPUT:
                    case NODE_LAGUNA_OUTPUT:
                    case NODE_LAGUNA_DATA:  // US+: U-turn SLL at the boundary of the device
                    // case NODE_PINFEED:
                        // assert(length == 0);
                        break;
                    case NODE_LOCAL:
                    case INTENT_DEFAULT:
                        // assert(length <= 1);
                        break;
                    case NODE_SINGLE:
                        assert(length <= 2);
                        if (length == 2) base_cost *= length;
                        break;
                    case NODE_DOUBLE:
                        if (end_tile_X != beg_tile_X) {
                            assert(length <= 2);
                            // Typically, length = 1 (since tile X is not equal)
                            // In US, have seen length = 2, e.g. VU440's INT_X171Y827/EE2_E_BEG7.
                            if (length == 2) base_cost *= length;
                        } else {
                            // Typically, length = 2 except for horizontal U-turns (length = 0)
                            // or vertical U-turns (length = 1).
                            // In US, have seen length = 3, e.g. VU440's INT_X171Y827/NN2_E_BEG7.
                            assert(length <= 3);
                        }
                        break;
                    case NODE_HQUAD:
                        // assert(length != 0 || get_outgoing_nodes(node_idx).empty());
                        base_cost = 35 * length;
                        break;
                    case NODE_VQUAD:
                        // In case of U-turn nodes
                        if (length != 0) base_cost = 15 * length;// VQUADs have length 4 and 5
                        break;
                    case NODE_HLONG:
                        assert(length != 0 || get_outgoing_nodes(node_idx).empty());
                        base_cost = 15 * length;// HLONGs have length 6 and 7
                        break;
                    case NODE_VLONG:
                        base_cost = 70;
                        break;
                }
                break;
            case PINFEED_I:
            case PINBOUNCE:
                break;
            case PINFEED_O:
                base_cost = 100;
                break;
        }
        return base_cost;
    };

    int node_vsingle_cnt = 0;
    int node_hsingle_cnt = 0;
    int node_vdouble_cnt = 0;
    int node_hdouble_cnt = 0;
    int node_vquad_cnt = 0;
    int node_hquad_cnt = 0;
    int node_vlong_cnt = 0;
    int node_hlong_cnt = 0;

    int node_vsingle_total_length = 0;
    int node_hsingle_total_length = 0;
    int node_vdouble_total_length = 0;
    int node_hdouble_total_length = 0;
    int node_vquad_total_length = 0;
    int node_hquad_total_length = 0;
    int node_vlong_total_length = 0;
    int node_hlong_total_length = 0;

    double node_vsingle_total_cost = 0;
    double node_hsingle_total_cost = 0;
    double node_vdouble_total_cost = 0;
    double node_hdouble_total_cost = 0;
    double node_vquad_total_cost = 0;
    double node_hquad_total_cost = 0;
    double node_vlong_total_cost = 0;
    double node_hlong_total_cost = 0;

    for (obj_idx node_idx = 0; node_idx < nodeNum; node_idx++) {
        routingGraph.routeNodes[node_idx].setBaseCost(get_node_base_cost(node_idx) / 100.0);
        if (nodeInfos[node_idx].intentCode == NODE_SINGLE && routingGraph.routeNodes[node_idx].getEndTileXCoordinate() == routingGraph.routeNodes[node_idx].getBeginTileXCoordinate()) {
            node_vsingle_cnt ++;
            node_vsingle_total_length += routingGraph.routeNodes[node_idx].getLength();
            node_vsingle_total_cost += routingGraph.routeNodes[node_idx].getBaseCost();
        } else {
            node_hsingle_cnt ++;
            node_hsingle_total_length += routingGraph.routeNodes[node_idx].getLength();
            node_hsingle_total_cost += routingGraph.routeNodes[node_idx].getBaseCost();
        }

        if (nodeInfos[node_idx].intentCode == NODE_DOUBLE && routingGraph.routeNodes[node_idx].getEndTileXCoordinate() == routingGraph.routeNodes[node_idx].getBeginTileXCoordinate()) {
            node_vdouble_cnt ++;
            node_vdouble_total_length += routingGraph.routeNodes[node_idx].getLength();
            node_vdouble_total_cost += routingGraph.routeNodes[node_idx].getBaseCost();
        } else {
            node_hdouble_cnt ++;
            node_hdouble_total_length += routingGraph.routeNodes[node_idx].getLength();
            node_hdouble_total_cost += routingGraph.routeNodes[node_idx].getBaseCost();
        }

        if (nodeInfos[node_idx].intentCode == NODE_VQUAD) {
            node_vquad_cnt ++;
            node_vquad_total_length += routingGraph.routeNodes[node_idx].getLength();
            node_vquad_total_cost += routingGraph.routeNodes[node_idx].getBaseCost();
        } 

        if (nodeInfos[node_idx].intentCode == NODE_HQUAD) {
            node_hquad_cnt ++;
            node_hquad_total_length += routingGraph.routeNodes[node_idx].getLength();
            node_hquad_total_cost += routingGraph.routeNodes[node_idx].getBaseCost();
        } 

        if (nodeInfos[node_idx].intentCode == NODE_VLONG) {
            node_vlong_cnt ++;
            node_vlong_total_length += routingGraph.routeNodes[node_idx].getLength();
            node_vlong_total_cost += routingGraph.routeNodes[node_idx].getBaseCost();
        } 

        if (nodeInfos[node_idx].intentCode == NODE_HLONG) {
            node_hlong_cnt ++;
            node_hlong_total_length += routingGraph.routeNodes[node_idx].getLength();
            node_hlong_total_cost += routingGraph.routeNodes[node_idx].getBaseCost();
        }   
    }
    // std::cout << "node_vsingle -- cnt: " << node_vsingle_cnt << " total_length: " << node_vsingle_total_length << " total_cost: " << node_vsingle_total_cost << " cost_per_length: " << node_vsingle_total_cost / node_vsingle_total_length << endl;
    // std::cout << "node_hsingle -- cnt: " << node_hsingle_cnt << " total_length: " << node_hsingle_total_length << " total_cost: " << node_hsingle_total_cost << " cost_per_length: " << node_hsingle_total_cost / node_hsingle_total_length << endl;
    // std::cout << "node_vdouble -- cnt: " << node_vdouble_cnt << " total_length: " << node_vdouble_total_length << " total_cost: " << node_vdouble_total_cost << " cost_per_length: " << node_vdouble_total_cost / node_vdouble_total_length << endl;
    // std::cout << "node_hdouble -- cnt: " << node_hdouble_cnt << " total_length: " << node_hdouble_total_length << " total_cost: " << node_hdouble_total_cost << " cost_per_length: " << node_hdouble_total_cost / node_hdouble_total_length << endl;
    // std::cout << "node_vlong -- cnt: " << node_vlong_cnt << " total_length: " << node_vlong_total_length << " total_cost: " << node_vlong_total_cost << " cost_per_length: " << node_vlong_total_cost / node_vlong_total_length << endl;
    // std::cout << "node_hlong -- cnt: " << node_hlong_cnt << " total_length: " << node_hlong_total_length << " total_cost: " << node_hlong_total_cost << " cost_per_length: " << node_hlong_total_cost / node_hlong_total_length << endl;
    // std::cout << "node_vquad -- cnt: " << node_vquad_cnt << " total_length: " << node_vquad_total_length << " total_cost: " << node_vquad_total_cost << " cost_per_length: " << node_vquad_total_cost / node_vquad_total_length << endl;
    // std::cout << "node_hquad -- cnt: " << node_hquad_cnt << " total_length: " << node_hquad_total_length << " total_cost: " << node_hquad_total_cost << " cost_per_length: " << node_hquad_total_cost / node_hquad_total_length << endl;
    log() << "Finish reading." << endl;
}

obj_idx Device::get_node_idx(obj_idx tile_idx, obj_idx wire_idx) 
{
    if (tile_idx >= tile_wire_to_node.size()) return invalid_obj_idx;
    if (wire_idx >= tile_wire_to_node[tile_idx].size()) return invalid_obj_idx;
    return tile_wire_to_node[tile_idx][wire_idx];
}

obj_idx Device::get_node_idx(string& tile_name, string& wire_name) 
{
    auto it = tile_name_to_idx.find(tile_name);
    if (it == tile_name_to_idx.end()) return invalid_obj_idx;
    obj_idx tile_idx = it->second;

    it = tile_name_to_type.find(tile_name);
    if (it == tile_name_to_type.end()) return invalid_obj_idx;
    obj_idx tile_type_idx = it->second;

    it = wire_name_to_idx_in_tile_type[tile_type_idx].find(wire_name);
    if (it == wire_name_to_idx_in_tile_type[tile_type_idx].end()) return invalid_obj_idx;
    obj_idx wire_idx = it->second;

    return get_node_idx(tile_idx, wire_idx);
}

const TileTypePIP& Device::getTileTypePIP(obj_idx node0, obj_idx node1)
{
    // Wire& node1_begin_wire  = node_to_wires[node1][0];
    // int tile_type_idx       = node1_begin_wire.tile_type_idx;
    // int tile_idx            = node1_begin_wire.tile_idx;
    // int wire1_it_idx        = node1_begin_wire.wire_in_tile_idx;
    const auto& n0_children = get_outgoing_nodes(node0);
    bool found_child = false;
    for (obj_idx child: n0_children) {
        if (child == node1) {
            found_child = true;
            break;
        }
    }

    const auto& n1_parents = get_incoming_nodes(node1);
    bool found_parent = false;
    for (obj_idx parent: n1_parents) {
        if (parent == node0) {
            found_parent = true;
            break;
        }
    }

    // obj_idx tile_type_idx = routingGraph.routeNodes[node1].getTileType();
    // obj_idx tile_idx = routingGraph.routeNodes[node1].getBeginTileId();
    // obj_idx wire1_it_idx = routingGraph.routeNodes[node1].getWireId();
    obj_idx tile_type_idx;
    obj_idx tile_idx;
    obj_idx wire0_it_idx;
    obj_idx wire1_it_idx;
    bool found = false;

    for (Wire& node1_wire: node_to_wires[node1]) {
        for (Wire& node0_wire: node_to_wires[node0]) {
            if (node0_wire.tile_idx == node1_wire.tile_idx) {
                // some nodes will have several segments in a tile
                auto id = utils::ints2long(node0_wire.wire_in_tile_idx, node1_wire.wire_in_tile_idx);
                if (tile_type_node_pair_to_pip_idx[node1_wire.tile_type_idx].find(id) != tile_type_node_pair_to_pip_idx[node1_wire.tile_type_idx].end()) {
                    found = true;
                    wire0_it_idx = node0_wire.wire_in_tile_idx;
                    wire1_it_idx = node1_wire.wire_in_tile_idx;
                    tile_type_idx = node1_wire.tile_type_idx;
                    tile_idx = node1_wire.tile_idx;
                    break;
                }
            }
        }
    }

    
    if (!found) {
        log(LOG_ERROR) << "Cannot found tile for " << node0 << endl;
    }

    unsigned long id = utils::ints2long(wire0_it_idx, wire1_it_idx);
	assert_t(tile_type_node_pair_to_pip_idx[tile_type_idx].find(id) != tile_type_node_pair_to_pip_idx[tile_type_idx].end());
	return tile_type_pip_list[tile_type_idx][tile_type_node_pair_to_pip_idx[tile_type_idx][id]];
}

void Device::dump() {
    
}

Device::~Device() {
    log() << "Device destruct" << endl;
}

void Device::clearData(int i) {
	if (i == 0) {
    	node_in_allowed_tile.clear();
		nodes_in_graph.clear();
		nodeInfos.clear();
    	string_list.clear();
    	string_to_idx.clear();
	} else if (i == 1) {
    	tile_to_name_idx.clear();
    	tile_to_type.clear();
    	tile_type_wire_to_name_idx.clear();
    	node_to_wires.clear();
	} else if (i == 2) {
    	tile_type_outgoing_wires.clear();         
    	tile_type_pip_list.clear();
	} else if (i == 3) {
    	tile_type_incoming_wires.clear();          
    	tile_type_node_pair_to_pip_idx.clear();
	} else if (i == 4) {
    	tile_wire_to_node.clear(); // tile_idx, wire_idx -> node_idx
    	site_type_pin_name_to_idx.clear(); // site_idx -> pin_name : pin_idx
    	site_name_to_idx.clear(); //
    	sites.clear();
	} else if (i == 5) {
    	tile_type_site_pin_to_wire_idx.clear(); // tile_type_idx -> site_idx -> pin_idx -> tile_wire_idx
    	tile_type_wire_str_to_idx.clear(); // tile_type_idx -> wire_str_idx -> wire_idx(in tile)
    	tile_str_to_idx.clear();
    	tile_name_to_idx.clear();
	} else if (i == 6) {
    	tile_name_to_type.clear();
    	tile_x_coor.clear();
    	tile_y_coor.clear();
    	wire_name_to_idx_in_tile_type.clear();
	}
}

};
