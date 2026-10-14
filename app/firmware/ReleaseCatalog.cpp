#include "ReleaseCatalog.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>

namespace Firmware {
QUrl catalogUrl() {
    return QUrl(QStringLiteral("https://api.enterprise.eposaudio.com/api/Updates/AvailableFirmware/"
                               "IMPACT_860T_ANC?tenantId=1e89135c-7e56-4ad3-821c-e278427918a0"));
}
Result<Release> latestRelease(const QByteArray &json) {
    if (json.size() > 1024 * 1024)
        return std::unexpected(QStringLiteral("Release catalog exceeds the size limit."));
    const auto document = QJsonDocument::fromJson(json);
    if (!document.isArray() || document.array().size() > 1024)
        return std::unexpected(QStringLiteral("The server returned an invalid release catalog."));
    std::optional<Release> latest;
    QMap<QString, Release> seen;
    const QRegularExpression hashPattern(QStringLiteral("\\A[0-9a-fA-F]{64}\\z"));
    for (const auto &item : document.array()) {
        if (!item.isObject())
            return std::unexpected(QStringLiteral("Invalid catalog entry."));
        const auto row = item.toObject();
        if (!row["BuildType"].isString())
            return std::unexpected(QStringLiteral("Catalog entry is missing its release channel."));
        if (row["BuildType"] != "Production")
            continue;
        const auto version = parseVersion(row["Version"].toString().toStdString());
        const QUrl url(row["FileUrl"].toString(), QUrl::StrictMode);
        const auto hash = row["HashOfZipFile"].toString();
        if (!version || !isPackageUrl(url) || !hashPattern.match(hash).hasMatch() ||
            url.fileName() !=
                QStringLiteral("IMPACT_860T_ANC_%1.zip").arg(QString::fromStdString(version->text())))
            return std::unexpected(
                QStringLiteral("Production release has an invalid version, package URL or hash."));
        Release release{*version, url, hash.toLatin1().toLower()};
        const auto key = QString::fromStdString(version->text());
        if (seen.contains(key) && (seen[key].url != url || seen[key].archiveHash != release.archiveHash))
            return std::unexpected(QStringLiteral("Conflicting packages for the same firmware version."));
        seen.insert(key, release);
        if (!latest || release.version > latest->version)
            latest = release;
    }
    if (!latest)
        return std::unexpected(QStringLiteral("No production release was returned for this headset."));
    return *latest;
}
} // namespace Firmware
