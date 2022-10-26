#pragma once

void preview_output_init(const char *default_name);
void preview_output_start(const char *output_name);
void preview_output_stop();
void preview_output_deinit();
bool preview_output_is_enabled();