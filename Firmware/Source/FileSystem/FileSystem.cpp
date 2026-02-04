#include "FileSystem.h"

// File method implementations
std::vector<uint8_t> FileSystem::File::readAll() {
    int32_t sz = size();
    if (sz <= 0) return {};
    if (seek(0) < 0) return {};
    
    std::vector<uint8_t> buf(sz);
    int32_t rd = read(buf.data(), sz);
    if (rd < 0) return {};
    buf.resize(rd);
    return buf;
}

bool FileSystem::File::writeAll(const std::vector<uint8_t>& data) {
    return write(data.data(), data.size()) == static_cast<int32_t>(data.size());
}

std::string FileSystem::File::readString() {
    auto buf = readAll();
    return std::string(buf.begin(), buf.end());
}

bool FileSystem::File::writeString(const std::string& str) {
    return write(str.data(), str.size()) == static_cast<int32_t>(str.size());
}

bool FileSystem::File::appendLine(const std::string& line) {
    if (!isOpen()) return false;
    if (seek(0, LFS_SEEK_END) < 0) return false;
    if (write(line.data(), line.size()) != static_cast<int32_t>(line.size())) return false;
    const char nl = '\n';
    return write(&nl, 1) == 1;
}