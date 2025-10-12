#pragma once
#include "global.h"
#include "capnp/serialize.h"
#include "capnp/serialize-packed.h"
#include "kj/std/iostream.h"
#include "DeviceResources.capnp.h"
#include "db/routeNodeGraph.h"

namespace Raw {

class Node {
public:
    obj_idx index;
    uint16_t x;
    uint16_t y;
    cost_type cost;

    obj_idx reserved_for_net_idx = invalid_obj_idx;
    int used_times = 0;
    float history = 0;

    Node(int idx): index(idx), cost(1) {}
    void reserve_for(obj_idx net_idx) { reserved_for_net_idx = net_idx; }
    void use() { used_times += 1; }
    void unuse() { used_times -= 1; }
};

class Wire {
public:
    Wire() {}
	Wire(obj_idx tile_idx_, obj_idx tile_type_idx_, obj_idx wire_in_tile_idx_) : 
		tile_idx(tile_idx_), tile_type_idx(tile_type_idx_), wire_in_tile_idx(wire_in_tile_idx_) {};
	obj_idx tile_idx;
    obj_idx tile_type_idx;
    obj_idx wire_in_tile_idx;

    friend class boost::serialization::access;
	template<class Archive>
    void serialize(Archive & ar, const unsigned int version)
    {
        ar & tile_idx;
        ar & tile_type_idx;
        ar & wire_in_tile_idx;
    }
};

class TileTypePIP {
public:
    TileTypePIP() {}
	TileTypePIP(obj_idx wire0_it_idx_, obj_idx wire1_it_idx_, bool forward_) : 
		wire0_it_idx(wire0_it_idx_), wire1_it_idx(wire1_it_idx_), forward(forward_) {};

    obj_idx wire0_it_idx;
    obj_idx wire1_it_idx;
	bool forward;

    friend class boost::serialization::access;
	template<class Archive>
    void serialize(Archive & ar, const unsigned int version)
    {
        ar & wire0_it_idx;
        ar & wire1_it_idx;
        ar & forward;
    }
};

class Edge {
public:
    obj_idx start_node_idx;
    obj_idx end_node_idx;
    obj_idx tile_idx;
    obj_idx tile_pip_idx;

    Edge(obj_idx start_node, obj_idx end_node, obj_idx tile, obj_idx pip):
        start_node_idx(start_node), end_node_idx(end_node), tile_idx(tile), tile_pip_idx(pip) {}
};

class Site {
public:
    obj_idx tile_idx;
    obj_idx tile_type_idx;
    obj_idx in_tile_site_idx;
    obj_idx site_type_idx;
    unordered_map<string, obj_idx> pin_name_to_idx;

    Site() {}
    Site(obj_idx tile, obj_idx tile_type, obj_idx tile_site_idx, obj_idx type_idx):
        tile_idx(tile), tile_type_idx(tile_type), in_tile_site_idx(tile_site_idx), site_type_idx(type_idx) {}

    friend class boost::serialization::access;
	template<class Archive>
    void serialize(Archive & ar, const unsigned int version)
    {
        ar & tile_idx;
        ar & tile_type_idx;
        ar & in_tile_site_idx;
        ar & site_type_idx;
        ar & pin_name_to_idx;
    }
};

class NodeInfo { // not used for routing
public:
	// used in netlist
	int intentCode;
	int tileType;
	int beginTileId;
	int endTileId;
	bool laguna;

    friend class boost::serialization::access;
	template<class Archive>
    void serialize(Archive & ar, const unsigned int version)
    {
        ar & intentCode;
        ar & tileType;
        ar & beginTileId;
        ar & endTileId;
        ar & laguna;
    }
};

class Device {
public:
    // Device(string device_file);
    Device(RouteNodeGraph& routingGraph_) : routingGraph(routingGraph_){};
    ~Device();
    void read(string device_file);
    obj_idx get_site_pin_node(string site_name, string pin_name) const;
    vector<obj_idx> get_outgoing_nodes(obj_idx node_idx);
    vector<obj_idx> get_incoming_nodes(obj_idx node_idx);
    obj_idx get_node_idx(obj_idx tile_idx, obj_idx wire_idx);
    obj_idx get_node_idx(string& tile_name, string& wire_name);
	const TileTypePIP& getTileTypePIP(obj_idx node0, obj_idx node1);
    void add_tile_type_pip(obj_idx tile_type_idx, obj_idx tile_type_wire_0_idx, obj_idx tile_type_wire_1_idx, bool directional);

    void dump();
    void load();

	int nodeNum;
    int edgeNum;
    vector<bool> node_in_allowed_tile;
	vector<int> nodes_in_graph;
	vector<NodeInfo> nodeInfos;
    vector<string> string_list;
    unordered_map<string, int> string_to_idx;
    vector<str_idx> tile_to_name_idx;
    vector<obj_idx> tile_to_type;
    vector<vector<str_idx>> tile_type_wire_to_name_idx;
    std::pair<str_idx, string> get_tile_name(obj_idx tile_idx) {
		str_idx idx = tile_to_name_idx[tile_idx];
		return {idx, string_list[idx]};
	}
    std::pair<str_idx, string> get_wire_name(obj_idx tile_idx, obj_idx wire_idx) {
		str_idx idx = tile_type_wire_to_name_idx[tile_to_type[tile_idx]][wire_idx];
		return {idx, string_list[idx]};
	}
    // string get_tile_wire_name(obj_idx tile_idx, obj_idx wire_idx) {return (get_tile_name(tile_idx) + "/" + get_wire_name(tile_idx, wire_idx));}

