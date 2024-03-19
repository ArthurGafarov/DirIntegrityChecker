#pragma once

#include <functional>
#include <string>

class Watcher
{
public:
    Watcher();
    Watcher operator=(const Watcher&) = delete;
    ~Watcher();

    void AddWatch(const std::string& path);

    void SetCallback(const std::function< void(const std::string) > callback);

    void RunWatcher ();

private:

    std::function< void(const std::string) > m_fn;
    int m_fd;
    std::unordered_map<int, std::string> m_wdDirMap;
};
