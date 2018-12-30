/*
 *  IXSetThreadName_apple.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
 */
#include "../IXSetThreadName.h"
#include <pthread.h>

namespace ix
{
    void setThreadName(const std::string& name)
    {
        //
        // Apple reserves 16 bytes for its thread names
        // Notice that the Apple version of pthread_setname_np
        // does not take a pthread_t argument
        //
        pthread_setname_np(name.substr(0, 63).c_str());
    }
}
