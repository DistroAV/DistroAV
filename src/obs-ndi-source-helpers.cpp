#include "obs-ndi.h"
#include "obs-ndi-source-helpers.h"
#include "ndi-source-finder.h"
#include <util/dstr.h>
#include <obs-module.h>
#include <Processing.NDI.Lib.h>

const char* delim = "/";
const char* encoded_delim = "#2F";

static inline void encode_dstr(struct dstr* str) {
	dstr_replace(str, "#", "#22");
	dstr_replace(str, "/", "#2F");
}

static inline char* decode_str(const char* src) {
	struct dstr str = { nullptr };
	dstr_copy(&str, src);
	dstr_replace(&str, "#2F", "/");
	dstr_replace(&str, "#22", "#");
	return str.array;
}


void serialize_ndi_source(const NDIlib_source_t *ndi_source, dstr *dest_serialized) {
	struct dstr name = { nullptr,0,0 };
	struct dstr url = { nullptr,0,0 };

	dstr_init_copy(&name, ndi_source->p_ndi_name);
	dstr_init_copy(&url, ndi_source->p_url_address);

	encode_dstr(&name);
	encode_dstr(&url);

	dstr_cat_dstr(dest_serialized, &name);
	dstr_cat(dest_serialized, "/");
	dstr_cat_dstr(dest_serialized, &url);

	dstr_free(&url);
	dstr_free(&name);
}

static void add_ndi_source(const NDIlib_source_t* ndi_source,void* userData) {
    auto source_list = (obs_property_t*)userData;
    struct dstr serialized = {nullptr,0,0};
    serialize_ndi_source(ndi_source, &serialized);

    obs_property_list_add_string(source_list, ndi_source->p_ndi_name, serialized.array);

    dstr_free(&serialized);
}


void deserialize_ndi_source(const char *serialized_ndi_source, char **dni_name, char **url) {
    char **strlist;

    *dni_name = nullptr;
    *url = nullptr;
    if (!serialized_ndi_source) {
        return;
    }

    strlist = strlist_split(serialized_ndi_source,'/',true);

    if (strlist && strlist[0] && strlist[1]) {
        *dni_name = decode_str(strlist[0]);
        *url = decode_str(strlist[1]);
    }

    strlist_free(strlist);
}


