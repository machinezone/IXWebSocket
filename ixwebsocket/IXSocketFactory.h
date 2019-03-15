
/*
 *  IXSocketFactory.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include <memory>

namespace ix
{
    class Socket;
    std::shared_ptr<Socket> createSocket(bool tls,
                                         std::string& errorMsg);

    std::shared_ptr<Socket> createSocket(int fd,
                                         std::string& errorMsg);
}
