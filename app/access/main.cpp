// Invoked through pkexec. No Qt, shell, caller-supplied paths, rule text or commands.
#include "RuleInstaller.h"
#include "UdevRule.h"
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <libudev.h>
#include <memory>
#include <string>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace {
bool run(const char *program, std::vector<std::string> arguments) {
    std::vector<char *> argv{const_cast<char *>(program)};
    for (auto &argument : arguments)
        argv.push_back(argument.data());
    argv.push_back(nullptr);
    const auto child = ::fork();
    if (child == -1)
        return false;
    if (child == 0) {
        ::execv(program, argv.data());
        ::_exit(127);
    }
    int status = 0;
    while (::waitpid(child, &status, 0) == -1) {
        if (errno != EINTR)
            return false;
    }
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

bool matches(udev_device *device) {
    auto *usb = udev_device_get_parent_with_subsystem_devtype(device, "usb", "usb_device");
    auto *interface = udev_device_get_parent_with_subsystem_devtype(device, "usb", "usb_interface");
    auto attr = [](udev_device *parent, const char *name, const char *expected) {
        const auto *value = parent ? udev_device_get_sysattr_value(parent, name) : nullptr;
        return value && std::strcmp(value, expected) == 0;
    };
    return attr(usb, "idVendor", "1395") && attr(usb, "idProduct", "018e") &&
           (attr(interface, "bInterfaceNumber", "03") || attr(interface, "bInterfaceNumber", "04"));
}
} // namespace

int main(int argc, char **argv) {
    if (argc == 2 && std::strcmp(argv[1], "--print-rule") == 0) {
        return std::fwrite(DeviceAccess::Rule.data(), 1, DeviceAccess::Rule.size(), stdout) ==
                       DeviceAccess::Rule.size()
                   ? 0
                   : 1;
    }
    if (argc != 1) {
        std::fputs("Usage: headset-access-helper [--print-rule]\n", stderr);
        return 1;
    }
    if (::geteuid() != 0) {
        std::fputs("Administrator authorization is required to install the headset access rule.\n", stderr);
        return 1;
    }
    const char *udevadm = nullptr;
    for (const auto *path : {"/usr/bin/udevadm", "/bin/udevadm", "/usr/sbin/udevadm", "/sbin/udevadm"}) {
        if (::access(path, X_OK) == 0) {
            udevadm = path;
            break;
        }
    }
    if (!udevadm) {
        std::fputs("udevadm is not installed.\n", stderr);
        return 1;
    }
    // /etc/udev is supplied by the OS. Create rules.d if necessary, without following symlinks.
    const int parent = ::open("/etc/udev", O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (parent < 0) {
        std::perror("Open /etc/udev");
        return 1;
    }
    struct stat info{};
    if (::fstat(parent, &info) != 0 || info.st_uid != 0 || (info.st_mode & 0022) != 0) {
        ::close(parent);
        std::fputs("/etc/udev must be root-owned and not group/world writable.\n", stderr);
        return 1;
    }
    if (::mkdirat(parent, "rules.d", 0755) != 0 && errno != EEXIST) {
        ::close(parent);
        std::perror("Create rules.d");
        return 1;
    }
    const int directory = ::openat(parent, "rules.d", O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    ::close(parent);
    if (directory < 0) {
        std::perror("Open rules.d");
        return 1;
    }
    if (::fstat(directory, &info) != 0 || info.st_uid != 0 || (info.st_mode & 0022) != 0) {
        ::close(directory);
        std::fputs("rules.d must be root-owned and not group/world writable.\n", stderr);
        return 1;
    }
    const auto installed = DeviceAccess::installRule(directory, DeviceAccess::Rule);
    ::close(directory);
    if (!installed) {
        std::fprintf(stderr, "%s\n", installed.error().c_str());
        return 1;
    }
    // Exit 3 means the persistent rule exists but immediate activation was incomplete.
    if (!run(udevadm, {"control", "--timeout=10", "--reload-rules"})) {
        std::fputs("Rule installed; udev could not reload it. Restart the system before retrying access.\n",
                   stderr);
        return 3;
    }
    const std::unique_ptr<udev, decltype(&udev_unref)> context(udev_new(), udev_unref);
    const std::unique_ptr<udev_enumerate, decltype(&udev_enumerate_unref)> devices(
        context ? udev_enumerate_new(context.get()) : nullptr, udev_enumerate_unref);
    if (!devices || udev_enumerate_add_match_subsystem(devices.get(), "hidraw") < 0 ||
        udev_enumerate_scan_devices(devices.get()) < 0) {
        std::fputs("Rule installed; reconnect the headset to activate access.\n", stderr);
        return 3;
    }
    bool activated = true;
    udev_list_entry *entry = nullptr;
    udev_list_entry_foreach(entry, udev_enumerate_get_list_entry(devices.get())) {
        const auto *path = udev_list_entry_get_name(entry);
        const std::unique_ptr<udev_device, decltype(&udev_device_unref)> device(
            udev_device_new_from_syspath(context.get(), path), udev_device_unref);
        if (device && matches(device.get()))
            activated = run(udevadm, {"trigger", "--action=change", "--", path}) && activated;
    }
    activated = run(udevadm, {"settle", "--timeout=10"}) && activated;
    if (!activated) {
        std::fputs("Rule installed; reconnect the headset to finish activating access.\n", stderr);
        return 3;
    }
    std::puts("Persistent headset access rule installed and activated.");
    return 0;
}
