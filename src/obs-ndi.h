#pragma once

#include <memory>
#include <Processing.NDI.Lib.h>

#include "obs-ndi-macros.generated.h"

// For translations
#define T_(text) obs_module_text(text)

// Temporary debug override
#define LOG_DEBUG LOG_INFO

extern const NDIlib_v5* ndiLib;

extern NDIlib_find_instance_t ndi_finder;

bool restart_ndi_finder();

class obs_ndi_config;
typedef std::shared_ptr<obs_ndi_config> config_ptr;

config_ptr get_config();
