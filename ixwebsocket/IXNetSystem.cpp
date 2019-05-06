/*
 *  IXNetSystem.cpp
 *  Author: Korchynskyi Dmytro
 *  Copyright (c) 2019 Machine Zone. All rights reserved.
 */

#include "IXNetSystem.h"

namespace ix
{
    bool initNetSystem()
    {
#ifdef _WIN32
        WORD wVersionRequested;
        WSADATA wsaData;
        int err;

        /* Use the MAKEWORD(lowbyte, highbyte) macro declared in Windef.h */
        wVersionRequested = MAKEWORD(2, 2);

        err = WSAStartup(wVersionRequested, &wsaData);

        return err == 0;
#else
        return true;
#endif
    }

    bool uninitNetSystem()
    {
#ifdef _WIN32
        int err = WSACleanup();

        return err == 0;
#else
        return true;
#endif
    }
}
