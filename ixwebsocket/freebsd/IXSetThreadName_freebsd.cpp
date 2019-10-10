/*
 *  IXSetThreadName_freebsd.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */
#include "../IXSetThreadName.h"
#include <pthread.h>
#include <pthread_np.h>

namespace ix
{
    void setThreadName(const std::string& name)
    {
        pthread_set_name_np(pthread_self(), name.substr(0, 15).c_str());
    }
} // namespace ix
