#pragma once

#define OBS_NDI_VERSION "5.0.0"

#define blog(level, msg, ...) blog(level, "[obs-ndi] " msg, ##__VA_ARGS__)
