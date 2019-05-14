/*
obs-ndi
Copyright (C) 2016-2018 Stéphane Lepin <steph  name of author

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; If not, see <https://www.gnu.org/licenses/>
*/


//
// Created by Bastien Aracil on 13/05/2019.
//
#pragma once
#include <obs-properties.h>
#include <Processing.NDI.Lib.h>
#include <util/dstr.h>


void serialize_ndi_source(const NDIlib_source_t *ndi_source, dstr *dest_serialized);

void deserialize_ndi_source(const char *serialized_ndi_source, char **dni_name, char **url);
