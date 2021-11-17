#pragma once

#include <string>
#include <util/config-file.h>

class obs_ndi_config {
	public:
		std::string ndi_extra_ips;

		obs_ndi_config();
		void load();
		void save();
		void set_defaults();
		config_t* get_config_store();
};
