#include <iostream>
#include <filesystem>
#include <boost/program_options.hpp>
#include <stdint.h>
#include <cstdarg>
#include <syslog.h>
#include <sys/inotify.h>
#include <shared_mutex>
#include <list>

#include "app/stacktrace.h"
#include "app/thread_pool.h"
#include "app/crc32.h"
#include "app/periodic_task.h"
#include "app/watcher.h"

namespace po = boost::program_options;
namespace fs = std::filesystem;

/*
Написать демон, выполняющий контроль целостности (crc32) файлов.

0) Среда Linux. Предоставить исходники со скриптами сборки (лучше cmake)

1) Путь до директории с файлами указывается либо через переменную окружения, либо через
параметр командной строки (последний в приоритете).
Требования по поведению 1. Брать из параметров командной строки, если не задано 2. Брать из
переменной окружения, если нет 3. Поднимать ошибку и писать в syslog;

2) Триггеры для проверки чек-сумм:
- На старте демон должен рассчитать чек-суммы файлов и "запомнить" их для последующих
сравнений;
- Периодически по таймеру (период указывается в секундах либо через переменную окружения, либо
через параметр командной строки (последний в приоритете));
- По сигналу SIGUSR1 (при этом должна быть организована очередь задач на проверку чек-сумм, т.е.
если сигнал поступил во время расчета чек-сумм по таймеру, то запрос на проверку не должен
игнорироваться).

3) Результат проверки должен логгироваться в syslog:
В случае успеха: Integrity check: OK
В случае неудачи: Integrity check: FAIL (%1 - %2)
, где
%1 - путь до файла с чек-суммой, отличной от рассчитанной на старте демона
%2 – сообщение об ошибке. Например, в случае расхождения чек-сумм привести пару <эталонная,
вычисленная> или сообщение о конкретной ошибке (нет доступа к файлу, удален или новый).

4) Завершение работы по SIGTERM (игнорировать SIGQUIT, SIGINT, SIGHUP, SIGSTOP, SIGCONT).
Будет плюсом:
- Добавить тест (желательно на python).
- Поставить директорию c файлами под мониторинг событий (inotify) и выполнять проверку чек-сумм в
случае изменения/добавления/удаления хотя бы одного файла в директории.
- Сделать распараллеливание обработки файла 1) через потоки 2) через порождаемые дочерние
процессы.
- Сохранить в файле результат расчета чек-сумм файлов в JSON-формате:
*/

/* TODO:
python json inotify,
*/
namespace {

inline std::string get_env(const std::string &var)
{
    const char * val = std::getenv(var.c_str());
    if ( !val )
        return std::string();
    else
        return std::string(val);
}


#define MAX_PATH_LEN = PATH_MAX
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

// defer?
class WaitGroup {
public:
    void Add(int incr = 1) { counter += incr; }
    void Done() { if (--counter <= 0) cond.notify_all(); }
    void Wait() {
        std::unique_lock<std::mutex> lock(mutex);
        cond.wait(lock, [&] { return counter <= 0; });
    }

private:
    std::mutex mutex;
    std::atomic<int> counter;
    std::condition_variable cond;
};


class Application : public Watcher {

    public:

