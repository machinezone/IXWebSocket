/*
 *  IXBench.h
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2017-2020 Machine Zone, Inc. All rights reserved.
 */

#include <string>
#include <chrono>
#include <stdint.h>

namespace ix
{
    class Bench
    {
    public:
        Bench(const std::string& description);
        ~Bench();

        void report();
        uint64_t getDuration() const;

    private:
        std::string _description;
        std::chrono::time_point<std::chrono::system_clock> _start;
        uint64_t _ms;
        bool _reported;
    };
}
