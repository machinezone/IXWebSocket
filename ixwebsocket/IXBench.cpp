/*
 *  IXBench.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2017-2020 Machine Zone, Inc. All rights reserved.
 */

#include "IXBench.h"

#include <iostream>

namespace ix
{
    Bench::Bench(const std::string& description)
        : _description(description)
        , _start(std::chrono::high_resolution_clock::now())
        , _reported(false)
    {
        ;
    }

    Bench::~Bench()
    {
        if (!_reported)
        {
            report();
        }
    }

    void Bench::report()
    {
        auto now = std::chrono::high_resolution_clock::now();
        auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now - _start);

        _ms = milliseconds.count();
        std::cerr << _description << " completed in " << _ms << "ms" << std::endl;

        _reported = true;
    }

    uint64_t Bench::getDuration() const
    {
        return _ms;
    }
} // namespace ix
