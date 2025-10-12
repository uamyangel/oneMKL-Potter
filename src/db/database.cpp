#include "database.h"

void Database::readDevice(string deviceName) {
    size_t pos = deviceName.rfind('/');
    string deviceDir = (pos == deviceName.npos) ? "." : deviceName.substr(0, pos);
    string dumpDir = deviceDir + "/dump";
    string dumpDevice = deviceDir + "/dump/device";
    // string dumpNodeToWires = deviceDir + "/dump/node_to_wires";
    string dumpNodeToWires0 = deviceDir + "/dump/node_to_wires0";
    string dumpNodeToWires1 = deviceDir + "/dump/node_to_wires1";
    string dumpNodeToWires2 = deviceDir + "/dump/node_to_wires2";
    string dumpNodeToWires3 = deviceDir + "/dump/node_to_wires3";
    string dumpRouteNodes = deviceDir + "/dump/routeNodes";
    // std::cout << dumpDevice << " " << dumpRouteNodes << endl;

    auto isFileExists_stat = [](string& name) {
        struct stat buffer;   
        return (stat(name.c_str(), &buffer) == 0); 
    };

    if (isFileExists_stat(dumpDevice) && 
        isFileExists_stat(dumpRouteNodes) && 
        isFileExists_stat(dumpNodeToWires0) &&
        isFileExists_stat(dumpNodeToWires1) &&
        isFileExists_stat(dumpNodeToWires2) &&
        isFileExists_stat(dumpNodeToWires3)) {
        vector<vector<Raw::Wire>> node_to_wires0;
        vector<vector<Raw::Wire>> node_to_wires1;
        vector<vector<Raw::Wire>> node_to_wires2;
        vector<vector<Raw::Wire>> node_to_wires3;
        vector<RouteNode> routeNode0;
        vector<RouteNode> routeNode1;
        vector<RouteNode> routeNode2;
        vector<RouteNode> routeNode3;
        auto load_node_to_wires0 = [&] {
            // log() << "start load_node_to_wires0" << endl;
            std::ifstream ifs;
            ifs.open(dumpNodeToWires0);
            boost::archive::binary_iarchive ia_node_to_wires(ifs);
            ia_node_to_wires & node_to_wires0;
            ifs.close();
            // log() << "end load_node_to_wires0" << endl;
        };
        auto load_node_to_wires1 = [&] {
            // log() << "start load_node_to_wires1" << endl;
            std::ifstream ifs;
            ifs.open(dumpNodeToWires1);
            boost::archive::binary_iarchive ia_node_to_wires(ifs);
            ia_node_to_wires & node_to_wires1;
            ifs.close();
            // log() << "end load_node_to_wires1" << endl;
        };
        auto load_node_to_wires2 = [&] {
            // log() << "start load_node_to_wires2" << endl;
            std::ifstream ifs;
            ifs.open(dumpNodeToWires2);
            boost::archive::binary_iarchive ia_node_to_wires(ifs);
            ia_node_to_wires & node_to_wires2;
            ifs.close();
            // log() << "end load_node_to_wires2" << endl;
        };
        auto load_node_to_wires3 = [&] {
            // log() << "start load_node_to_wires3" << endl;
            std::ifstream ifs;
            ifs.open(dumpNodeToWires3);
            boost::archive::binary_iarchive ia_node_to_wires(ifs);
            ia_node_to_wires & node_to_wires3;
            ifs.close();
            // log() << "end load_node_to_wires3" << endl;
        };
        auto load_device = [&] {
            std::ifstream ifs;
            ifs.open(dumpDevice);
            boost::archive::binary_iarchive ia_device(ifs);
            ia_device & device;
            ifs.close();
        };
        auto load_routeNodes = [&] {
            // log() << "start load_routeNodes3" << endl;
            std::ifstream ifs;
            ifs.open(dumpRouteNodes);
            boost::archive::binary_iarchive ia_routeNodes(ifs);
            ia_routeNodes & routingGraph.routeNodes;
            ifs.close();
            // log() << "end load_routeNodes3" << endl;
        };
        log() << "Device cache is found. Start loading." << endl;
        std::thread thread_node_to_wires0(load_node_to_wires0);
        std::thread thread_node_to_wires1(load_node_to_wires1);
        std::thread thread_node_to_wires2(load_node_to_wires2);
        std::thread thread_node_to_wires3(load_node_to_wires3);
        std::thread thread_device(load_device);
        std::thread thread_routeNodes(load_routeNodes);
        auto concat_node_to_wires = [&] {
            thread_node_to_wires0.join();
            thread_node_to_wires1.join();
            thread_node_to_wires2.join();
            thread_node_to_wires3.join();
            // log() << "start concat_node_to_wires" << endl;
            device.node_to_wires.reserve((node_to_wires0.size() + node_to_wires1.size() + node_to_wires2.size() + node_to_wires3.size()));
            device.node_to_wires.insert(device.node_to_wires.end(), node_to_wires0.begin(), node_to_wires0.end());
            device.node_to_wires.insert(device.node_to_wires.end(), node_to_wires1.begin(), node_to_wires1.end());
            device.node_to_wires.insert(device.node_to_wires.end(), node_to_wires2.begin(), node_to_wires2.end());
            device.node_to_wires.insert(device.node_to_wires.end(), node_to_wires3.begin(), node_to_wires3.end());
            // log() << "end concat_node_to_wires" << endl;
        };
        std::thread thread_concat_node_to_wires(concat_node_to_wires);
        // std::thread thread_concat_routeNodes(concat_routeNodes);
        thread_device.join();
        thread_concat_node_to_wires.join();
        thread_routeNodes.join();
        assert_t(device.node_to_wires.size() == device.nodeNum);
        assert_t(routingGraph.routeNodes.size() == device.nodeNum);
        log() << "Finish loading." << endl;
    } else {
        device.read(deviceName);
        {
            if (!isFileExists_stat(dumpDir)) mkdir(dumpDir.c_str(), S_IRWXU | S_IRUSR | S_IWUSR | S_IXUSR | S_IRWXG | S_IRWXO);
            auto dump_node_to_wires0 = [&]() {
                vector<vector<Raw::Wire>> node_to_wires0;
                // node_to_wires0.reserve(device.node_to_wires.size() / 4);
                auto it_begin = device.node_to_wires.begin();
                auto it_end = device.node_to_wires.begin() + device.node_to_wires.size() / 4;
                node_to_wires0.insert(node_to_wires0.end(), it_begin, it_end);
                std::ofstream ofs;
                ofs.open(dumpNodeToWires0);
                boost::archive::binary_oarchive oa_node_to_wires(ofs);
                oa_node_to_wires & node_to_wires0;
                ofs.close();
            };
            auto dump_node_to_wires1 = [&]() {
                vector<vector<Raw::Wire>> node_to_wires1;
                // node_to_wires1.reserve(device.node_to_wires.size() / 4);
                auto it_begin = device.node_to_wires.begin() + device.node_to_wires.size() / 4;
                auto it_end = device.node_to_wires.begin() + 2 * device.node_to_wires.size() / 4;
                node_to_wires1.insert(node_to_wires1.end(), it_begin, it_end);
                std::ofstream ofs;
                ofs.open(dumpNodeToWires1);
                boost::archive::binary_oarchive oa_node_to_wires(ofs);
                oa_node_to_wires & node_to_wires1;
                ofs.close();
            };
            auto dump_node_to_wires2 = [&]() {
                vector<vector<Raw::Wire>> node_to_wires2;
                // node_to_wires2.reserve(device.node_to_wires.size() / 4);
                auto it_begin = device.node_to_wires.begin() + 2 * device.node_to_wires.size() / 4;
                auto it_end = device.node_to_wires.begin() + 3 * device.node_to_wires.size() / 4;
                node_to_wires2.insert(node_to_wires2.end(), it_begin, it_end);
                std::ofstream ofs;
                ofs.open(dumpNodeToWires2);
                boost::archive::binary_oarchive oa_node_to_wires(ofs);
                oa_node_to_wires & node_to_wires2;
                ofs.close();
            };
            auto dump_node_to_wires3 = [&]() {
                vector<vector<Raw::Wire>> node_to_wires3;
                // node_to_wires3.reserve(device.node_to_wires.size() - 3 * device.node_to_wires.size() / 4);
                auto it_begin = device.node_to_wires.begin() + 3 * device.node_to_wires.size() / 4;
                auto it_end = device.node_to_wires.end();
                node_to_wires3.insert(node_to_wires3.end(), it_begin, it_end);
                std::ofstream ofs;
                ofs.open(dumpNodeToWires3);
                boost::archive::binary_oarchive oa_node_to_wires(ofs);
                oa_node_to_wires & node_to_wires3;
                ofs.close();
            };
            auto dump_device = [&]() {
                std::ofstream ofs;
                ofs.open(dumpDevice);
                boost::archive::binary_oarchive oa_device(ofs);
                oa_device & device;
                ofs.close();
            };
            auto dump_routenodes = [&]() {
                std::ofstream ofs;
                ofs.open(dumpRouteNodes);
                boost::archive::binary_oarchive oa_routeNodes(ofs);
                oa_routeNodes & routingGraph.routeNodes;
                ofs.close();
            };

            log() << "Dump device cache to the disk." << endl;
            std::thread thread_node_to_wires0(dump_node_to_wires0);
            std::thread thread_node_to_wires1(dump_node_to_wires1);
            std::thread thread_node_to_wires2(dump_node_to_wires2);
            std::thread thread_node_to_wires3(dump_node_to_wires3);
            std::thread thread_device(dump_device);
            std::thread thread_routeNodes(dump_routenodes);
            thread_node_to_wires0.join();
            thread_node_to_wires1.join();
            thread_node_to_wires2.join();
            thread_node_to_wires3.join();
            thread_device.join();
            thread_routeNodes.join();
            log() << "Finish dumping." << endl;
        }
    }
    numNodes = device.nodeNum; // TODO: unify the name
    numEdges = device.edgeNum;
}

