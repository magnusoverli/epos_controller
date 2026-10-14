#include "FakeSettingsTransport.h"
#include "MuteEventFixtures.h"
#include "settings/SettingsSession.h"
#include <cstdio>
#include <source_location>
#include <utility>

namespace {
void check(bool condition, const char *message,
           std::source_location location = std::source_location::current()) {
    if (condition)
        return;
    std::fprintf(stderr, "%s:%u: %s\n", location.file_name(), location.line(), message);
    std::exit(1);
}
} // namespace

int main() {
    using namespace Settings;
    check(EposProtocol::readFirmware() == CapturedSettings::report(CapturedFirmware::Request),
          "Firmware getter matches captured request");
    const auto firmwareReply = EposProtocol::decode(CapturedSettings::report(CapturedFirmware::Reply));
    check(firmwareReply && EposProtocol::firmwareVersion(*firmwareReply)->text() == "1.5.64",
          "Captured firmware reply decodes three little-endian version words");
    auto shortVersion = *firmwareReply;
    shortVersion.trailing.reset();
    check(!EposProtocol::firmwareVersion(shortVersion), "Reject truncated version tuple");
    shortVersion = *firmwareReply;
    shortVersion.reply = false;
    check(!EposProtocol::firmwareVersion(shortVersion), "Reject version request as reply");
    for (const auto &pair : SettingsTestPairs) {
        const auto expected = CapturedSettings::report(pair.request);
        const auto encoded = pair.write ? EposProtocol::write(pair.feature, pair.value)
                                        : Result<EposProtocol::Report>(EposProtocol::read(pair.feature));
        check(encoded && *encoded == expected, "Encoder matches independent fixture bytes");
        const auto raw = CapturedSettings::report(pair.response);
        const auto decoded = EposProtocol::decode(raw);
        check(decoded && decoded->reply, "Captured reply accepted");
        check(decoded->operation == EposProtocol::operation(pair.feature, pair.write), "Reply operation");
        if (!pair.write)
            check(EposProtocol::readValue(pair.feature, *decoded) == pair.value,
                  "Read value from fixture reply");
        else
            check(EposProtocol::validWriteReply(pair.feature, *decoded), "Recognized setter reply shape");
        for (std::size_t size = 0; size < raw.size(); ++size)
            check(!EposProtocol::decode(std::span(raw).first(size)), "Reject every truncated length");
        for (const auto offset : {0, 1, 2, 3, 4, 7, 8, 9, 10, 11, 12, 13, 18, 106}) {
            auto bad = raw;
            bad[static_cast<std::size_t>(offset)] ^= 0x80;
            check(!EposProtocol::decode(bad), "Reject invalid header, body/checksum and padding");
        }
    }
    check(!EposProtocol::write(Feature::AncMode, 3), "Reject mode beyond TalkThrough");
    check(!EposProtocol::write(Feature::AutoPause, 2) && !EposProtocol::write(Feature::Prompts, 3) &&
              !EposProtocol::write(Feature::Prompts, 4) && !EposProtocol::write(Feature::Prompts, 5),
          "Expose only traced prompt choices for this headset, not other models' modes");
    check(!featureForKey("factoryReset"), "Factory reset is not a writable value feature");
    check(!EposProtocol::write(Feature::PromptLanguage, 6) &&
              !EposProtocol::write(Feature::PromptLanguage, 8) &&
              !EposProtocol::write(Feature::PromptLanguage, 0xffff),
          "Exclude Russian and unknown languages from this model's writable choices");
    auto language = *EposProtocol::decode(CapturedSettings::report(EnumeratedSettings::Pairs[14].response));
    language.value = 0x1234;
    check(EposProtocol::readValue(Feature::PromptLanguage, language) == 0x1234,
          "Unknown native language readings remain lossless");
    language.extra = 0;
    check(!EposProtocol::readValue(Feature::PromptLanguage, language), "Language read rejects extra words");
    language.operation = EposProtocol::operation(Feature::PromptLanguage, true);
    language.extra.reset();
    for (const std::uint16_t status : {1, 2, 0xffff}) {
        language.value = status;
        check(!EposProtocol::validWriteReply(Feature::PromptLanguage, language),
              "Language setter accepts only status zero, never starts a package-update fallback");
    }
    language.value.reset();
    check(!EposProtocol::validWriteReply(Feature::PromptLanguage, language),
          "Language setter requires an explicit status word");
    check(EposProtocol::factoryReset() == CapturedSettings::report(DerivedActions::ResetRequest),
          "Factory reset matches independent derived bytes");
    check(!EposProtocol::write(Feature::AncLevel, 3), "Reject level beyond High");
    check(!EposProtocol::write(Feature::AdaptiveAnc, 2), "Adaptive remains boolean");
    check(!EposProtocol::write(Feature::Sidetone, 256), "Reject raw sidetone beyond byte range");
    check(!sidetoneFromPercent(-1) && !sidetoneFromPercent(101) && !sidetonePercent(256),
          "Reject out-of-range percentages and native values, rather than clamping");
    // Boundary and half-step expectations, independent of the conversion implementation.
    constexpr std::pair<int, std::uint16_t> percentages[]{
        {0, 0}, {1, 3}, {10, 26}, {30, 77}, {32, 82}, {33, 84}, {50, 128}, {75, 191}, {99, 252}, {100, 255}};
    for (const auto &[percent, raw] : percentages) {
        check(sidetoneFromPercent(percent) == raw, "Percentage conversion, including round-half-up");
        check(sidetonePercent(raw) == percent, "Native value display percentage");
    }
    check(sidetonePercent(81) == 32 && sidetonePercent(82) == 32,
          "Distinct native values can share one display percentage");

    auto device = std::make_shared<FakeSettingsDevice>();
    FakeSettingsTransport transport(device);
    Session session(transport);
    for (const auto &fixture : CapturedMuteEvents::Reports) {
        const auto event = CapturedSettings::report(fixture.hex);
        transport.incoming.emplace_back(event.begin(), event.end());
    }
    auto values = session.readAll();
    check(values && *values == device->values && device->sends == int(Features.size()), "Read all fields");
    // Unrelated input and a valid but wrong-operation reply precede the real reply.
    transport.incoming.push_back({2, 0});
    const auto other = CapturedSettings::report(CapturedSettings::Pairs[6].response);
    transport.incoming.emplace_back(other.begin(), other.end());
    auto changed = session.apply({Feature::Sidetone, 84, *values});
    check(changed && changed->sidetone == 84 && device->writes == 1, "Write followed by full readback");
    auto restored = session.apply({Feature::Sidetone, 82, *changed});
    check(restored && *restored == *values && device->writes == 2, "Exact original raw value restoration");
    const auto stale = session.apply({Feature::Sidetone, 82, *changed});
    check(!stale && stale.error().code == ErrorCode::StateChanged && device->writes == 2,
          "Reject stale cached values before output");
    auto adaptive = session.apply({Feature::AdaptiveAnc, 1, *restored});
    check(adaptive && adaptive->adaptiveAnc == 1, "Adaptive setter");
    auto dependent = session.apply({Feature::AncLevel, 1, *adaptive});
    check(!dependent && dependent.error().code == ErrorCode::Precondition && device->writes == 3,
          "ANC level dependency enforced");
    auto off = session.apply({Feature::AncMode, 0, *adaptive});
    check(off && off->ancMode == 0, "ANC mode setter");
    check(!session.apply({Feature::AdaptiveAnc, 0, *off}), "Adaptive requires ANC on");

    auto talk = session.apply({Feature::AncMode, 2, *off});
    check(talk && talk->ancMode == 2, "TalkThrough full readback");
    const auto writesBefore = device->writes;
    check(!session.apply({Feature::AncLevel, 0, *talk}) && !session.apply({Feature::AdaptiveAnc, 0, *talk}) &&
              device->writes == writesBefore,
          "TalkThrough does not satisfy ANC dependencies");
    auto on = session.apply({Feature::AncMode, 1, *talk});
    check(on.has_value(), "Restore ANC mode");
    auto manual = session.apply({Feature::AdaptiveAnc, 0, *on});
    check(manual.has_value(), "Disable Adaptive before level test");
    auto low = session.apply({Feature::AncLevel, 0, *manual});
    check(low && low->ancLevel == 0, "Low ANC full readback");
    auto state = *low;
    for (const std::uint16_t raw : {0, 128, 255, 81, 82}) {
        const auto result = session.apply({Feature::Sidetone, raw, state});
        check(result && result->sidetone == raw, "Extended sidetone write and lossless restoration");
        state = *result;
    }

    // Exercise every new setter choice and exact restoration, including both
    // selectors of the shared boom-arm command and the full 16-bit button mask.
    for (const auto &pair : DerivedSettings::Pairs) {
        if (!pair.write || pair.feature == Feature::Sidetone || pair.feature == Feature::AncMode ||
            pair.feature == Feature::AdaptiveAnc || pair.feature == Feature::AncLevel)
            continue;
        auto headset = std::make_shared<FakeSettingsDevice>();
        const auto original = headset->values;
        FakeSettingsTransport wire(headset);
        Session operation(wire);
        auto result = operation.apply({pair.feature, pair.value, original});
        auto expected = original;
        expected.set(pair.feature, pair.value);
        check(result && *result == expected, "New setting changed with all other fields preserved");
        auto restored = operation.apply({pair.feature, original.get(pair.feature), *result});
        check(restored && *restored == original, "New setting restores exact full snapshot");
    }
    check(!EposProtocol::write(Feature::ButtonStatus, 1) &&
              !EposProtocol::write(Feature::ButtonStatus, 65535),
          "Do not approximate the button mask");
    check(!EposProtocol::write(Feature::Busylight, 3) &&
              !EposProtocol::write(Feature::NoPromptsDuringCalls, 2),
          "Reject unknown preference values");
    check(!EposProtocol::write(Feature::AudioLimiter, 3) && !EposProtocol::write(Feature::AnswerOnWear, 2) &&
              !EposProtocol::write(Feature::MuteOnRemoval, 2) && !EposProtocol::write(Feature::SmartPause, 2),
          "Proximity query mode is never a writable choice");

    for (const bool limiter : {false, true}) {
        auto headset = std::make_shared<FakeSettingsDevice>();
        headset->limiterErrorStatus = limiter;
        headset->wrongProximitySubcommand = !limiter;
        FakeSettingsTransport wire(headset);
        Session operation(wire);
        const auto result = operation.readAll();
        check(!result && result.error().code == ErrorCode::InvalidReport,
              "Reject nonzero limiter status and unrelated proximity subcommands");
        const auto sent = headset->sends;
        check(!operation.readAll() && headset->sends == sent, "Malformed semantic reply retires session");
    }
    for (const auto feature :
         {Feature::AudioLimiter, Feature::AnswerOnWear, Feature::Prompts, Feature::PromptLanguage}) {
        auto headset = std::make_shared<FakeSettingsDevice>();
        headset->badWriteReply = true;
        FakeSettingsTransport wire(headset);
        Session operation(wire);
        const auto result = operation.apply(
            {feature, feature == Feature::Prompts ? std::uint16_t{2} : std::uint16_t{1}, headset->values});
        check(!result && result.error().writeMayHaveApplied && headset->writes == 1,
              "Malformed setter reply reports uncertain outcome without retry");
        const auto sent = headset->sends;
        check(!operation.readAll() && headset->sends == sent, "Malformed write reply retires session");
    }
    for (const bool neighbor : {false, true}) {
        auto headset = std::make_shared<FakeSettingsDevice>();
        headset->ignoreWrite = !neighbor;
        headset->neighborSideEffect = neighbor;
        FakeSettingsTransport wire(headset);
        Session operation(wire);
        const auto result = operation.apply({Feature::AnswerOnWear, 1, headset->values});
        check(!result && result.error().code == ErrorCode::ReadbackMismatch &&
                  result.error().writeMayHaveApplied && headset->writes == 1,
              "Proximity acknowledgement is not success: detect ignored writes and neighboring changes");
    }
    // Preserve unknown native values and reject truncated/extra semantic fields.
    auto limiterReply =
        *EposProtocol::decode(CapturedSettings::report(EnumeratedSettings::Pairs[0].response));
    limiterReply.extra = 513;
    check(EposProtocol::readValue(Feature::AudioLimiter, limiterReply) == 513,
          "Unknown limiter value remains lossless");
    limiterReply.extra.reset();
    check(!EposProtocol::readValue(Feature::AudioLimiter, limiterReply), "Limiter value is required");
    auto wearReply = *EposProtocol::decode(CapturedSettings::report(EnumeratedSettings::Pairs[1].response));
    wearReply.trailing = 0;
    check(!EposProtocol::readValue(Feature::AnswerOnWear, wearReply) &&
              !EposProtocol::validWriteReply(Feature::AnswerOnWear, wearReply),
          "Do not silently discard extra reply words");

    for (int failure = 0; failure < 7; ++failure) {
        auto headset = std::make_shared<FakeSettingsDevice>();
        headset->failReset = failure == 1;
        headset->dropResetReply = failure == 2;
        headset->disconnectAfterReset = failure == 3;
        headset->malformedResetReply = failure == 4;
        auto expected = headset->values;
        if (failure == 5)
            expected.sidetone = 84;
        std::stop_source stop;
        if (failure == 6)
            stop.request_stop();
        FakeSettingsTransport wire(headset);
        Session operation(wire, stop.get_token());
        const auto result = operation.factoryReset(expected);
        if (failure == 0) {
            check(result && *result == headset->valuesAfterReset && headset->resets == 1 &&
                      headset->sends == int(2 * Features.size() + 1),
                  "Reset preflights and reads actual resulting values, without inventing defaults");
        } else if (failure < 5) {
            check(!result && result.error().writeMayHaveApplied && headset->resets == 1,
                  "Reset output/reply/reconnect failures are uncertain, with no automatic repeat");
            const auto sent = headset->sends;
            check(!operation.factoryReset(expected) && headset->sends == sent && headset->resets == 1,
                  "An uncertain reset session cannot be reused");
        } else {
            check(!result && !result.error().writeMayHaveApplied && headset->resets == 0,
                  "Stale preflight and cancellation prevent reset output");
        }
    }
    for (int failure = 0; failure < 3; ++failure) {
        auto faulty = std::make_shared<FakeSettingsDevice>();
        faulty->ignoreWrite = failure == 0;
        faulty->sideEffect = failure == 1;
        faulty->failWrite = failure == 2;
        FakeSettingsTransport wire(faulty);
        Session operation(wire);
        const auto result = operation.apply({Feature::AncLevel, 1, faulty->values});
        check(!result && result.error().writeMayHaveApplied, "Ambiguous output/readback failure is reported");
        check(faulty->writes == 1, "No automatic output retry or blind rollback");
    }
    for (bool corrupt : {false, true}) {
        auto faulty = std::make_shared<FakeSettingsDevice>();
        faulty->timeout = !corrupt;
        faulty->corruptReply = corrupt;
        FakeSettingsTransport wire(faulty);
        Session operation(wire);
        check(!operation.readAll(), "Timeout/corruption fail the operation");
        check(!operation.readAll() && faulty->sends == 1,
              "Failed session cannot consume a late reply on retry");
    }
    std::stop_source stop;
    stop.request_stop();
    FakeSettingsTransport cancelled(device);
    Session cancelledSession(cancelled, stop.get_token());
    auto result = cancelledSession.readAll();
    check(!result && result.error().code == ErrorCode::Cancelled, "Cancellation before output");
    std::puts("Capture fixtures, codec rejection, readback, dependencies and failure paths passed");
}
