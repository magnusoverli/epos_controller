#pragma once

#include "EposProtocol.h"
#include "ReportTransport.h"

namespace Settings {

class Session {
  public:
    explicit Session(ReportTransport &transport, std::stop_token stop = {},
                     std::chrono::milliseconds replyTimeout = std::chrono::seconds{2})
        : m_transport(transport), m_stop(stop), m_replyTimeout(replyTimeout) {}
    [[nodiscard]] Result<Values> readAll();
    [[nodiscard]] Result<Firmware::Version> readFirmware();
    [[nodiscard]] Result<Values> apply(const Change &change);
    // Explicit destructive action: preflight, one output, acknowledgement and fresh readback.
    [[nodiscard]] Result<Values> factoryReset(const Values &expected);

  private:
    Result<EposProtocol::Message> exchange(const EposProtocol::Report &request);
    Result<std::uint16_t> read(Feature feature);
    ReportTransport &m_transport;
    std::stop_token m_stop;
    std::chrono::milliseconds m_replyTimeout;
    bool m_failed = false;
};

} // namespace Settings