void Database::readNetlist(string netlistName) {
    inputName = netlistName;
    netlist.read(netlistName); // TODO: unify the name and consider the indirect and direct num 
    numConns = netlist.connNum;
    numNets  = netlist.netNum;
}

void Database::reduceRouteNode() {
    // mark pin nodes
    vector<int> pinNodes(numNodes, 0);
    auto getPinNodes = [this, &pinNodes](int tid) {
        for (int i = tid; i < indirectConnections.size(); i += numThread) {
            auto& conn = indirectConnections[i];
            pinNodes[conn.getSourceRNode()->getId()] = 1;
            pinNodes[conn.getSinkRNode()->getId()] = 1;
        }
    };
    vector<std::thread> jobs;
    for (int tid = 0; tid < numThread; tid ++) jobs.emplace_back(getPinNodes, tid);
    for (int tid = 0; tid < numThread; tid ++) jobs[tid].join();
    jobs.clear();

    // out degree for each node
    vector<int> outDegrees(numNodes, 0);
    auto getOutDegrees = [this, &outDegrees] (int tid) {
        for (obj_idx node_0_idx = tid; node_0_idx < device.nodeNum; node_0_idx += numThread) {
            if (device.nodes_in_graph[node_0_idx]) {
                for (obj_idx node_1_idx : device.get_outgoing_nodes(node_0_idx)) {
                    if (device.node_in_allowed_tile[node_1_idx] && !preservedNodes[node_1_idx] && device.nodes_in_graph[node_1_idx]) {
                        outDegrees[node_0_idx] ++;
                    }
                }
            }
        }
    };
    for (int tid = 0; tid < numThread; tid ++) jobs.emplace_back(getOutDegrees, tid);
    for (int tid = 0; tid < numThread; tid ++) jobs[tid].join();
    jobs.clear();

    // parent nodes for recursive deletion
    vector<vector<int>> parents(numNodes);
    auto set_parents = [this, &parents] (int tid) {
        for (obj_idx node_0_idx = tid; node_0_idx < device.nodeNum; node_0_idx += numThread) {
            if (device.nodes_in_graph[node_0_idx] && device.node_in_allowed_tile[node_0_idx] && !preservedNodes[node_0_idx]) {
                for (obj_idx node_1_idx : device.get_incoming_nodes(node_0_idx)) {
                    if (device.nodes_in_graph[node_1_idx]) {
                        parents[node_0_idx].emplace_back(node_1_idx);
                    }
                }
            }
        }
    };
    for (int tid = 0; tid < numThread; tid ++) jobs.emplace_back(set_parents, tid);
    for (int tid = 0; tid < numThread; tid ++) jobs[tid].join();
    jobs.clear();

    vector<int> deadEndNodeIds;
    int deadEndNodeForPins = 0;
    for (int i = 0; i < numNodes; i ++) {
        if (device.nodes_in_graph[i]) {
            if (outDegrees[i] == 0) {
                if (pinNodes[i])
                    deadEndNodeForPins ++;
                else
                    deadEndNodeIds.emplace_back(i);
            }
        }	
    }
    log() << "DeadEndNodeForPins: " << deadEndNodeForPins << endl;
    while (deadEndNodeIds.size() > 0) {
        log() << "#DeadEndNode: " << deadEndNodeIds.size() << std::endl;
        vector<int> newDeadEndNodeIds;
        for (auto id : deadEndNodeIds) {
            device.nodes_in_graph[id] = false;
            for (auto pid : parents[id]) {
                outDegrees[pid] --;
                if (outDegrees[pid] == 0 && !pinNodes[pid])
                    newDeadEndNodeIds.emplace_back(pid);
            }
        }
        deadEndNodeIds = newDeadEndNodeIds;
    }
}

