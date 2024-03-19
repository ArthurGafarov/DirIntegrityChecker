#include "watcher.h"

#include <sys/inotify.h>
#include <stdexcept>
#include <unistd.h>
#include <limits.h>
#include <syslog.h>
#include <thread>


Watcher::Watcher() {
    m_fd = inotify_init();

    if ( m_fd < 0 ) {
        throw std::runtime_error("Cannot initialize inotify");
    }
}

Watcher::~Watcher() {
    for (auto& wd: m_wdDirMap) {
        inotify_rm_watch( m_fd, wd.first );
    }
    close( m_fd );
}

void Watcher::AddWatch(const std::string& path) {
    int wd = inotify_add_watch(m_fd, path.c_str(), IN_DELETE | IN_CREATE | IN_CLOSE_WRITE);
    if ( wd < 0 ) {
        throw std::runtime_error("Cannot add watch to the directory");
    }
    m_wdDirMap[wd] = path;
}

void Watcher::SetCallback(const std::function< void(const std::string) > callback) {
    m_fn = callback;
}

void Watcher::RunWatcher () {
    auto loop = [this]()
    {
        const size_t MAX_EVENTS = 1024;
        const size_t EVENT_SIZE = sizeof(inotify_event) + NAME_MAX;
        const size_t BUFF_SIZE = MAX_EVENTS * EVENT_SIZE;
        auto buf = std::make_unique<char[]>(BUFF_SIZE);
        char* buffer = buf.get();

        auto handle_event = [this](const inotify_event* event)
        {
            const std::string path = m_wdDirMap[event->wd] + "/" + event->name;

            if ( event->mask & IN_DELETE) {
                if (event->mask & IN_ISDIR)
                    syslog(LOG_ERR, "Integrity check: FAIL (%s - the directory was removed)", path.c_str());
                else 
                    syslog(LOG_ERR, "Integrity check: FAIL (%s - the file was removed)", path.c_str());
            }
            if ( event->mask & IN_CREATE) {
                if (event->mask & IN_ISDIR)
                    syslog(LOG_INFO, "The directory %s was created", path.c_str());
                else {
                    syslog(LOG_INFO, "The file %s was created, recalculating", path.c_str());
                    m_fn(path);
                }
            }
            if ( event->mask & IN_CLOSE_WRITE) {
                if (event->mask & IN_ISDIR)
                    syslog(LOG_INFO, "The directory %s was modified", path.c_str());
                else {
                    syslog(LOG_INFO, "The file %s was modified, recalculating", path.c_str());
                    m_fn(path);
                }
            }
        };

        while (true) {
            int size = read(m_fd, buffer, BUFF_SIZE);

            int i = 0;
            while(i < size) {
                inotify_event* event = (inotify_event*)&buffer[i];
                if ( event->len ) {
                    handle_event(event);
                    i += sizeof(inotify_event) + event->len;
                }

            }
        }
    };

    std::thread watchThread(loop);
    watchThread.detach();
}
