#pragma once

#include <map>
#include <string>
#include <chrono>
#include <obs-module.h>
#include "plugin-support.h"

void obs_sync_debug_log(const char *checkpoint, const char *source_ndi_name,
			uint64_t timecode, uint64_t timestamp);

void obs_clear_last_log_time();