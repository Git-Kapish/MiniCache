cmake_minimum_required(VERSION 3.16)
project(minicache VERSION 0.4.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/FS)
endif()

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

# Core sources
set(MINICACHE_SOURCES
    src/store/shard.cpp
    src/store/shard_router.cpp
    src/store/consistent_hash_ring.cpp
    src/store/active_sweeper.cpp
    src/eviction/lru_policy.cpp
    src/eviction/lfu_policy.cpp
    src/persistence/aof_writer.cpp
    src/persistence/snapshotter.cpp
    src/persistence/recovery.cpp
    src/replication/replication_streamer.cpp
    src/replication/replica_client.cpp
    src/protocol/resp_parser.cpp
    src/protocol/resp_encoder.cpp
    src/command/commands.cpp
    src/command/hash_commands.cpp
    src/command/list_commands.cpp
    src/command/set_commands.cpp
    src/command/command_factory.cpp
    src/command/dispatcher.cpp
    src/net/socket_shim.cpp
    src/net/tcp_server.cpp
)

# Server Executable
add_executable(minicache_server src/main.cpp ${MINICACHE_SOURCES})

if(WIN32)
    target_link_libraries(minicache_server PRIVATE ws2_32)
endif()

# Enable Testing
enable_testing()

# Unit Test: RESP Protocol
add_executable(test_resp_protocol tests/test_resp_protocol.cpp ${MINICACHE_SOURCES})
if(WIN32)
    target_link_libraries(test_resp_protocol PRIVATE ws2_32)
endif()
add_test(NAME test_resp_protocol COMMAND test_resp_protocol)

# Unit Test: Shard & Store
add_executable(test_shard tests/test_shard.cpp ${MINICACHE_SOURCES})
if(WIN32)
    target_link_libraries(test_shard PRIVATE ws2_32)
endif()
add_test(NAME test_shard COMMAND test_shard)

# Unit Test: Eviction Policies (LRU / LFU)
add_executable(test_eviction tests/test_eviction.cpp ${MINICACHE_SOURCES})
if(WIN32)
    target_link_libraries(test_eviction PRIVATE ws2_32)
endif()
add_test(NAME test_eviction COMMAND test_eviction)

# Unit Test: Concurrency Stress Test
add_executable(test_concurrency_stress tests/test_concurrency_stress.cpp ${MINICACHE_SOURCES})
if(WIN32)
    target_link_libraries(test_concurrency_stress PRIVATE ws2_32)
endif()
add_test(NAME test_concurrency_stress COMMAND test_concurrency_stress)

# Unit Test: Persistence & Crash Recovery
add_executable(test_persistence tests/test_persistence.cpp ${MINICACHE_SOURCES})
if(WIN32)
    target_link_libraries(test_persistence PRIVATE ws2_32)
endif()
add_test(NAME test_persistence COMMAND test_persistence)

# Unit Test: Consistent Hashing Ring
add_executable(test_consistent_hash tests/test_consistent_hash.cpp ${MINICACHE_SOURCES})
if(WIN32)
    target_link_libraries(test_consistent_hash PRIVATE ws2_32)
endif()
add_test(NAME test_consistent_hash COMMAND test_consistent_hash)

# System Test: Replication Streaming
add_executable(test_replication tests/test_replication.cpp ${MINICACHE_SOURCES})
if(WIN32)
    target_link_libraries(test_replication PRIVATE ws2_32)
endif()
add_test(NAME test_replication COMMAND test_replication)
