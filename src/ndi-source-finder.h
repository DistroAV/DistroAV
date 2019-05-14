//
// Created by Bastien Aracil on 13/05/2019.
//

#pragma once
#include <Processing.NDI.Lib.h>

typedef void (*ndi_source_consumer_t)(const NDIlib_source_t* ndi_source, void* private_data);

void destroy_ndi_finder();

void update_ndi_finder(const char *extraIps);

void foreach_current_ndi_source(ndi_source_consumer_t consumer, void* private_data);
