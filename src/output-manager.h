#pragma once

#include <obs.hpp>

class output_manager {
public:
	output_manager();
	~output_manager();

	obs_output_t *get_program_output(); // Increments ref
	void update_program_output();

private:
	OBSOutputAutoRelease _program_output;
};
