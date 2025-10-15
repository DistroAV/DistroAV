#pragma once

#include <memory>
#include <QString>

namespace av {

class Transport {
public:
    virtual ~Transport() = default;

    virtual bool initialize() = 0;
    virtual void register_types() = 0;
    virtual void shutdown() = 0;

    virtual void main_output_init() = 0;
    virtual void main_output_deinit() = 0;
    virtual bool main_output_is_supported() = 0;
    virtual QString main_output_last_error() = 0;

    virtual void preview_output_init() = 0;
    virtual void preview_output_deinit() = 0;
};

// Accessor for the global transport instance
std::shared_ptr<Transport> get_transport();
void set_transport(std::shared_ptr<Transport> transport);

} // namespace av

