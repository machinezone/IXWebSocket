/*
 *  IXStrCaseCompare.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2020 Machine Zone. All rights reserved.
 */

#include "IXStrCaseCompare.h"

#include <algorithm>

namespace
{
    unsigned char asciiLower(unsigned char c)
    {
        return c >= 'A' && c <= 'Z' ? static_cast<unsigned char>(c + ('a' - 'A')) : c;
    }
} // namespace

namespace ix
{
    bool CaseInsensitiveLess::NocaseCompare::operator()(const unsigned char& c1,
                                                        const unsigned char& c2) const
    {
        return asciiLower(c1) < asciiLower(c2);
    }

    bool CaseInsensitiveLess::cmp(const std::string& s1, const std::string& s2)
    {
        return std::lexicographical_compare(s1.begin(),
                                            s1.end(), // source range
                                            s2.begin(),
                                            s2.end(),         // dest range
                                            NocaseCompare()); // comparison
    }

    bool CaseInsensitiveLess::operator()(const std::string& s1, const std::string& s2) const
    {
        return CaseInsensitiveLess::cmp(s1, s2);
    }
} // namespace ix
