#pragma once

#include <util/base.h>
#include <Processing.NDI.Lib.h>

#include "obs-ndi-macros.generated.h"

#define blog(level, msg, ...) blog(level, "[obs-ndi] " msg, ##__VA_ARGS__)
#define QT_TO_UTF8(str) str.toUtf8().constData()
