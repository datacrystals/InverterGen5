// FileSystem.hpp
#pragma once
#include <string>
#include <vector>
#include <optional>
#include <cstring>
#include <pico-littlefs/littlefs-lib/lfs.h>

class FileSystem {
public:
    struct Config {
        bool formatIfCorrupt = false;
    };

    // RAII File Handle using pico_hal file descriptors (int)
    class File {
        int fd_ = -1;
        
    public:
        File() = default;
        explicit File(int fd) : fd_(fd) {}
        ~File() { close(); }
        
        File(const File&) = delete;
        File& operator=(const File&) = delete;
        File(File&& other) noexcept : fd_(other.fd_) { other.fd_ = -1; }
        File& operator=(File&& other) noexcept {
            if (this != &other) {
                close();
                fd_ = other.fd_;
                other.fd_ = -1;
            }
            return *this;
        }
        
        bool isOpen() const { return fd_ >= 0; }
        int fd() const { return fd_; }
        
        void close() {
            if (fd_ >= 0) {
                pico_close(fd_);
                fd_ = -1;
            }
        }
        
        int32_t read(void* buffer, uint32_t size) {
            if (!isOpen()) return -1;
            return pico_read(fd_, buffer, size);
        }
        
        int32_t write(const void* buffer, uint32_t size) {
            if (!isOpen()) return -1;
            return pico_write(fd_, buffer, size);
        }
        
        int32_t seek(int32_t offset, int whence = LFS_SEEK_SET) {
            if (!isOpen()) return -1;
            return pico_seek(fd_, offset, whence);
        }
        
        // Remove const - pico_hal needs non-const
        int32_t tell() {
            if (!isOpen()) return -1;
            return pico_seek(fd_, 0, LFS_SEEK_CUR);
        }
        
        int32_t size() {
            if (!isOpen()) return -1;
            int32_t pos = tell();
            if (pos < 0) return -1;
            int32_t sz = pico_seek(fd_, 0, LFS_SEEK_END);
            pico_seek(fd_, pos, LFS_SEEK_SET);
            return sz;
        }
        
        bool truncate(uint32_t size) {
            if (!isOpen()) return false;
            // pico_hal doesn't expose truncate, reopen with truncate flag
            return false;  // Not implemented in pico_hal
        }
        
        bool flush() {
            if (!isOpen()) return false;
            // pico_hal syncs on close, no explicit flush
            return true;
        }
        
        std::vector<uint8_t> readAll();
        bool writeAll(const std::vector<uint8_t>& data);
        std::string readString();
        bool writeString(const std::string& str);
        bool appendLine(const std::string& line);
    };

    // Fix: Provide default Config() 
    explicit FileSystem(const Config& cfg = Config{}) : cfg_(cfg) {}
    ~FileSystem() { if (mounted_) unmount(); }

    // Lifecycle
    bool mount() {
        if (mounted_) return true;
        int err = pico_mount(cfg_.formatIfCorrupt);
        mounted_ = (err == 0);
        return mounted_;
    }
    
    void unmount() {
        if (!mounted_) return;
        pico_unmount();
        mounted_ = false;
    }
    
    bool format() {
        bool wasMounted = mounted_;
        if (mounted_) unmount();
        int err = pico_mount(true);  // true forces format
        if (err == 0 && !wasMounted) unmount();
        return err == 0;
    }
    
    bool isMounted() const { return mounted_; }
    
    // File operations
    File openFile(const char* path, int flags) {
        int fd = pico_open(path, flags);
        return File(fd);
    }
    
    File create(const char* path) { 
        return openFile(path, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC); 
    }
    
    File append(const char* path) { 
        return openFile(path, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_APPEND); 
    }
    
    File openRead(const char* path) { 
        return openFile(path, LFS_O_RDONLY); 
    }
    
    bool remove(const char* path) { return pico_remove(path) == 0; }
    bool rename(const char* oldPath, const char* newPath) { return pico_rename(oldPath, newPath) == 0; }
    
    bool exists(const char* path) {
        int fd = pico_open(path, LFS_O_RDONLY);
        if (fd < 0) return false;
        pico_close(fd);
        return true;
    }
    
    std::optional<uint32_t> fileSize(const char* path) {
        File f = openRead(path);
        if (!f.isOpen()) return std::nullopt;
        int32_t sz = f.size();
        if (sz < 0) return std::nullopt;
        return static_cast<uint32_t>(sz);
    }
    
    // Directory operations (pico_hal doesn't expose these, skip or use raw lfs if needed)
    bool createDir(const char* path) { return pico_mkdir(path) == 0; }
    bool removeDir(const char* path) { return pico_remove(path) == 0; }  // LittleFS remove works for both
    bool dirExists(const char* path) {
        // Check if it's a directory by trying to open as dir (not exposed in pico_hal)
        // For now, just return false or check if file exists (LittleFS can tell but pico_hal wraps it)
        return false;  // TODO: implement with raw lfs if needed
    }
    
    std::vector<std::string> listDir(const char* path = "/") {
        // pico_hal doesn't expose directory iteration
        // Return empty for now, or implement using raw littlefs if needed
        return {};
    }
    
    // One-shot helpers
    std::optional<std::vector<uint8_t>> readFile(const char* path) {
        File f = openRead(path);
        if (!f.isOpen()) return std::nullopt;
        return f.readAll();
    }
    
    bool writeFile(const char* path, const std::vector<uint8_t>& data) {
        File f = create(path);
        return f.isOpen() && f.writeAll(data);
    }
    
    bool appendFile(const char* path, const std::vector<uint8_t>& data) {
        File f = append(path);
        return f.isOpen() && f.writeAll(data);
    }
    
    std::optional<std::string> readString(const char* path) {
        auto data = readFile(path);
        if (!data) return std::nullopt;
        return std::string(data->begin(), data->end());
    }
    
    bool writeString(const char* path, const std::string& data) {
        return writeFile(path, std::vector<uint8_t>(data.begin(), data.end()));
    }
    
    bool appendString(const char* path, const std::string& data) {
        return appendFile(path, std::vector<uint8_t>(data.begin(), data.end()));
    }
    
    // Stats
    struct Stats {
        uint32_t blockSize;
        uint32_t blockCount;
        uint32_t blocksUsed;
        size_t totalBytes() const { return static_cast<size_t>(blockCount) * blockSize; }
        size_t usedBytes() const { return static_cast<size_t>(blocksUsed) * blockSize; }
        size_t freeBytes() const { return totalBytes() - usedBytes(); }
        uint8_t percentUsed() const { return blockCount ? static_cast<uint8_t>((blocksUsed * 100) / blockCount) : 0; }
    };
    
    std::optional<Stats> getStats() {
        pico_fsstat_t stat;
        int err = pico_fsstat(&stat);
        if (err < 0) return std::nullopt;
        
        Stats s;
        s.blockSize = stat.block_size;
        s.blockCount = stat.block_count;
        s.blocksUsed = stat.blocks_used;
        return s;
    }

private:
    Config cfg_;
    bool mounted_ = false;
};