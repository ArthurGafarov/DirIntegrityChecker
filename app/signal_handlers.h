#pragma once

#include <iostream>
#include <csignal>

#define BOOST_STACKTRACE_USE_ADDR2LINE
#include <boost/stacktrace.hpp>

namespace stacktrace {

void terminateHandler()
{
    std::exception_ptr exptr = std::current_exception();

    if (exptr != 0) {
        try {
            std::rethrow_exception(exptr);
        }
        catch (std::exception &ex) {
            std::cout << "CRITICAL ERROR     Terminated due to exception: " + std::string(ex.what()) << std::endl;
        }
        catch (...) {
            std::cout << "CRITICAL ERROR     Terminated due to unknown exception" << std::endl;
        }
    }
    else {
            std::cout << "CRITICAL ERROR      Terminated due to unknown reason" << std::endl;
    }
    std::cout << boost::stacktrace::stacktrace() << std::endl;
    std::abort();
}

int g_signal = 0;

void signalHandler(int signal)
{
    switch( signal )
    {
        case SIGTERM:
        case SIGUSR1:
            g_signal = signal;
            break;
        default:
            std::stringstream ss;
            ss << "CRITICAL ERROR      Aborting due to signal #";
            ss << signal << ": " << strsignal(signal) << '\n';
            ss << boost::stacktrace::stacktrace();

            std::cout << ss.str() << std::endl;
            std::abort();
    }
}

void registerHandlers()
{
    signal(SIGTERM, signalHandler);
    signal(SIGUSR1, signalHandler);
    signal(SIGSEGV, signalHandler);

    signal(SIGQUIT, SIG_IGN);
    signal(SIGINT, SIG_IGN);
    signal(SIGHUP, SIG_IGN);
    // signal(SIGSTOP, SIG_IGN); // uncathcable
    signal(SIGCONT, SIG_IGN); // why we should stub SIGCONT if SIGSTOP acts as usual?

    std::set_terminate(terminateHandler);
}

}
