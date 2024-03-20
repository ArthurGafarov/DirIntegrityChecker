#include "dir_scanner.h"

#include "crc32.h"
#include "defer.h"

#include <cstdarg>
#include <iostream>
#include <syslog.h>

namespace fs = std::filesystem;

namespace {
    //#define MAX_PATH_LEN = PATH_MAX
    inline std::string format(const std::string fmt, ...) {
        va_list argL;
        va_start(argL, fmt);
        int len = vsnprintf(nullptr, 0, fmt.c_str(), argL);
        va_end(argL);

        std::string str(len, '\0');

        va_start(argL, fmt);
        vsnprintf(str.data(), len + 1, fmt.c_str(), argL);
        va_end(argL);

        return str;
    }
}

DirScanner::DirScanner(const std::string& dir, int threadsCount, int threadQeueSize)
    : m_directory(dir)
{
    if (!fs::exists(m_directory)) {
        throw std::runtime_error(format("\"%s\" not exists", m_directory.c_str()));
    }
    if (!fs::is_directory(m_directory)) {
        throw std::runtime_error(format("\"%s\" is not a directory", m_directory.c_str()));
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

#ifdef COMPILE_UNUSED_FUNC
void DirScanner::Check()
{
    for (const auto& [filename, savedCrc]: m_fileCrcMap) {
        if (!fs::exists(filename)) {
            syslog(LOG_ERR, "Integrity check: FAIL (%s - removed)", filename.c_str());
        }
        if (!m_workerTreads->addTask( std::bind(&DirScanner::calculateCrc, this, filename, false) )) {
            syslog(LOG_ERR, "ThreadPool queue is full");
            std::cerr << "ThreadPool queue is full\n";
        }
    }
}
#endif


// TODO its maybe better to split the func to separate read and write
void DirScanner::calculateCrc(const fs::path& filename, bool save) 
{
    Defer doOnScopeExit(
        [this]() { m_waitGroup.Done(); } );

    try {
        std::unordered_map<fs::path, uint32_t>::const_iterator it;
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
        // std::cout << format("%08x\t%s\n", crc, filename.c_str());

        if (it != m_fileCrcMap.end()) {
            if (crc != it->second) {
                throw std::runtime_error(
                    format("CRC mismatch: expected %08x  actual %08x", it->second, crc) );
            }
        }
        else if (save) {
            std::unique_lock lock(m_mutex);
            m_fileCrcMap[filename] = crc;
        }
    }
    catch (const std::exception& e) {
        m_ok.store(false);
        syslog(LOG_ERR, "Integrity check: FAIL (%s - %s)", filename.c_str(), e.what());
    }
}
