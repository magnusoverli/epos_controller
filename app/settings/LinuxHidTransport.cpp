#include "LinuxHidTransport.h"
#include "SysfsReader.h"
#include <QFile>
#include <QFileInfo>
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <linux/hidraw.h>
#include <linux/input.h>
#include <poll.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>
#include <utility>

// Build with older userspace headers too. A kernel lacking this UAPI returns ENOTTY;
// never substitute a feature report or detach the kernel driver as a fallback.
#ifndef HIDIOCSOUTPUT
#define HIDIOCSOUTPUT(len) _IOC(_IOC_WRITE | _IOC_READ, 'H', 0x0B, len)
#endif

namespace Settings {
namespace {
const HidInterface *settingsInterface(const DeviceSnapshot &device, bool update = false) {
    if (device.usbId != "1395:018e")
        return nullptr;
    const HidInterface *selected = nullptr;
    for (const auto &interface : device.hidInterfaces) {
        if (interface.interfaceNumber != (update ? "04" : "03") || !interface.layout ||
            interface.descriptorTruncated)
            continue;
        const auto &layout = *interface.layout;
        const auto has = [&](HidDescriptor::ReportKind kind, std::uint8_t id, std::uint32_t size) {
            return std::ranges::find(layout.reports, HidDescriptor::Report{kind, id, size}) !=
                   layout.reports.end();
        };
        if (!has(HidDescriptor::ReportKind::Input, update ? 6 : 1, update ? 12 : 106) ||
            !has(HidDescriptor::ReportKind::Output, update ? 5 : 1, update ? 254 : 106) ||
            (update && !has(HidDescriptor::ReportKind::Feature, 3, 3)) ||
            std::ranges::find(layout.usagePages, update ? 0xff00U : 0xffffU) == layout.usagePages.end())
            continue;
        if (selected)
            return nullptr; // Ambiguous selection must not fall back to the first node.
        selected = &interface;
    }
    return selected;
}
Error systemError(const char *operation, int code) {
    const auto kind = code == EACCES || code == EPERM                     ? ErrorCode::AccessDenied
                      : code == ENODEV || code == ENOENT || code == EPIPE ? ErrorCode::Disconnected
                      : code == EBUSY || code == EWOULDBLOCK              ? ErrorCode::Busy
                                                                          : ErrorCode::Io;
    return {kind, std::string(operation) + ": " + std::strerror(code)};
}

class LinuxHidTransport final : public ReportTransport {
  public:
    explicit LinuxHidTransport(int fd) : m_fd(fd) {}
    ~LinuxHidTransport() override {
        if (m_fd >= 0)
            ::close(m_fd);
    }
    int releaseFd() { return std::exchange(m_fd, -1); }
    Result<void> feature(std::span<const std::uint8_t> report, std::stop_token stop) override {
        if (stop.stop_requested())
            return std::unexpected(Error{ErrorCode::Cancelled, "HID operation cancelled."});
        if (report.size() != 4 || report[0] != 3)
            return std::unexpected(Error{ErrorCode::InvalidReport, "Invalid update feature report."});
        const auto written = ::ioctl(m_fd, HIDIOCSFEATURE(report.size()), report.data());
        if (written < 0)
            return std::unexpected(systemError("HID feature SET_REPORT", errno));
        if (written != static_cast<int>(report.size()))
            return std::unexpected(Error{ErrorCode::Io, "Incomplete HID feature report."});
        return {};
    }
    Result<void> send(std::span<const std::uint8_t> report, std::stop_token stop) override {
        if (stop.stop_requested())
            return std::unexpected(Error{ErrorCode::Cancelled, "Settings operation cancelled."});
        if (report.empty() || report.size() > HID_MAX_DESCRIPTOR_SIZE)
            return std::unexpected(Error{ErrorCode::InvalidReport, "Invalid HID output length."});
        // Force the observed EP0 HID SET_REPORT(Output), not interrupt-OUT or a feature report.
        // Linux owns the synchronous control-transfer timeout; O_NONBLOCK does not bound ioctl.
        // This runs only on the worker. Never retry an interrupted/failed output request.
        const auto written = ::ioctl(m_fd, HIDIOCSOUTPUT(report.size()), report.data());
        if (written < 0)
            return std::unexpected(systemError("HID SET_REPORT", errno));
        if (written != static_cast<int>(report.size()))
            return std::unexpected(Error{ErrorCode::Io, "Incomplete HID output report."});
        return {};
    }
    Result<std::vector<std::uint8_t>> receive(Deadline deadline, std::stop_token stop) override {
        while (std::chrono::steady_clock::now() < deadline) {
            if (stop.stop_requested())
                return std::unexpected(Error{ErrorCode::Cancelled, "Settings operation cancelled."});
            const auto left =
                std::chrono::ceil<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now());
            pollfd descriptor{m_fd, POLLIN, 0};
            const int ready = ::poll(&descriptor, 1, static_cast<int>(std::clamp(left.count(), 1L, 25L)));
            if (ready < 0) {
                if (errno == EINTR)
                    continue;
                return std::unexpected(systemError("Poll HID", errno));
            }
            if (descriptor.revents & (POLLERR | POLLHUP | POLLNVAL))
                return std::unexpected(
                    Error{ErrorCode::Disconnected, "Headset disconnected during settings operation."});
            if (!(descriptor.revents & POLLIN))
                continue;
            std::array<std::uint8_t, 4096> bytes{};
            const auto size = ::read(m_fd, bytes.data(), bytes.size());
            if (size < 0) {
                if (errno == EAGAIN || errno == EINTR)
                    continue;
                return std::unexpected(systemError("Read HID", errno));
            }
            if (size == 0)
                return std::unexpected(Error{ErrorCode::Disconnected, "Headset closed the HID stream."});
            return std::vector<std::uint8_t>(bytes.begin(), bytes.begin() + size);
        }
        return std::unexpected(Error{ErrorCode::Timeout, "Timed out waiting for a headset reply."});
    }

