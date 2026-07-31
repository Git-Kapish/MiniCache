#include "persistence/aof_writer.hpp"
#include <chrono>

namespace minicache::persistence {

AofWriter::AofWriter(const std::string& filePath, FsyncPolicy policy)
    : filePath_(filePath), policy_(policy) {}

AofWriter::~AofWriter() {
    close();
}

bool AofWriter::open() {
    std::lock_guard<std::mutex> lock(mutex_);
    fileStream_.open(filePath_, std::ios::out | std::ios::app | std::ios::binary);
    if (!fileStream_.is_open()) {
        return false;
    }

    if (policy_ == FsyncPolicy::EverySec) {
        running_ = true;
        flusherThread_ = std::thread(&AofWriter::flusherLoop, this);
    }
    return true;
}

void AofWriter::close() {
    if (running_.exchange(false)) {
        if (flusherThread_.joinable()) {
            flusherThread_.join();
        }
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (fileStream_.is_open()) {
        fileStream_.flush();
        fileStream_.close();
    }
}

void AofWriter::append(const std::string& respCommandBytes) {
    if (respCommandBytes.empty()) return;

    std::lock_guard<std::mutex> lock(mutex_);
    if (!fileStream_.is_open()) return;

    fileStream_.write(respCommandBytes.data(), static_cast<std::streamsize>(respCommandBytes.size()));

    if (policy_ == FsyncPolicy::Always) {
        fileStream_.flush();
    }
}

void AofWriter::flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (fileStream_.is_open()) {
        fileStream_.flush();
    }
}

void AofWriter::reset() {
    close();
    std::lock_guard<std::mutex> lock(mutex_);
    fileStream_.open(filePath_, std::ios::out | std::ios::trunc | std::ios::binary);
    if (fileStream_.is_open()) {
        fileStream_.close();
    }
    open();
}

void AofWriter::flusherLoop() {
    while (running_) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        if (!running_) break;
        flush();
    }
}

} // namespace minicache::persistence
