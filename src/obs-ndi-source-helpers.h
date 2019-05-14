#pragma once
#include <obs-properties.h>
#include <Processing.NDI.Lib.h>
#include <util/dstr.h>


void serialize_ndi_source(const NDIlib_source_t *ndi_source, dstr *dest_serialized);

void deserialize_ndi_source(const char *serialized_ndi_source, char **dni_name, char **url);