void Database::setRouteNodeChildren() {
    reduceRouteNode();
    // Set RouteNode Children in after load nets
    // 1. add source and sink nodes of connections in to the graph
    auto set_source_and_sink_in_graph = [this](int tid) {
        for (int i = tid; i < indirectConnections.size(); i += numThread) {
            auto& conn = indirectConnections[i];
            device.nodes_in_graph[conn.getSourceRNode()->getId()] = true;
            device.nodes_in_graph[conn.getSinkRNode()->getId()] = true;
        }
    };

    vector<std::thread> jobs_set_in_graph;
    for (int tid = 0; tid < numThread; tid ++) {
        jobs_set_in_graph.emplace_back(set_source_and_sink_in_graph, tid);
    }

    for (int tid = 0; tid < numThread; tid ++) {
        jobs_set_in_graph[tid].join();
    }

    // TODO: also load children and then modify the children for some nodes
    auto set_childrens = [this] (int tid) {
        for (obj_idx node_0_idx = tid; node_0_idx < device.nodeNum; node_0_idx += numThread) {
            if (device.nodes_in_graph[node_0_idx]) {
                for (obj_idx node_1_idx : device.get_outgoing_nodes(node_0_idx)) {
                    if (device.node_in_allowed_tile[node_1_idx] && !preservedNodes[node_1_idx] && device.nodes_in_graph[node_1_idx]) {
                        routingGraph.routeNodes[node_0_idx].addChildren(&routingGraph.routeNodes[node_1_idx]);
                        // numEdgesInRRG ++;
                    }
                }
            }
        }
    };

    vector<std::thread> jobs_set_children;
    for (int tid = 0; tid < numThread; tid ++) {
        jobs_set_children.emplace_back(set_childrens, tid);
    }

    for (int tid = 0; tid < numThread; tid ++) {
        jobs_set_children[tid].join();
    }
}