  private:
    int m_fd;
};
Result<int> openValidatedNode(const DeviceSnapshot &device, bool readOnly, bool update = false) {
    const auto *interface = settingsInterface(device, update);
    if (!interface)
        return std::unexpected(
            Error{ErrorCode::UnsupportedDevice, "No supported IMPACT 860T ANC settings interface."});
    if (interface->sysfsPath.isEmpty() || interface->sysfsIdentity.isEmpty() ||
        SysfsReader::identity(interface->sysfsPath, false) != interface->sysfsIdentity)
        return std::unexpected(Error{ErrorCode::Disconnected, "Device identity changed. Refresh discovery."});
    const int fd = ::open(QFile::encodeName(interface->node).constData(),
                          (readOnly ? O_RDONLY : O_RDWR) | O_NONBLOCK | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0)
        return std::unexpected(systemError("Open settings HID interface", errno));
    auto transport = std::make_unique<LinuxHidTransport>(fd);
    struct stat stat{};
    if (::fstat(fd, &stat) != 0)
        return std::unexpected(systemError("Inspect HID handle", errno));
    const auto path =
        QStringLiteral("/sys/dev/char/%1:%2/device").arg(major(stat.st_rdev)).arg(minor(stat.st_rdev));
    if (!S_ISCHR(stat.st_mode) || QFileInfo(path).canonicalFilePath() != interface->sysfsPath ||
        SysfsReader::identity(path, false) != interface->sysfsIdentity)
        return std::unexpected(
            Error{ErrorCode::Disconnected, "Opened HID node does not match the selected connection."});
    if (!readOnly && ::flock(fd, LOCK_EX | LOCK_NB) != 0)
        return std::unexpected(systemError("Lock settings interface", errno));
    hidraw_devinfo info{};
    if (::ioctl(fd, HIDIOCGRAWINFO, &info) < 0)
        return std::unexpected(systemError("Read HID identity", errno));
    if (info.bustype != BUS_USB || static_cast<std::uint16_t>(info.vendor) != 0x1395 ||
        static_cast<std::uint16_t>(info.product) != 0x018e)
        return std::unexpected(Error{ErrorCode::UnsupportedDevice, "Unexpected device behind HID handle."});
    // Recheck layout on the opened handle, rather than trusting a cached descriptor alone.
    hidraw_report_descriptor descriptor{};
    int size = 0;
    if (::ioctl(fd, HIDIOCGRDESCSIZE, &size) < 0)
        return std::unexpected(systemError("Read HID descriptor size", errno));
    if (size <= 0 || size > HID_MAX_DESCRIPTOR_SIZE)
        return std::unexpected(Error{ErrorCode::UnsupportedDevice, "Unexpected HID descriptor size."});
    descriptor.size = static_cast<__u32>(size);
    if (::ioctl(fd, HIDIOCGRDESC, &descriptor) < 0)
        return std::unexpected(systemError("Read HID descriptor", errno));
    const auto layout = HidDescriptor::parse(std::as_bytes(std::span(descriptor.value, descriptor.size)));
    if (!layout || *layout != *interface->layout)
        return std::unexpected(
            Error{ErrorCode::UnsupportedDevice, "HID descriptor changed since discovery."});
    return transport->releaseFd();
}
} // namespace

