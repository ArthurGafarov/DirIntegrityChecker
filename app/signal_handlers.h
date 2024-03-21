#pragma once

#include "format.h"

#include <iostream>
#include <csignal>
#include <execinfo.h>

/*
#define BOOST_STACKTRACE_USE_ADDR2LINE
#include <boost/stacktrace.hpp>
*/
namespace stacktrace {

// no boost version
#define STACK_TRACE_BUFFER_SIZE 500
void _print_stacktrace()
{
    void* buffer[STACK_TRACE_BUFFER_SIZE];
    int bt_size = backtrace(buffer, STACK_TRACE_BUFFER_SIZE);
    char** bt_messages = backtrace_symbols(buffer, bt_size);
    if (bt_messages == NULL) {
        std::cout << "CRITICAL ERROR: backtrace_symbols is NULL\n";
        exit(EXIT_FAILURE);
    }
    std::string bt_formatted;
    for (int frame = 0; frame < bt_size; frame++) {
        bt_formatted += string::format(
            "\n[bt]: (%d) %s", frame, bt_messages[frame]);
    }
    std::cout << string::format("Backtrace returned %d frames:\n%s", bt_size, bt_formatted.c_str());
    free(bt_messages);
}

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
    // std::cout << boost::stacktrace::stacktrace() << std::endl;
    _print_stacktrace();
    std::abort();
}

int g_signal = 0;

void signalHandler(int signal)
{
    switch( signal )
    {
        case SIGTERM:
        case SIGUSR1:
        case SIGUSR2:
            g_signal = signal;
            break;
        default:
            std::stringstream ss;
            ss << "CRITICAL ERROR      Aborting due to signal #";
            ss << signal << ": " << strsignal(signal) << '\n';
            //ss << boost::stacktrace::stacktrace();
            _print_stacktrace();

            std::cout << ss.str() << std::endl;
            std::abort();
    }
}

void registerHandlers()
{
    signal(SIGTERM, signalHandler);
    signal(SIGUSR1, signalHandler);
    signal(SIGUSR2, signalHandler);
    signal(SIGSEGV, signalHandler);

    signal(SIGQUIT, SIG_IGN);
    signal(SIGINT, SIG_IGN);
    signal(SIGHUP, SIG_IGN);
    // signal(SIGSTOP, SIG_IGN); // uncathable
    signal(SIGCONT, SIG_IGN); // why should we  stub SIGCONT if SIGSTOP acts as usual?

    std::set_terminate(terminateHandler);
}

}
