#pragma once

#include <memory>

namespace av {

class Transport;

class TransportFactory {
public:
    virtual ~TransportFactory() = default;
    virtual std::shared_ptr<Transport> create() = 0;
};

} // namespace av

