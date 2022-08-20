#pragma once

#include <string>
#include <util/config-file.h>

class obs_ndi_config {
public:
	std::string ndi_extra_ips;

	bool program_output_enabled;
	std::string program_output_name;

	bool preview_output_enabled;
	std::string preview_output_name;

	obs_ndi_config();
	void load();
	void save();
	void set_defaults();
	config_t *get_config_store();
};
