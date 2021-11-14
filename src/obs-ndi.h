#pragma once

#include <util/base.h>
#include <Processing.NDI.Lib.h>

#include "obs-ndi-macros.generated.h"

#define blog(level, msg, ...) blog(level, "[obs-ndi] " msg, ##__VA_ARGS__)
#define QT_TO_UTF8(str) str.toUtf8().constData()

// For translations
#define T_(text) obs_module_text(text)

// Temporary debug override
#define LOG_DEBUG LOG_INFO

extern const NDIlib_v5* ndiLib;

extern NDIlib_find_instance_t ndi_finder;
