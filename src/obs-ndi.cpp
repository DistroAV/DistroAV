/*
obs-ndi (NDI I/O in OBS Studio)
Copyright (C) 2016-2017 St�phane Lepin <stephane.lepin@gmail.com>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library. If not, see <https://www.gnu.org/licenses/>
*/

#ifdef _WIN32
#include <Windows.h>
#endif

#include <sys/stat.h>

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/platform.h>
#include <QDir>
#include <QMainWindow>
#include <QAction>
#include <QMessageBox>
#include <QString>

#include "obs-ndi.h"
#include "Config.h"
#include "forms/output-settings.h"

OBS_DECLARE_MODULE()
OBS_MODULE_AUTHOR("Stephane Lepin (Palakis)")
OBS_MODULE_USE_DEFAULT_LOCALE("obs-ndi", "en-US")

const NDIlib_v3* ndiLib = NULL;

extern struct obs_source_info create_ndi_source_info();
struct obs_source_info ndi_source_info;

extern struct obs_output_info create_ndi_output_info();
struct obs_output_info ndi_output_info;

extern struct obs_source_info create_ndi_filter_info();
struct obs_source_info ndi_filter_info;

extern struct obs_source_info create_alpha_filter_info();
struct obs_source_info alpha_filter_info;

const NDIlib_v3* load_ndilib();
void* loaded_lib = NULL;

NDIlib_find_instance_t ndi_finder;
obs_output_t* main_out;
bool main_output_running = false;

OutputSettings* output_settings;

bool obs_module_load(void) {
    blog(LOG_INFO, "hello ! (version %s)", OBS_NDI_VERSION);

    QMainWindow* main_window = (QMainWindow*)obs_frontend_get_main_window();

    ndiLib = load_ndilib();
    if (!ndiLib) {
        QMessageBox::critical(main_window,
            obs_module_text("NDIPlugin.LibError.Title"),
            obs_module_text("NDIPlugin.LibError.Message"),
            QMessageBox::Ok, QMessageBox::NoButton);
        return false;
    }

    if (!ndiLib->NDIlib_initialize()) {
        blog(LOG_ERROR, "CPU unsupported by NDI library. Module won't load.");
        return false;
    }

    NDIlib_find_create_t find_desc = {0};
    find_desc.show_local_sources = true;
    find_desc.p_groups = NULL;
    ndi_finder = ndiLib->NDIlib_find_create_v2(&find_desc);

    ndi_source_info = create_ndi_source_info();
    obs_register_source(&ndi_source_info);

    ndi_output_info = create_ndi_output_info();
    obs_register_output(&ndi_output_info);

    ndi_filter_info = create_ndi_filter_info();
    obs_register_source(&ndi_filter_info);

    alpha_filter_info = create_alpha_filter_info();
    obs_register_source(&alpha_filter_info);

    Config* conf = Config::Current();
    conf->Load();

    // Ui setup
    QAction* menu_action = (QAction*)obs_frontend_add_tools_menu_qaction(
            obs_module_text("NDIPlugin.Menu.OutputSettings"));

    obs_frontend_push_ui_translation(obs_module_get_string);
    output_settings = new OutputSettings(main_window);
    obs_frontend_pop_ui_translation();

    auto menu_cb = [] {
        output_settings->ToggleShowHide();
    };
    menu_action->connect(menu_action, &QAction::triggered, menu_cb);

    obs_frontend_add_event_callback([](enum obs_frontend_event event,
        void *private_data)
    {
        if (event == OBS_FRONTEND_EVENT_EXIT)
            main_output_stop();
    }, NULL);

    // Run the server if configured
    if (conf->OutputEnabled)
        main_output_start(conf->OutputName.toUtf8().constData());

    return true;
}

void obs_module_unload()
{
    blog(LOG_INFO, "goodbye !");

    if (ndiLib) {
        ndiLib->NDIlib_find_destroy(ndi_finder);
        ndiLib->NDIlib_destroy();
    }

    if (loaded_lib)
        os_dlclose(loaded_lib);
}

const char* obs_module_name() {
    return "obs-ndi";
}

const char* obs_module_description() {
    return "NDI input/output integration for OBS Studio";
}

void main_output_start(const char* output_name) {
    if (!main_output_running) {
        blog(LOG_INFO, "starting main NDI output with name '%s'",
            qPrintable(Config::Current()->OutputName));

        obs_data_t* output_settings = obs_data_create();
        obs_data_set_string(output_settings, "ndi_name", output_name);

        main_out = obs_output_create("ndi_output", "main_ndi_output",
            output_settings, NULL);

        obs_output_start(main_out);
        obs_data_release(output_settings);

        main_output_running = true;
    }
}

void main_output_stop() {
    if (main_output_running) {
        blog(LOG_INFO, "stopping main NDI output");

        obs_output_stop(main_out);
        obs_output_release(main_out);
        main_output_running = false;
    }
}

bool main_output_is_running() {
    return main_output_running;
}

QString FindNDILib() {
    QString path;

    #if defined(_WIN32) || defined(_WIN64)
        QString runtime_dir = getenv(NDILIB_REDIST_FOLDER);
        if (runtime_dir.isEmpty()) {
            blog(LOG_ERROR, "Runtime environment variable not set.");
            return NULL;
        }

        blog(LOG_INFO, "Found NDI runtime at %s", runtime_dir);
        path = QDir(runtime_dir).absoluteFilePath(NDILIB_LIBRARY_NAME);
    #elif defined(__linux__)
        path = "/usr/lib/libndi.so";
    #elif defined(__APPLE__)
        struct stat stats;
        QString candidate =
            "/Library/Application Support/obs-studio/plugins/obs-ndi/bin/"
            + NDILIB_LIBRARY_NAME;
        if (os_stat(candidate.toUtf8().constData(), &stats) == 0) {
            path = candidate;
        } else {
            path = "./" + NDILIB_LIBRARY_NAME;
        }
    #endif

    blog(LOG_INFO, "Found NDI library at %s", path);
    return path;
}

const NDIlib_v2* load_ndilib()
{
    QString dll_file = FindNDILib();
    if (dll_file.isEmpty()) {
        blog(LOG_ERROR, "GetNDILibPath() returned a null pointer");
        return NULL;
    }

    loaded_lib = os_dlopen(dll_file.toUtf8().constData());
    if (loaded_lib) {
        const NDIlib_v2* (*lib_load)(void) =
            (const NDIlib_v2*(*)())os_dlsym(loaded_lib, "NDIlib_v2_load");
        return lib_load();
    }

    blog(LOG_ERROR, "Can't find the NDI library");
    return NULL;
}
