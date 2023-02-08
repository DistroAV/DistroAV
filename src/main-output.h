#pragma once

void main_output_init(const char *default_name);
void main_output_start(const char *output_name);
void main_output_stop();
void main_output_deinit();
bool main_output_is_running();