    vector<vector<Wire>> node_to_wires;
    vector<vector<vector<obj_idx>>> tile_type_outgoing_wires;           // tile_type_idx -> tile_type_w0_idx -> list(tile_type_w1_idx)
    vector<vector<vector<obj_idx>>> tile_type_incoming_wires;           // tile_type_idx -> tile_type_w1_idx -> list(tile_type_w0_idx)
    vector<vector<TileTypePIP>> tile_type_pip_list;
    vector<unordered_map<unsigned long, obj_idx>> tile_type_node_pair_to_pip_idx;
    // vector<string> node_names;
	void clearData(int i);

    // get shape of the device
    int getDeviceXMax() {return x_max;}
    int getDeviceYMax() {return y_max;}

private:
    int x_max = 0;
    int y_max = 0;
    // for indexing
	RouteNodeGraph& routingGraph;
    vector<vector<obj_idx>> tile_wire_to_node; // tile_idx, wire_idx -> node_idx
    vector<unordered_map<string, obj_idx>> site_type_pin_name_to_idx; // site_idx -> pin_name : pin_idx
    unordered_map<string, obj_idx> site_name_to_idx; //
    vector<Site> sites;
    vector<vector<vector<obj_idx>>> tile_type_site_pin_to_wire_idx; // tile_type_idx -> site_idx -> pin_idx -> tile_wire_idx
    vector<unordered_map<str_idx, obj_idx>> tile_type_wire_str_to_idx; // tile_type_idx -> wire_str_idx -> wire_idx(in tile)
    unordered_map<str_idx, obj_idx> tile_str_to_idx;
    unordered_map<string, obj_idx> tile_name_to_idx;
    unordered_map<string, obj_idx> tile_name_to_type;
    vector<int> tile_x_coor;
    vector<int> tile_y_coor;
    vector<unordered_map<string, obj_idx>> wire_name_to_idx_in_tile_type;

    vector<size_t> memory_peak_records;

    friend class boost::serialization::access;
    template<class Archive>
    void serialize(Archive & ar, const unsigned int version)
    {
        ar & nodeNum;
        ar & edgeNum;
        ar & node_in_allowed_tile;
	    ar & nodes_in_graph;
	    ar & nodeInfos;
        ar & string_list;
        ar & string_to_idx;
        ar & tile_to_name_idx;
        ar & tile_to_type;
        ar & tile_type_wire_to_name_idx;
        // ar & node_to_wires; // most time-consuming
        ar & tile_type_outgoing_wires; 
        ar & tile_type_incoming_wires; 
        ar & tile_type_pip_list;
        ar & tile_type_node_pair_to_pip_idx;

        ar & x_max;
        ar & y_max;

        ar & tile_wire_to_node; 
        ar & site_type_pin_name_to_idx; 
        ar & site_name_to_idx; 
        ar & sites;
        ar & tile_type_site_pin_to_wire_idx; 
        ar & tile_type_wire_str_to_idx; 
        ar & tile_str_to_idx;
        ar & tile_name_to_idx;
        ar & tile_name_to_type;
        ar & tile_x_coor;
        ar & tile_y_coor;
        ar & wire_name_to_idx_in_tile_type;
    }

    void add_edge(obj_idx start_node, obj_idx end_node, obj_idx tile_col, obj_idx tile_row, obj_idx pip_idx);
    void add_pip(obj_idx start_node, obj_idx end_node, obj_idx tile_idx, obj_idx wire0_idx, obj_idx wire1_idx, bool directional);
    
    int getTileX(std::string& tile_str) {
        auto loc_x = tile_str.rfind('X');
        auto loc_y = tile_str.rfind('Y');
        return std::stoi(tile_str.substr(loc_x + 1, loc_y - loc_x - 1));
    }

    int getTileY(std::string& tile_str) {
        auto loc_y = tile_str.rfind('Y');
        return std::stoi(tile_str.substr(loc_y + 1));
    }
    
public:
    size_t memory_peak()
    {
#ifdef __APPLE__
        // On macOS, we don't have /proc/self/status
        // Return a placeholder value or use macOS-specific method
        return 0;
#else
        FILE* file = fopen("/proc/self/status", "r");
        if (!file) {
            return 0;
        }
        int result = -1;
        char line[128];

        while (fgets(line, 128, file) != nullptr) {
            if (strncmp(line, "VmPeak:", 7) == 0) {
                int len = strlen(line);

                const char* p = line;
                for (; std::isdigit(*p) == false; ++p) {}

                line[len - 3] = 0;
                result = atoi(p);

                break;
            }
        }

        fclose(file);

        return result;
#endif
    }

    void check_memory_peak(int label) {
        std::stringstream ss;
        auto check_id = memory_peak_records.size();
        ss << "Check " << label << ":";
        auto this_memory_peak = memory_peak();
        ss << " memory_peak: " << this_memory_peak << " kB";
        if (check_id > 0) {
            auto last_memory_peak = memory_peak_records[memory_peak_records.size() - 1];
            ss << ", last memory_peak: " << last_memory_peak << " kB, diff: " << (this_memory_peak - last_memory_peak) << " kB";
        }
        memory_peak_records.emplace_back(this_memory_peak);
        log() << ss.str() << endl;
    }
};

};
