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