        Application(const std::string& dir, int threadsCount, int threadQeueSize)
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
                if (!m_workerTreads->addTask( std::bind(&Application::calculateCrc, this, path, true) )) {
                    syslog(LOG_ERR, "ThreadPool queue is full");
                    std::cerr << "ThreadPool queue is full\n";
                }
            };
            SetCallback(callback_fn);
            AddWatch(dir);
        }

        Application operator=(const Application&) = delete;

        void HandleEvent(bool init=false) 
        {
            m_ok.store(true);

            for (const auto& entry : fs::recursive_directory_iterator(m_directory)) {
                if (!entry.is_regular_file()) {
                    // another rule of directory watching?
                    if (entry.is_directory() && init) {
                        AddWatch(entry.path());
                    }
                    continue;
                }

                m_waitGroup.Add();
                if (!m_workerTreads->addTask( std::bind(&Application::calculateCrc, this, entry.path(), init) )) {
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

        void CheckDir()
        {
            for (const auto& [filename, savedCrc]: m_fileCrcMap) {
                if (!fs::exists(filename)) {
                    syslog(LOG_ERR, "Integrity check: FAIL (%s - removed)", filename.c_str());
                }
                if (!m_workerTreads->addTask( std::bind(&Application::calculateCrc, this, filename, false) )) {
                    syslog(LOG_ERR, "ThreadPool queue is full");
                    std::cerr << "ThreadPool queue is full\n";
                }
            }
        }

    private:

        // TODO may be its better to split the func to separate ScanDir and CheckDir (write and read)
        void calculateCrc(const fs::path& filename, bool init) 
        {
            try {
                std::unordered_map<fs::path, uint32_t>::const_iterator it;
                {
                    // TODO: equal_range for hash collision
                    std::shared_lock lock(m_mutex);
                    it = m_fileCrcMap.find(filename);
                    if (it == m_fileCrcMap.end()) {
                        if (!init) {
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
                else if (init) {
                    std::unique_lock lock(m_mutex);
                    m_fileCrcMap[filename] = crc;
                }

                m_waitGroup.Done();
            }
            catch (const std::exception& e) {
                m_ok.store(false);
                m_waitGroup.Done();
                syslog(LOG_ERR, "Integrity check: FAIL (%s - %s)", filename.c_str(), e.what());
            }
        }

        const fs::path m_directory;
        std::unique_ptr<ThreadPool> m_workerTreads;
        std::shared_mutex m_mutex;
        std::unordered_map<fs::path, uint32_t> m_fileCrcMap;
        WaitGroup m_waitGroup;
        std::atomic<bool> m_ok;
};

// TODO systemd script?
int main(int argc, char** argv) {
    stacktrace::registerHandlers();

    std::string directory;
    int worker_threads, period, queue_size; // TODO too small period for a large dir queue management?

    po::options_description desc("Program options");
    desc.add_options()
        ("help,h", "Show help")
        ("daemonize,d", "daemonize")
        ("dir,D", po::value< std::string >(&directory)->default_value(""), "The directory to monitore, may be setted by CRC_SCAN_DIRECTORY environment variable")
        ("worker_threads,T", po::value< int >( &worker_threads )->default_value(0), "Number of worker threads used for crc check, 0 - auto")
        ("queue,Q", po::value< int >(&queue_size)->default_value(100000), "Size of files queue")
        ("period,P", po::value< int >( &period )->default_value(0), "Recalculating period in seconds, may be setted by CRC_SCAN_DIRECTORY_PERIOD environment variable");


    try
    {
        po::parsed_options parsed = po::command_line_parser(argc, argv).options(desc).allow_unregistered().run();
        po::variables_map vm;
        po::store(parsed, vm);
        po::notify(vm);

        if (vm.count("help")) {
            std::cout << desc << std::endl;
            return 0;
        }

        if (vm.count("daemonize")) {
             // daemon(nochdir, noclose)
            if( daemon(1, 0) != 0 ) {
                std::cerr << "Failed to daemonize " << argv[0] << " process: " << strerror(errno) << std::endl;
                return 2;
            }
        }

        if (directory.empty()) {
            directory = get_env("CRC_SCAN_DIRECTORY");
            if (directory.empty()) {
                throw std::runtime_error("directory is not specified");
            }
        }

        if (period == 0) {
            auto periodStr = get_env("CRC_SCAN_DIRECTORY_PERIOD");
            period = std::atoi(periodStr.c_str());
            if (!period) {
                period = 60;
                syslog(LOG_INFO, "period was set to one minute");
            }
        }

        if (worker_threads == 0) {
            worker_threads = std::max((unsigned int)1, (unsigned int)std::thread::hardware_concurrency());
        }


        auto app = std::make_unique<Application>(directory, worker_threads, queue_size);
        app->HandleEvent(true);
        app->RunWatcher();

        PeriodicTask crcUpdateTask(period, std::bind(&Application::HandleEvent, app.get(), false));

        bool sStop = false;
        do {
            pause();

            switch (stacktrace::g_signal)
            {
                case SIGTERM:
                    // stopping the application
                    sStop = true;
                    break;
                case SIGUSR1:
                    app->HandleEvent();
                    break;
                default:
                    break;
            }
        } while(!sStop);
    }
    catch(const std::exception &e)
    {
        syslog(LOG_CRIT, "ERROR: %s", e.what());
        std::cerr << e.what() << std::endl;
        return 1;
    }
}
