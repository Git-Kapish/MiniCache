#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <atomic>
#include <thread>

namespace minicache::persistence {

enum class FsyncPolicy {
    Always,
    EverySec,
    Never
};

class AofWriter {
public:
    explicit AofWriter(const std::string& filePath, FsyncPolicy policy = FsyncPolicy::EverySec);
    ~AofWriter();

    AofWriter(const AofWriter&) = delete;
    AofWriter& operator=(const AofWriter&) = delete;

    bool open();
    void close();

    void append(const std::string& respCommandBytes);
    void flush();
    void reset();

private:
    void flusherLoop();

    std::string filePath_;
    FsyncPolicy policy_;
    std::ofstream fileStream_;
    std::mutex mutex_;
    std::atomic<bool> running_{false};
    std::thread flusherThread_;
};

} // namespace minicache::persistence
