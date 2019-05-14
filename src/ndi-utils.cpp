/*
obs-ndi
Copyright (C) 2016-2018 Stéphane Lepin <steph  name of author

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; If not, see <https://www.gnu.org/licenses/>
*/


//
// Created by Bastien Aracil on 13/05/2019.
//

#include <cstdint>
#include "ndi-utils.h"

static const int MINIMAL_IP_ADDRESS_SIZE = 7;

static bool is_single_ip_valid(const char *ips, const uint32_t start, const uint32_t end) {
    if ((end - start) < MINIMAL_IP_ADDRESS_SIZE) {
        return false;
    }
    bool hasDigits = false;
    int nbNumbers = 0;
    for (uint32_t i = start; i < end; i++) {
        char c = ips[i];
        if (c >= '0' && c <= '9') {
            if (!hasDigits) {
                ++nbNumbers;
            }
            hasDigits = true;
        }
        else if (c == '.') {
            if (hasDigits) {
                hasDigits = false;
            }
            else {
                return false;
            }
        }
        else {
            return false;
        }
    }
    return hasDigits && nbNumbers == 4;
}


bool is_extra_ips_valid(const char *extra_ips) {
    if (!extra_ips || !*extra_ips) {
        return true;
    }
    uint32_t start = 0;
    uint32_t end = 0;
    while (extra_ips[start]) {
        while (extra_ips[end] != 0 && extra_ips[end] != ',') {
            ++end;
        }
        if (!is_single_ip_valid(extra_ips, start, end)) {
            return false;
        }
        start = end;
        ++end;
    }
    return true;
}
