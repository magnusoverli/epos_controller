#include "Diagnostics.h"
#include "BuildInfo.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

QByteArray Diagnostics::toJson(const ScanResult &snapshot) {
    return QJsonDocument(
               QJsonObject{{"schemaVersion", 1},
                           {"application", QString::fromLatin1(BuildInfo::Name) + ' ' + BuildInfo::Version},
                           {"qtRuntimeVersion", qVersion()},
                           {"source", "sysfs descriptors; no device commands"},
                           {"settingsTransport", "unverified"},
                           {"capturedAt", snapshot.capturedAt.toString(Qt::ISODateWithMs)},
                           {"scanStatus", statusName(snapshot.status)},
                           {"stale", snapshot.stale},
                           {"issues", QJsonArray::fromVariantList(issueVariants(snapshot.issues))},
                           {"devices", QJsonArray::fromVariantList(deviceVariants(snapshot.devices))}})
        .toJson(QJsonDocument::Indented);
}