void Database::printStatistic() {
    for (int i = 0; i < numNodes; i ++) {
        if (preservedNodes[i])
            preservedNum ++;
        if (device.nodes_in_graph[i]) {
            numNodesInRRG ++;
            numEdgesInRRG += routingGraph.routeNodes[i].getChildrenSize();
        }	
    }
    log() << "#Node: " << numNodes << " #Node (RRG) " << numNodesInRRG << " #Edges: "  << numNodes << " #Edges (RRG): " << numEdgesInRRG << std::endl;
    log() << "#Conns: " << numConns << " #Nets: " << numConns << " #Indirect: " <<  indirectConnections.size() << " #direct: " << directConnections.size() << std::endl;	
    log() << "#preservedNum: " << preservedNum << endl;
}

void Database::checkRoute() {
    // check no overflow
    log() << "Checking on routing overflow" << endl;
    vector<int> nodeUsage(numNodes, -1);
    for (int i = 0; i < numConns; i ++) {
        auto& conn = indirectConnections[i];
        if (conn.getRNodeSize() == 0) {
            log(LOG_ERROR) << "Connection " << i << " path length is 0" << endl;
            exit(0);
        }
        for (auto rnode : conn.getRNodes()) {
            int nodeId = rnode->getId();
            if (nodeUsage[nodeId] != -1 && nodeUsage[nodeId] != conn.getNetId()) {
                log(LOG_ERROR) << "Overflow in node " << nodeId << " " << nodeUsage[nodeId] << " " << conn.getNetId() << endl;
                exit(0);
            }
            nodeUsage[nodeId] = conn.getNetId();
        }
    }
}