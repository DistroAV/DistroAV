#pragma once

#include <string>

#include <obs.hpp>
#include <obs-frontend-api.h>

// A wrapper for the NDI output and obs_view_t which handles the complexities of interfacing between the two
class aux_output {
public:
	aux_output(std::string output_name);
	~aux_output();

	bool start(bool audio_enabled = true);
	void stop();

	bool set_ndi_source_name(std::string name);

	obs_source_t *get_source();
	void set_source(obs_source_t *source);

private:
	OBSOutputAutoRelease _output;
	obs_view_t *_view;
	video_t *_video;
};
