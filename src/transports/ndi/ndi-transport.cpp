#include "ndi-transport.hpp"
#include "ndi-common.h"
#include "../plugin-main.h"
#include "main-output.h"
#include "preview-output.h"

#include <obs-module.h>
#include <QDir>
#include <QFileInfo>
#include <QLibrary>
#include <QRegularExpression>

namespace av {

static QLibrary *loaded_lib = nullptr;
const NDIlib_v6 *ndiLib = nullptr;

static const NDIlib_v6 *load_ndilib()
{
    auto locations = QStringList();
    auto temp_path = QString(qgetenv(NDILIB_REDIST_FOLDER));
    if (!temp_path.isEmpty()) {
        locations << temp_path;
    }
#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
    locations << "/usr/lib";
    locations << "/usr/local/lib";
#if defined(Q_OS_LINUX)
    locations << "/app/plugins/DistroAV/extra/lib";
#endif
#endif
    auto lib_path = QString();
#if defined(Q_OS_LINUX)
    auto regex = QRegularExpression("libndi\\.so\\.(\\d+)");
    int max_version = 0;
#endif
    for (const auto &location : locations) {
        auto dir = QDir(location);
#if defined(Q_OS_LINUX)
        auto filters = QStringList("libndi.so.*");
        dir.setNameFilters(filters);
        auto file_names = dir.entryList(QDir::Files);
        for (const auto &file_name : file_names) {
            auto match = regex.match(file_name);
            if (match.hasMatch()) {
                int version = match.captured(1).toInt();
                if (version > max_version) {
                    max_version = version;
                    lib_path = dir.absoluteFilePath(file_name);
                }
            }
        }
#else
        temp_path = QDir::cleanPath(dir.absoluteFilePath(NDILIB_LIBRARY_NAME));
        obs_log(LOG_DEBUG, "load_ndilib: Trying '%s'", QT_TO_UTF8(QDir::toNativeSeparators(temp_path)));
        auto file_info = QFileInfo(temp_path);
        if (file_info.exists() && file_info.isFile()) {
            lib_path = temp_path;
            break;
        }
#endif
    }
    if (!lib_path.isEmpty()) {
        obs_log(LOG_DEBUG, "load_ndilib: Found '%s'; attempting to load NDI library...", QT_TO_UTF8(QDir::toNativeSeparators(lib_path)));
        loaded_lib = new QLibrary(lib_path, nullptr);
        if (loaded_lib->load()) {
            obs_log(LOG_DEBUG, "load_ndilib: NDI library loaded successfully");
            using NDIlib_v6_load_ = const NDIlib_v6 *(*)(void);
            auto lib_load = reinterpret_cast<NDIlib_v6_load_>(loaded_lib->resolve("NDIlib_v6_load"));
            if (lib_load != nullptr) {
                obs_log(LOG_DEBUG, "load_ndilib: NDIlib_v6_load found");
                return lib_load();
            } else {
                obs_log(LOG_DEBUG, "load_ndilib: ERROR: NDIlib_v6_load not found in loaded library");
            }
        } else {
            obs_log(LOG_DEBUG, "load_ndilib: ERROR: QLibrary returned the following error: '%s'", QT_TO_UTF8(loaded_lib->errorString()));
            delete loaded_lib;
            loaded_lib = nullptr;
        }
    }
    obs_log(LOG_DEBUG, "load_ndilib: ERROR: Can't find the NDI library");
    return nullptr;
}

static bool is_version_supported(const char *version, const char *min_version)
{
    if (version == nullptr || min_version == nullptr) {
        return false;
    }
    auto version_parts = QString::fromUtf8(version).split('.');
    auto min_version_parts = QString::fromUtf8(min_version).split('.');
    for (int i = 0; i < qMax(version_parts.size(), min_version_parts.size()); ++i) {
        auto version_part = i < version_parts.size() ? version_parts[i].toInt() : 0;
        auto min_version_part = i < min_version_parts.size() ? min_version_parts[i].toInt() : 0;
        if (version_part > min_version_part) {
            return true;
        } else if (version_part < min_version_part) {
            return false;
        }
    }
    return true;
}

bool NdiTransport::initialize()
{
    ndiLib = load_ndilib();
    if (!ndiLib) {
        return false;
    }
    if (!ndiLib->initialize()) {
        obs_log(LOG_ERROR, "ERR-406 - NDI library could not initialize due to unsupported CPU.");
        return false;
    }
    QString ndi_version_short = QRegularExpression(R"((\d+\.\d+(\.\d+)?(\.\d+)?$))").match(ndiLib->version()).captured(1);
    obs_log(LOG_INFO, "NDI Version detected: %s", QT_TO_UTF8(ndi_version_short));
    if (!is_version_supported(QT_TO_UTF8(ndi_version_short), PLUGIN_MIN_NDI_VERSION)) {
        obs_log(LOG_ERROR, "ERR-425 - %s requires at least NDI version %s. NDI Version detected: %s.",
                PLUGIN_DISPLAY_NAME, PLUGIN_MIN_NDI_VERSION, QT_TO_UTF8(ndi_version_short));
        return false;
    }
    obs_log(LOG_INFO, "obs_module_load: NDI library initialized successfully");
    return true;
}

extern obs_source_info create_ndi_source_info();
extern obs_output_info create_ndi_output_info();
extern obs_source_info create_ndi_filter_info();
extern obs_source_info create_ndi_audiofilter_info();
extern obs_source_info create_alpha_filter_info();

void NdiTransport::register_types()
{
    auto source_info = create_ndi_source_info();
    obs_register_source(&source_info);
    auto output_info = create_ndi_output_info();
    obs_register_output(&output_info);
    auto filter_info = create_ndi_filter_info();
    obs_register_source(&filter_info);
    auto audiofilter_info = create_ndi_audiofilter_info();
    obs_register_source(&audiofilter_info);
    auto alpha_info = create_alpha_filter_info();
    obs_register_source(&alpha_info);
}

void NdiTransport::shutdown()
{
    if (ndiLib) {
        ndiLib->destroy();
        ndiLib = nullptr;
    }
    if (loaded_lib) {
        delete loaded_lib;
        loaded_lib = nullptr;
    }
}

void NdiTransport::main_output_init() { ::main_output_init(); }
void NdiTransport::main_output_deinit() { ::main_output_deinit(); }
bool NdiTransport::main_output_is_supported() { return ::main_output_is_supported(); }
QString NdiTransport::main_output_last_error() { return ::main_output_last_error(); }
void NdiTransport::preview_output_init() { ::preview_output_init(); }
void NdiTransport::preview_output_deinit() { ::preview_output_deinit(); }

std::shared_ptr<Transport> NdiTransportFactory::create()
{
    return std::make_shared<NdiTransport>();
}

} // namespace av

