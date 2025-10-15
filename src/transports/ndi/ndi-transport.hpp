#pragma once

#include "../../core/transport.hpp"
#include "../../core/transport_factory.hpp"

namespace av {

class NdiTransport : public Transport {
public:
    bool initialize() override;
    void register_types() override;
    void shutdown() override;

    void main_output_init() override;
    void main_output_deinit() override;
    bool main_output_is_supported() override;
    QString main_output_last_error() override;

    void preview_output_init() override;
    void preview_output_deinit() override;
};

class NdiTransportFactory : public TransportFactory {
public:
    std::shared_ptr<Transport> create() override;
};

} // namespace av

