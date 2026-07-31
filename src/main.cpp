#include "store/shard_router.hpp"
#include "store/active_sweeper.hpp"
#include "persistence/aof_writer.hpp"
#include "persistence/recovery.hpp"
#include "replication/replication_streamer.hpp"
#include "replication/replica_client.hpp"
#include "command/dispatcher.hpp"
#include "net/tcp_server.hpp"
#include <iostream>
#include <string>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>

namespace {
std::atomic<bool> g_stopServer{false};

void signalHandler(int /*signum*/) {
    g_stopServer = true;
}

size_t parseMemoryString(const std::string& str) {
    if (str.empty()) return 0;
    size_t multiplier = 1;
    std::string numStr = str;

    char lastChar = std::toupper(str.back());
    if (lastChar == 'B') {
        numStr.pop_back();
        if (!numStr.empty()) {
            lastChar = std::toupper(numStr.back());
        }
    }

    if (lastChar == 'K') {
        multiplier = 1024;
        numStr.pop_back();
    } else if (lastChar == 'M') {
        multiplier = 1024 * 1024;
        numStr.pop_back();
    } else if (lastChar == 'G') {
        multiplier = 1024 * 1024 * 1024;
        numStr.pop_back();
    }

    try {
        return std::stoull(numStr) * multiplier;
    } catch (...) {
        return 0;
    }
}
}

int main(int argc, char* argv[]) {
    uint16_t port = 6380;
    size_t numShards = 4;
    size_t maxMemory = parseMemoryString("256mb");
    std::string evictionPolicy = "lru";
    bool enableAof = true;
    std::string aofFile = "appendonly.aof";
    std::string snapshotFile = "dump.rdb";
    std::string fsyncMode = "everysec";
    minicache::store::ShardingMode shardingMode = minicache::store::ShardingMode::Modulo;

    bool isReplica = false;
    std::string masterHost = "";
    uint16_t masterPort = 0;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "--port" || arg == "-p") && i + 1 < argc) {
            port = static_cast<uint16_t>(std::stoul(argv[++i]));
        } else if (arg == "--shards" && i + 1 < argc) {
            numShards = std::stoull(argv[++i]);
        } else if (arg == "--maxmemory" && i + 1 < argc) {
            maxMemory = parseMemoryString(argv[++i]);
        } else if (arg == "--eviction" && i + 1 < argc) {
            evictionPolicy = argv[++i];
        } else if (arg == "--aof" && i + 1 < argc) {
            std::string val = argv[++i];
            enableAof = (val == "true" || val == "yes" || val == "1");
        } else if (arg == "--aof-file" && i + 1 < argc) {
            aofFile = argv[++i];
        } else if (arg == "--fsync" && i + 1 < argc) {
            fsyncMode = argv[++i];
        } else if (arg == "--sharding-mode" && i + 1 < argc) {
            std::string smode = argv[++i];
            if (smode == "consistent" || smode == "chash") {
                shardingMode = minicache::store::ShardingMode::ConsistentHash;
            }
        } else if (arg == "--replicaof" && i + 2 < argc) {
            isReplica = true;
            masterHost = argv[++i];
            masterPort = static_cast<uint16_t>(std::stoul(argv[++i]));
        }
    }

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    minicache::store::ShardRouter router(numShards, maxMemory, evictionPolicy, shardingMode);

    // Startup Crash Recovery Flow
    if (!isReplica) {
        std::cout << "Starting MiniCache recovery..." << std::endl;
        size_t replayed = minicache::persistence::Recovery::recover(router, snapshotFile, enableAof ? aofFile : "");
        std::cout << "Recovery completed: " << replayed << " commands replayed." << std::endl;
    }

    // Setup AOF Writer (Leader mode)
    minicache::persistence::FsyncPolicy policy = minicache::persistence::FsyncPolicy::EverySec;
    if (fsyncMode == "always") {
        policy = minicache::persistence::FsyncPolicy::Always;
    } else if (fsyncMode == "never") {
        policy = minicache::persistence::FsyncPolicy::Never;
    }

    std::unique_ptr<minicache::persistence::AofWriter> aofWriter;
    if (enableAof && !isReplica) {
        aofWriter = std::make_unique<minicache::persistence::AofWriter>(aofFile, policy);
        if (!aofWriter->open()) {
            std::cerr << "Warning: Could not open AOF file " << aofFile << std::endl;
        }
    }

    // Replication Streamer (Leader side)
    std::unique_ptr<minicache::replication::ReplicationStreamer> streamer;
    if (!isReplica) {
        streamer = std::make_unique<minicache::replication::ReplicationStreamer>();
    }

    minicache::store::ActiveSweeper sweeper(router, 100);
    minicache::command::CommandDispatcher dispatcher(router, aofWriter.get(), streamer.get(), isReplica);
    minicache::net::TcpServer server(port, dispatcher);

    // Replica Client (Follower side)
    std::unique_ptr<minicache::replication::ReplicaClient> replicaClient;
    if (isReplica) {
        replicaClient = std::make_unique<minicache::replication::ReplicaClient>(masterHost, masterPort, router);
        if (!replicaClient->start()) {
            std::cerr << "Warning: Could not connect to leader at " << masterHost << ":" << masterPort << std::endl;
        }
    }

    sweeper.start();
    if (!server.start()) {
        std::cerr << "Failed to start server on port " << port << std::endl;
        sweeper.stop();
        if (aofWriter) aofWriter->close();
        if (replicaClient) replicaClient->stop();
        return 1;
    }

    std::cout << "MiniCache Server running ("
              << "mode=" << (isReplica ? "FOLLOWER (Read-Only)" : "LEADER")
              << ", port=" << port
              << ", shards=" << numShards
              << ", sharding=" << (shardingMode == minicache::store::ShardingMode::ConsistentHash ? "consistent-hash" : "modulo")
              << "). Press Ctrl+C to exit." << std::endl;

    while (!g_stopServer) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "Shutting down MiniCache server..." << std::endl;
    server.stop();
    sweeper.stop();
    if (replicaClient) {
        replicaClient->stop();
    }
    if (aofWriter) {
        aofWriter->close();
    }
    std::cout << "MiniCache server stopped." << std::endl;

    return 0;
}
