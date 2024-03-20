#include "dir_scanner.h"

#include "crc32.h"
#include "defer.h"
#include "format.h"

#include <fstream>
#include <iostream>
#include <syslog.h>

namespace fs = std::filesystem;


DirScanner::DirScanner(const std::string& dir, int threadsCount, int threadQeueSize)
    : m_directory(dir)
{
    if (!fs::exists(m_directory)) {
        throw std::runtime_error(string::format("\"%s\" not exists", m_directory.c_str()));
    }
    if (!fs::is_directory(m_directory)) {
        throw std::runtime_error(string::format("\"%s\" is not a directory", m_directory.c_str()));
    }

    m_workerTreads.reset(new ThreadPool(threadsCount, threadQeueSize));
    init_crc_table();

    // init Watcher
    auto callback_fn = [this](const std::string& path)
    {
        m_waitGroup.Add();
        if (!m_workerTreads->addTask( std::bind(&DirScanner::calculateCrc, this, path, true) )) {
            syslog(LOG_ERR, "ThreadPool queue is full");
            std::cerr << "ThreadPool queue is full\n";
        }
    };
    SetCallback(callback_fn);
    AddWatch(dir);
}

void DirScanner::Scan(bool save) 
{
    m_ok.store(true);

    for (const auto& entry : fs::recursive_directory_iterator(m_directory)) {
        if (!entry.is_regular_file()) {
            // another rule of directory watching?
            if (entry.is_directory() && save) {
                AddWatch(entry.path());
            }
            continue;
        }

        m_waitGroup.Add();
        if (!m_workerTreads->addTask( std::bind(&DirScanner::calculateCrc, this, entry.path(), save) )) {
            m_waitGroup.Done();
            syslog(LOG_ERR, "ThreadPool queue is full");
            std::cerr << "ThreadPool queue is full\n";
        }
    }

    m_waitGroup.Wait();
    if (m_ok.load()) {
        syslog(LOG_INFO, "Integrity check: OK");
    }
}

// TODO: mapstruct, marshall or smth; remove last ','
void DirScanner::Save(const std::string& filename) {
    std::ofstream ofs(filename.c_str());
    if (!ofs.good()) {
        throw std::runtime_error(string::format("Unable to open %s", filename.c_str()));
    }
    ofs << "[\n";
    for (const auto& [path, info]: m_fileCrcMap) {
        ofs << string::format("{ \"path\": \"%s\", \"etalon_crc32\": \"0X%08X\", \"result_crc32\": \"0X%08X\", \"status\": %d},\n", path.c_str(), info.etalon_crc32, info.result_crc32, info.status);
    }
    ofs << "\n]";
}


// TODO its maybe better to split the func to separate read and write
void DirScanner::calculateCrc(const fs::path& filename, bool save) 
{
    Defer doOnScopeExit(
        [this]() { m_waitGroup.Done(); } );

    try {
        std::unordered_map<fs::path, file_info>::iterator it;
        {
            // TODO: equal_range for hash collision
            std::shared_lock lock(m_mutex);
            it = m_fileCrcMap.find(filename);
            if (it == m_fileCrcMap.end()) {
                if (!save) {
                    throw std::runtime_error("new file");
                }
            }
        }

        uint32_t crc;
        calc_crc(filename.c_str(), &crc);
        // std::cout << string::format("%08x\t%s\n", crc, filename.c_str());

        if (it != m_fileCrcMap.end()) {
            it->second.result_crc32 = crc;
            if (crc != it->second.etalon_crc32) {
                it->second.status = file_status::FAIL;
                throw std::runtime_error(
                    string::format("CRC mismatch: expected %08x  actual %08x", it->second.etalon_crc32, crc) );
            }
        }
        else if (save) {
            std::unique_lock lock(m_mutex);
            file_info info(crc);
            m_fileCrcMap[filename] = info;
        }
    }
    catch (const std::exception& e) {
        m_ok.store(false);
        syslog(LOG_ERR, "Integrity check: FAIL (%s - %s)", filename.c_str(), e.what());
    }
}
