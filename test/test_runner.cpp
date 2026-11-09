/*
 *  test_runner.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018 Machine Zone. All rights reserved.
 */

#include <catch_amalgamated.hpp>
#include <common/IXLog.h>
#include <ixwebsocket/IXNetSystem.h>

#ifndef _WIN32
#include <signal.h>
#endif

int main(int argc, char* argv[])
{
    ix::initNetSystem();

#ifndef _WIN32
    signal(SIGPIPE, SIG_IGN);
#endif
    ix::logger::setLevel(ix::logger::Level::Debug);

    int result = Catch::Session().run(argc, argv);

    ix::uninitNetSystem();
    return result;
}
