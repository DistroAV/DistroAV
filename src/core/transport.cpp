#include "transport.hpp"

namespace av {
namespace {
std::shared_ptr<Transport> g_transport;
} // namespace

std::shared_ptr<Transport> get_transport() { return g_transport; }

void set_transport(std::shared_ptr<Transport> transport) {
    g_transport = std::move(transport);
}

} // namespace av

