#pragma once

#include <obs.hpp>
#include <obs-frontend-api.h>

#include "aux-output.h"

class output_manager {
public:
	output_manager();
	~output_manager();

	obs_output_t *get_program_output(); // Increments ref
	void update_program_output();
	void update_preview_output();

private:
	OBSOutputAutoRelease _program_output;
	aux_output _preview_output;

	static void on_frontend_event(enum obs_frontend_event event, void *priv_data);
};
