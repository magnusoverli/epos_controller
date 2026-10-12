#include "Diagnostics.h"
#include "BuildInfo.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

QString Diagnostics::toJson(const QVariantList &devices) {
    return QString::fromUtf8(QJsonDocument(QJsonObject{
        {"schemaVersion", 1},
        {"application", QStringLiteral("Headset Controller ") + BuildInfo::Version},
        {"qtRuntimeVersion", qVersion()},
        {"source", "sysfs descriptors; no device commands"},
        {"settingsTransport", "unverified"},
        {"devices", QJsonArray::fromVariantList(devices)}
    }).toJson(QJsonDocument::Indented));
}
