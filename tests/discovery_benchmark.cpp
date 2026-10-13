#include "Diagnostics.h"
#include "TestFixtures.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <algorithm>

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    Fixture fixture;
    fixture.addHid(fixture.addDevice());
    SysfsDeviceScanner scanner(fixture.paths);
    const auto expected = scanner();
    for (const int unrelated : {0, 10, 50}) {
        for (int i = 0; i < unrelated; ++i)
            writeFixture(fixture.paths.usb + QString("/2-%1/idVendor").arg(i), "1234\n");
        std::vector<qint64> timings;
        for (int iteration = 0; iteration < 1000; ++iteration) {
            QElapsedTimer timer;
            timer.start();
            const auto result = scanner();
            check(result.devices == expected.devices, "stable benchmark snapshot");
            timings.push_back(timer.nsecsElapsed());
        }
        std::ranges::sort(timings);
        std::printf("unrelated=%d median_us=%.2f p95_us=%.2f\n", unrelated,
                    static_cast<double>(timings[500]) / 1000., static_cast<double>(timings[950]) / 1000.);
    }
}
