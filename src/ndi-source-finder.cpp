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


#include <cstring>
#include <util/bmem.h>
#include <util/dstr.h>
#include "ndi-source-finder.h"
#include "obs-ndi.h"
#include "ndi-utils.h"
#include <util/threading.h>

typedef struct {
    NDIlib_find_instance_t find_instance;
    struct dstr extra_ips;
    pthread_mutex_t mutex;
} ndi_finder_t;

static ndi_finder_t ndi_finder = {nullptr,{nullptr,0,0},PTHREAD_RECURSIVE_MUTEX_INITIALIZER};

static const char* get_effective_extra_ips(const char *extraIps) {
    if (!extraIps || !*extraIps) {
        return nullptr;
    }
    return extraIps;
}

static const bool update_current_extra_ips(const char *extraIps) {
    if (!is_extra_ips_valid(extraIps)) {
        ndiblog(LOG_WARNING, "Invalid IPS %s",extraIps);
        return false;
    }
    ndiblog(LOG_WARNING, "Valid IPS %s",extraIps);
    const char* effectiveIps = get_effective_extra_ips(extraIps);
    if (effectiveIps == ndi_finder.extra_ips.array) {
        return false;
    }
    //effectiveIps and ndi_finder_extra_ips.array cannot be both null
    if (effectiveIps == nullptr) {
        dstr_free(&ndi_finder.extra_ips);
        return true;
    }

    if (ndi_finder.extra_ips.array != nullptr && dstr_cmp(&ndi_finder.extra_ips, effectiveIps) == 0) {
        return false;
    } else {
        dstr_copy(&ndi_finder.extra_ips, effectiveIps);
        return true;
    }
}

static void destroy_current_ndi_finder() {
    if (ndi_finder.find_instance) {
        ndiblog(LOG_INFO, "destroy ndi_finder");
        ndiLib->NDIlib_find_destroy(ndi_finder.find_instance);
        ndi_finder.find_instance = nullptr;
    }
}

static void create_ndi_finder() {
    ndiblog(LOG_INFO, "create ndi_finder");
    NDIlib_find_create_t setting = {true,nullptr,ndi_finder.extra_ips.array};
    ndi_finder.find_instance = ndiLib->NDIlib_find_create_v2(&setting);
}

void destroy_ndi_finder() {
    pthread_mutex_lock(&ndi_finder.mutex);
    destroy_current_ndi_finder();
    pthread_mutex_unlock(&ndi_finder.mutex);

}

void update_ndi_finder(const char *extraIps) {
    pthread_mutex_lock(&ndi_finder.mutex);

    const bool should_change_ndi_finder = update_current_extra_ips(extraIps);

    if (should_change_ndi_finder) {
        destroy_current_ndi_finder();
        create_ndi_finder();
    }

    pthread_mutex_unlock(&ndi_finder.mutex);

}

void foreach_current_ndi_source(ndi_source_consumer_t consumer, void* private_data) {
    pthread_mutex_lock(&ndi_finder.mutex);

    if (!ndi_finder.find_instance) {
        create_ndi_finder();
    }

    uint32_t nb_sources = 0;
    const NDIlib_source_t *source_list = ndiLib->NDIlib_find_get_current_sources((ndi_finder.find_instance),&nb_sources);
    for (uint32_t i = 0; i < nb_sources; i++) {
        consumer(&source_list[i], private_data);
    }

    pthread_mutex_unlock(&ndi_finder.mutex);
}
