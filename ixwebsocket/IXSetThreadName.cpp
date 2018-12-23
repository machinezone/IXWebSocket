/*
 *  IXSetThreadName.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
 */
#include "IXSetThreadName.h"
#include <pthread.h>

namespace ix
{
    void setThreadName(const std::string& name)
    {
#if defined(__linux__)
        //
        // Linux only reserve 16 bytes for its thread names
        // See prctl and PR_SET_NAME property in
        // http://man7.org/linux/man-pages/man2/prctl.2.html
        //
        pthread_setname_np(pthread_self(),
                           name.substr(0, 15).c_str());
#elif defined(__APPLE__)
        //
        // Apple is more generous with 64 chars.
        // notice how the Apple version does not take a pthread_t argument
        //
        pthread_setname_np(name.substr(0, 63).c_str());
#elif
        #error("Unsupported platform");
#endif
    }
}
