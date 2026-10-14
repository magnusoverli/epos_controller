#include "SettingsSession.h"

namespace Settings {
Result<Firmware::Version> Session::readFirmware() {
    auto reply = exchange(EposProtocol::readFirmware());
    if (!reply)
        return std::unexpected(reply.error());
    return EposProtocol::firmwareVersion(*reply);
}
Result<EposProtocol::Message> Session::exchange(const EposProtocol::Report &request) {
    if (m_failed)
        return std::unexpected(Error{ErrorCode::Precondition, "Failed sessions cannot be reused."});
    m_failed = true; // Every early exit retires this session; no ambiguous retries.
    if (m_stop.stop_requested())
        return std::unexpected(Error{ErrorCode::Cancelled, "Settings operation cancelled."});
    const auto expected = EposProtocol::decode(request);
    if (!expected)
        return std::unexpected(expected.error());
    auto sent = m_transport.send(request, m_stop);
    if (!sent)
        return std::unexpected(sent.error());
    const auto deadline = std::chrono::steady_clock::now() + m_replyTimeout;
    // Bounded even for a faulty/flooding fake or native transport.
    for (int ignored = 0; ignored < 128 && std::chrono::steady_clock::now() < deadline; ++ignored) {
        if (m_stop.stop_requested())
            return std::unexpected(Error{ErrorCode::Cancelled, "Settings operation cancelled."});
        auto incoming = m_transport.receive(deadline, m_stop);
        if (!incoming)
            return std::unexpected(incoming.error());
        // Other HID report IDs and asynchronous vendor notifications are not replies.
        if (incoming->empty() || (*incoming)[0] != 1)
            continue;
        if (incoming->size() == EposProtocol::ReportSize && ((*incoming)[5] != 0 || (*incoming)[6] != 0))
            continue;
        auto message = EposProtocol::decode(*incoming);
        if (!message)
            return std::unexpected(message.error());
        if (!message->reply || message->operation != expected->operation)
            continue;
        m_failed = false;
        return *message;
    }
    return std::unexpected(Error{ErrorCode::Timeout, "No matching headset reply before the deadline."});
}

Result<std::uint16_t> Session::read(Feature feature) {
    auto reply = exchange(EposProtocol::read(feature));
    if (!reply)
        return std::unexpected(reply.error());
    auto value = EposProtocol::readValue(feature, *reply);
    if (!value)
        m_failed = true;
    return value;
}

Result<Values> Session::readAll() {
    Values values{};
    for (const auto feature : Features) {
        auto value = read(feature);
        if (!value) {
            auto error = value.error();
            error.message = std::string(key(feature)) + ": " + error.message;
            return std::unexpected(std::move(error));
        }
        values.set(feature, *value);
    }
    return values;
}

Result<Values> Session::apply(const Change &change) {
    // Re-read instead of trusting a UI cache. Never silently alter prerequisites.
    auto before = readAll();
    if (!before)
        return before;
    auto valid = validate(change, *before);
    if (!valid)
        return std::unexpected(valid.error());
    if (before->get(change.feature) == change.value)
        return before;
    auto request = EposProtocol::write(change.feature, change.value);
    if (!request)
        return std::unexpected(request.error());
    if (m_stop.stop_requested())
        return std::unexpected(Error{ErrorCode::Cancelled, "Settings operation cancelled before write."});
    auto uncertain = [](Error error) -> Result<Values> {
        error.writeMayHaveApplied = true;
        return std::unexpected(std::move(error));
    };
    auto reply = exchange(*request);
    if (!reply)
        return uncertain(reply.error());
    if (!EposProtocol::validWriteReply(change.feature, *reply)) {
        m_failed = true;
        return uncertain({ErrorCode::InvalidReport, "Unrecognized settings write reply."});
    }
    auto after = readAll();
    if (!after)
        return uncertain(after.error());
    for (const auto feature : Features) {
        const auto expected = feature == change.feature ? change.value : before->get(feature);
        if (after->get(feature) != expected)
            return uncertain(
                {ErrorCode::ReadbackMismatch,
                 "Device readback differs from the requested settings. Refresh to inspect current values."});
    }
    return after;
}
Result<Values> Session::factoryReset(const Values &expected) {
    auto before = readAll();
    if (!before)
        return before;
    if (*before != expected)
        return std::unexpected(
            Error{ErrorCode::StateChanged, "Headset values changed. Refresh and confirm reset again."});
    if (m_stop.stop_requested())
        return std::unexpected(Error{ErrorCode::Cancelled, "Factory reset cancelled before output."});
    auto uncertain = [](Error error) -> Result<Values> {
        error.writeMayHaveApplied = true;
        return std::unexpected(std::move(error));
    };
    auto reply = exchange(EposProtocol::factoryReset());
    if (!reply)
        return uncertain(reply.error());
    if (reply->value || reply->extra || reply->trailing) {
        m_failed = true;
        return uncertain({ErrorCode::InvalidReport, "Unrecognized factory reset reply."});
    }
    // Read actual resulting values; do not invent a factory-default tuple.
    // A reboot/disconnect is an uncertain outcome, never a reason to resend reset.
    auto after = readAll();
    if (!after)
        return uncertain(after.error());
    return after;
}
} // namespace Settings
