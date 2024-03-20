#pragma once

#include <cstdarg>
#include <string>

namespace string 
{

//#define BUFFER_LEN = PATH_MAX
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
