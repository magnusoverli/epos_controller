#include "RuleInstaller.h"
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace DeviceAccess {
std::expected<void, std::string> installRule(int directory, std::string_view contents) {
    struct stat existing{};
    if (::fstatat(directory, RuleName, &existing, AT_SYMLINK_NOFOLLOW) == 0) {
        if (!S_ISREG(existing.st_mode))
            return std::unexpected("The rule destination is not a regular file.");
    } else if (errno != ENOENT) {
        return std::unexpected(std::string("Inspect rule destination: ") + std::strerror(errno));
    }
    std::string temporary;
    int fd = -1;
    for (int attempt = 0; attempt < 100; ++attempt) {
        temporary = ".headset-controller-" + std::to_string(::getpid()) + "-" + std::to_string(attempt);
        fd = ::openat(directory, temporary.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC,
                      0600);
        if (fd >= 0 || errno != EEXIST)
            break;
    }
    if (fd < 0)
        return std::unexpected(std::string("Create rule: ") + std::strerror(errno));
    auto fail = [&](const char *operation) -> std::expected<void, std::string> {
        const auto message = std::string(operation) + ": " + std::strerror(errno);
        ::close(fd);
        ::unlinkat(directory, temporary.c_str(), 0);
        return std::unexpected(message);
    };
    std::size_t written = 0;
    while (written < contents.size()) {
        const auto count = ::write(fd, contents.data() + written, contents.size() - written);
        if (count < 0 && errno == EINTR)
            continue;
        if (count <= 0) {
            if (count == 0)
                errno = EIO;
            return fail("Write rule");
        }
        written += static_cast<std::size_t>(count);
    }
    if (::fchmod(fd, 0644) != 0 || ::fsync(fd) != 0)
        return fail("Finalize rule");
    if (::renameat(directory, temporary.c_str(), directory, RuleName) != 0)
        return fail("Install rule");
    ::close(fd);
    if (::fsync(directory) != 0)
        return std::unexpected(std::string("Rule installed, but directory sync failed: ") +
                               std::strerror(errno));
    return {};
}
} // namespace DeviceAccess
