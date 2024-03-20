#pragma once

#include "thread_pool.h"
#include "waitgroup.h"
#include "watcher.h"

#include <cstring>
#include <filesystem>
#include <shared_mutex>
#include <stdint.h>


class DirScanner : public Watcher {

public:

    DirScanner(const std::string& dir, int threadsCount, int threadQeueSize);

    void Scan(bool save=false);
    void Save(const std::string& filename);

private:

    enum file_status {
        OK = 0,
        FAIL,
        NEW,
        ABSENT
    };

    typedef struct file_info_t {
        uint32_t etalon_crc32;
        uint32_t result_crc32;
        file_status status;

        file_info_t() {
            memset(this, 0x00, sizeof(*this));
        }
        file_info_t(uint32_t crc) : file_info_t() {
            etalon_crc32 = crc;
        }
    } file_info;

    // ATTENTION: m_waitGroup.Add() must be called before this function to synchronize output status
    void calculateCrc(const std::filesystem::path& filename, bool save);

    const std::filesystem::path m_directory;
    std::unique_ptr<ThreadPool> m_workerTreads;
    std::shared_mutex m_mutex;
    std::unordered_map<std::filesystem::path, file_info> m_fileCrcMap;
    // ATTENTION: possible deadlock or race condition, Done() MUST be called for each Add()
    WaitGroup m_waitGroup;
    std::atomic<bool> m_ok;
};