bool supportsSettings(const DeviceSnapshot &device) {
    return settingsInterface(device) != nullptr;
}

Result<int> openHidEventInput(const DeviceSnapshot &device) {
    return openValidatedNode(device, true);
}

Result<std::unique_ptr<ReportTransport>> openHidTransport(const DeviceSnapshot &device,
                                                          std::stop_token stop) {
    if (stop.stop_requested())
        return std::unexpected(Error{ErrorCode::Cancelled, "Settings operation cancelled."});
    auto fd = openValidatedNode(device, false);
    if (!fd)
        return std::unexpected(fd.error());
    auto transport = std::make_unique<LinuxHidTransport>(*fd);
    // Discard pending reports, require a quiet window, and refuse an active competing stream.
    // This is not a guarantee against other software that ignores the advisory lock.
    const auto drainLimit = std::chrono::steady_clock::now() + std::chrono::milliseconds{500};
    for (int count = 0; count < 128 && std::chrono::steady_clock::now() < drainLimit; ++count) {
        auto pending = transport->receive(
            std::min(drainLimit, std::chrono::steady_clock::now() + std::chrono::milliseconds{75}), stop);
        if (!pending) {
            if (pending.error().code == ErrorCode::Timeout)
                return std::unique_ptr<ReportTransport>(std::move(transport));
            return std::unexpected(pending.error());
        }
    }
    return std::unexpected(
        Error{ErrorCode::Busy,
              "Settings interface is receiving continuous traffic. Close other headset controllers."});
}
Result<std::unique_ptr<ReportTransport>> openUpdateHidTransport(const DeviceSnapshot &device,
                                                                std::stop_token stop) {
    if (stop.stop_requested())
        return std::unexpected(Error{ErrorCode::Cancelled, "Update cancelled before opening HID."});
    auto fd = openValidatedNode(device, false, true);
    if (!fd)
        return std::unexpected(fd.error());
    auto transport = std::make_unique<LinuxHidTransport>(*fd);
    // Do not drain a live update session or steal it from another controller.
    auto pending = transport->receive(std::chrono::steady_clock::now() + std::chrono::milliseconds{75}, stop);
    if (pending || pending.error().code != ErrorCode::Timeout)
        return std::unexpected(Error{
            ErrorCode::Busy, "Update interface is active or unavailable. Close other headset controllers."});
    return std::unique_ptr<ReportTransport>(std::move(transport));
}
} // namespace Settings
