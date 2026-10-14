#pragma once
#include "FirmwareVersion.h"
#include <QByteArray>
#include <QJsonObject>
#include <QMap>
#include <QString>
#include <QUrl>
#include <expected>

namespace Firmware {
struct Release;
inline constexpr qsizetype PackageSizeLimit = 64 * 1024 * 1024;
template <typename T> using Result = std::expected<T, QString>;
struct Offer {
    Version version;
    QUrl url;
    QByteArray archiveHash;
    QMap<QString, QByteArray> members;
};
struct PreparedUpdate {
    QByteArray image;
    QString releaseNotes;
    Version version;
};
// Newer releases for this model use the same application-update protocol.
// Package integrity comes from the current official production catalog.
bool supportedUpgrade(Version installed, const Release &release);
Result<PreparedUpdate> prepareUpdate(const QByteArray &zip, const Release &release, Version installed);
// Parses one exported catalog offer, not a live endpoint response or credentials.
Result<Offer> parseOffer(const QByteArray &json);
bool isPackageUrl(const QUrl &url);
// Offline only: no USB, network, extraction to disk, or firmware session commands.
// Hash agreement is relative to the supplied offer, not publisher-signature verification.
Result<QJsonObject> inspectPackage(const QByteArray &zip, const Offer &offer,
                                   std::optional<Version> installed = {});
Result<QJsonObject> inspectFiles(const QString &zipPath, const QString &offerPath,
                                 std::optional<Version> installed = {});
} // namespace Firmware
