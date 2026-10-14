#pragma once

#include "HidDescriptor.h"
#include <QDateTime>
#include <QString>
#include <QVariantList>
#include <QVector>

enum class ScanStatus { Ready, Partial, Unavailable };
enum class ReadError { Missing, Empty, PermissionDenied, ReadFailed, TooLarge, EnumerationFailed };

struct ScanIssue {
    ReadError code;
    QString path;
    QString message;
    bool operator==(const ScanIssue &) const = default;
};

struct HidInterface {
    QString node;
    QString sysfsPath;
    QString sysfsIdentity;
    QString interfaceNumber;
    HidDescriptor::Result layout =
        std::unexpected(HidDescriptor::ParseError{HidDescriptor::ErrorCode::Empty, 0});
    qsizetype descriptorBytes = 0;
    bool descriptorTruncated = false;
    bool canRead = false;
    bool canWrite = false;
    QVector<ScanIssue> issues;
    bool operator==(const HidInterface &) const = default;
};

struct DeviceSnapshot {
    QString key;
    QString connectionId; // Process-local connection generation, never a persistent hardware identity.
    QString name;
    QString usbId;
    QString usbRevision;
    QVector<HidInterface> hidInterfaces;
    QVector<ScanIssue> issues;
    bool operator==(const DeviceSnapshot &) const = default;
};

struct ScanResult {
    ScanStatus status = ScanStatus::Ready;
    QVector<DeviceSnapshot> devices;
    QVector<ScanIssue> issues;
    QDateTime capturedAt = QDateTime::currentDateTimeUtc();
    bool stale = false;
};

[[nodiscard]] QString statusName(ScanStatus status);
[[nodiscard]] QString errorName(ReadError code);
[[nodiscard]] QVariantList issueVariants(const QVector<ScanIssue> &issues);
[[nodiscard]] QVariantList deviceVariants(const QVector<DeviceSnapshot> &devices);
