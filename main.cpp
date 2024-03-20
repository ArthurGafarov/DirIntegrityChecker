#include <boost/program_options.hpp>
#include <stdint.h>
#include <sys/inotify.h>
#include <syslog.h>

#include "app/dir_scanner.h"
#include "app/periodic_task.h"
#include "app/signal_handlers.h"

namespace po = boost::program_options;


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

/* TODO
json: extend map by statuses struct and write it to the file
python (what to test?)
fork (why to fork?)
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
}



// TODO: systemd script or it's mean to be a daemon()?
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


        auto app = std::make_unique<DirScanner>(directory, worker_threads, queue_size);
        app->Scan(true);
        app->RunWatcher();

        PeriodicTask crcUpdateTask(period, std::bind(&DirScanner::Scan, app.get(), false));

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
                    app->Scan();
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
