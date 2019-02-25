/*
 *  IXUrlParser.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#pragma once

#include <string>

namespace ix
{
    bool parseUrl(const std::string& url,
                  std::string& protocol,
                  std::string& host,
                  std::string& path,
                  std::string& query,
                  int& port);

    void printUrl(const std::string& url);
}
