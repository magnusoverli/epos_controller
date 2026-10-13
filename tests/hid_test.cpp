#include "HidDescriptorPresentation.h"
#include "TestFixtures.h"
#include <QTest>

class HidTest : public QObject {
    Q_OBJECT
  private slots:
    void layoutsAncestryAndPermissions() {
        Fixture fixture;
        const auto usb = fixture.addDevice();
        fixture.addHid(usb);
        const auto unrelated = fixture.addDevice("1-10");
        fixture.addHid(unrelated, "hidraw1");
        HidInspection inspector;
        const auto entries = HidInspection::enumerate(fixture.paths.hid);
        QVERIFY(entries);
        auto interfaces = inspector.scan(usb, "1-1#1", *entries, fixture.paths.devices, false);
        QCOMPARE(interfaces.size(), 1);
        const auto &info = interfaces.first();
        QVERIFY(info.layout);
        QCOMPARE(info.layout->reports.size(), 6U);
        QCOMPARE(info.interfaceNumber, QString("04"));
        QVERIFY(!info.canRead);
        QVERIFY(!info.canWrite);
        // Permissions remain live even when immutable layouts are cached.
        writeFixture(fixture.paths.devices + "/hidraw0", {});
        interfaces = inspector.scan(usb, "1-1#1", *entries, fixture.paths.devices, false);
        QVERIFY(interfaces.first().canRead);
        QVERIFY(interfaces.first().canWrite);
    }

    void failuresAndCacheInvalidation() {
        Fixture fixture;
        const auto usb = fixture.addDevice();
        const auto descriptor = fixture.addHid(usb);
        HidInspection inspector;
        const auto entries = HidInspection::enumerate(fixture.paths.hid);
        QVERIFY(entries);
        auto scan = [&](bool force = false) {
            return inspector.scan(usb, "generation", *entries, fixture.paths.devices, force).first();
        };
        QVERIFY(scan().layout);
        writeFixture(descriptor, QByteArray::fromHex("26ff"));
        auto bad = scan();
        QVERIFY(!bad.layout);
        QCOMPARE(bad.layout.error().code, HidDescriptor::ErrorCode::TruncatedItem);
        const auto json = HidDescriptorPresentation::toVariant(bad.layout, 2);
        QCOMPARE(json["errorCode"].toString(), QString("truncatedItem"));
        QCOMPARE(json["errorOffset"].toULongLong(), 0ULL);
        writeFixture(descriptor, QByteArray(8193, '\0'));
        bad = scan(true);
        QVERIFY(bad.descriptorTruncated);
        QCOMPARE(bad.descriptorBytes, 8193);
        QCOMPARE(bad.issues.first().code, ReadError::TooLarge);
        QVERIFY(QFile::remove(descriptor));
        bad = scan(true);
        QCOMPARE(bad.issues.first().code, ReadError::Missing);
        writeFixture(descriptor, QByteArray::fromHex(CapturedDescriptorHex));
        QVERIFY(scan().layout); // A failed read is not cached permanently.
        inspector.prune({});
        QVERIFY(scan().layout);
    }
};

QTEST_GUILESS_MAIN(HidTest)
#include "hid_test.moc"
