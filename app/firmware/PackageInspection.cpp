#include "PackageInspection.h"
#include "ReleaseCatalog.h"
#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSet>
#include <QXmlStreamReader>
#include <QtEndian>
#include <archive.h>
#include <archive_entry.h>
#include <array>
#include <fcntl.h>
#include <memory>
#include <sys/stat.h>
#include <unistd.h>

namespace Firmware {
namespace {
constexpr qsizetype ImageLimit = 32 * 1024 * 1024;
constexpr qsizetype TextLimit = 64 * 1024;
QByteArray sha256(const QByteArray &bytes) {
    return QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex();
}
bool safeName(const QString &name) {
    static const QRegularExpression pattern(QStringLiteral("\\A[A-Za-z0-9_][A-Za-z0-9_.-]{0,127}\\z"));
    return pattern.match(name).hasMatch();
}
QByteArray hashValue(const QJsonValue &value) {
    static const QRegularExpression pattern(QStringLiteral("\\A[0-9a-fA-F]{64}\\z"));
    const auto text = value.toString();
    return pattern.match(text).hasMatch() ? text.toLatin1().toLower() : QByteArray();
}
Result<QByteArray> readFile(const QString &path, qsizetype limit) {
    const int fd = ::open(QFile::encodeName(path).constData(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0)
        return std::unexpected(QStringLiteral("Cannot open file: %1").arg(path));
    struct stat info{};
    if (::fstat(fd, &info) != 0 || !S_ISREG(info.st_mode) || info.st_size <= 0 || info.st_size > limit) {
        ::close(fd);
        return std::unexpected(QStringLiteral("Expected a bounded, nonempty regular file: %1").arg(path));
    }
    QFile file;
    if (!file.open(fd, QIODevice::ReadOnly, QFileDevice::AutoCloseHandle)) {
        ::close(fd);
        return std::unexpected(QStringLiteral("Cannot read file: %1").arg(path));
    }
    auto bytes = file.read(limit + 1);
    if (file.error() != QFileDevice::NoError || bytes.size() != info.st_size || bytes.size() > limit)
        return std::unexpected(QStringLiteral("File changed size or could not be read: %1").arg(path));
    return bytes;
}
Result<QMap<QString, QByteArray>> unpack(const QByteArray &zip, const Offer &offer) {
    const std::unique_ptr<archive, decltype(&archive_read_free)> reader(archive_read_new(),
                                                                        archive_read_free);
    if (!reader || archive_read_support_format_zip(reader.get()) != ARCHIVE_OK ||
        archive_read_open_memory(reader.get(), zip.constData(), static_cast<std::size_t>(zip.size())) !=
            ARCHIVE_OK)
        return std::unexpected(QStringLiteral("Cannot open firmware ZIP."));
    QMap<QString, QByteArray> contents;
    archive_entry *entry = nullptr;
    int status = 0;
    while ((status = archive_read_next_header(reader.get(), &entry)) == ARCHIVE_OK) {
        const auto *rawName = archive_entry_pathname_utf8(entry);
        const QString name = rawName ? QString::fromUtf8(rawName) : QString();
        if (!safeName(name) || (!offer.members.isEmpty() && !offer.members.contains(name)) ||
            contents.contains(name) || contents.size() >= 3 || archive_entry_filetype(entry) != AE_IFREG ||
            archive_entry_symlink(entry) || archive_entry_hardlink(entry) ||
            archive_entry_is_encrypted(entry))
            return std::unexpected(QStringLiteral("Unsupported, duplicate or unexpected ZIP entry."));
        const qsizetype limit = name.endsWith(QStringLiteral(".bin")) ? ImageLimit : TextLimit;
        if (!archive_entry_size_is_set(entry) || archive_entry_size(entry) <= 0 ||
            archive_entry_size(entry) > limit)
            return std::unexpected(QStringLiteral("ZIP member exceeds the inspection limit."));
        QByteArray bytes;
        std::array<char, 16384> block{};
        for (;;) {
            const auto count = archive_read_data(reader.get(), block.data(), block.size());
            if (count < 0)
                return std::unexpected(QStringLiteral("ZIP member decompression or CRC check failed."));
            if (count == 0)
                break;
            if (count > limit - bytes.size())
                return std::unexpected(QStringLiteral("ZIP member expands beyond the inspection limit."));
            bytes.append(block.data(), static_cast<qsizetype>(count));
        }
        if (bytes.size() != archive_entry_size(entry) ||
            (!offer.members.isEmpty() && sha256(bytes) != offer.members.value(name)))
            return std::unexpected(
                QStringLiteral("ZIP member size or SHA-256 does not match the supplied offer: %1").arg(name));
        contents.insert(name, std::move(bytes));
    }
    if (status != ARCHIVE_EOF || contents.size() != 3)
        return std::unexpected(QStringLiteral("Incomplete or malformed firmware ZIP."));
    return contents;
}
Result<QMap<QString, QString>> manifest(const QByteArray &xml) {
    const QSet<QString> allowed{QStringLiteral("SeComAppHSFWU"),
                                QStringLiteral("SeComAppHSFWU/Model"),
                                QStringLiteral("SeComAppHSFWU/FWVersion"),
                                QStringLiteral("SeComAppHSFWU/FWVersion/Version"),
                                QStringLiteral("SeComAppHSFWU/FWFiles"),
                                QStringLiteral("SeComAppHSFWU/FWFiles/FWFile"),
                                QStringLiteral("SeComAppHSFWU/ReleaseNote")};
    QXmlStreamReader reader(xml);
    QStringList path;
    QSet<QString> seen;
    QMap<QString, QString> fields;
    while (!reader.atEnd()) {
        const auto token = reader.readNext();
        if (token == QXmlStreamReader::DTD || token == QXmlStreamReader::EntityReference)
            return std::unexpected(QStringLiteral("Manifest DTDs and entity references are unsupported."));
        if (reader.isStartElement()) {
            path.append(reader.name().toString());
            const auto key = path.join('/');
            if (!allowed.contains(key) || seen.contains(key) || !reader.attributes().isEmpty() ||
                !reader.namespaceUri().isEmpty() || !reader.namespaceDeclarations().isEmpty())
                return std::unexpected(QStringLiteral("Unexpected or duplicate firmware manifest element."));
            seen.insert(key);
        } else if (reader.isEndElement()) {
            if (!path.isEmpty())
                path.removeLast();
        } else if (reader.isCharacters() && !reader.isWhitespace()) {
            if (path.size() < 2 || path.last() == QStringLiteral("FWVersion") ||
                path.last() == QStringLiteral("FWFiles"))
                return std::unexpected(QStringLiteral("Unexpected text in firmware manifest."));
            fields[path.last()] += reader.text().toString();
        }
    }
    if (reader.hasError() || seen != allowed || fields.size() != 4)
        return std::unexpected(QStringLiteral("Incomplete or invalid firmware manifest."));
    return fields;
}
Result<QJsonArray> imageRecords(const QByteArray &image) {
    QJsonArray records;
    QSet<quint32> partitions;
    qsizetype offset = 0;
    bool footer = false;
    while (offset < image.size()) {
        if (image.size() - offset < 12 || records.size() >= 130 || footer)
            return std::unexpected(QStringLiteral("Invalid firmware container bounds or trailing bytes."));
        const auto tag = image.mid(offset, 8);
        const auto length = qFromBigEndian<quint32>(image.constData() + offset + 8);
        if (length > static_cast<quint64>(image.size() - offset - 12))
            return std::unexpected(QStringLiteral("Truncated firmware container record."));
        QJsonObject record{
            {"tag", QString::fromLatin1(tag)}, {"offset", offset}, {"payloadBytes", qint64(length)}};
        if (records.isEmpty()) {
            if (tag != "APPUHDR5" || length != 36 || image.mid(offset + 12, 8) != "QCC518Xx")
                return std::unexpected(QStringLiteral("Unsupported firmware container header."));
        } else if (tag == "PARTDATA" && length >= 4) {
            const auto index = qFromBigEndian<quint32>(image.constData() + offset + 12);
            if (partitions.contains(index))
                return std::unexpected(QStringLiteral("Duplicate firmware partition index."));
            partitions.insert(index);
            record.insert("partitionIndex", qint64(index));
        } else if (tag == "APPUPFTR" && length == 256 && !partitions.isEmpty()) {
            footer = true;
        } else {
            return std::unexpected(QStringLiteral("Unsupported firmware container record."));
        }
        records.append(record);
        offset += 12 + static_cast<qsizetype>(length);
    }
    if (!footer)
        return std::unexpected(QStringLiteral("Firmware container footer is missing."));
    return records;
}
} // namespace

bool supportedUpgrade(Version installed, const Release &release) {
    static const QRegularExpression hashPattern(QStringLiteral("\\A[0-9a-fA-F]{64}\\z"));
    return release.version > installed && isPackageUrl(release.url) &&
           release.url.fileName() ==
               QStringLiteral("IMPACT_860T_ANC_%1.zip").arg(QString::fromStdString(release.version.text())) &&
           hashPattern.match(QString::fromLatin1(release.archiveHash)).hasMatch();
}
Result<PreparedUpdate> prepareUpdate(const QByteArray &zip, const Release &release, Version installed) {
    if (!supportedUpgrade(installed, release))
        return std::unexpected(
            QStringLiteral("Expected a newer official firmware release for this headset."));
    if (zip.isEmpty() || zip.size() > PackageSizeLimit || sha256(zip) != release.archiveHash.toLower())
        return std::unexpected(
            QStringLiteral("Downloaded firmware does not match the official catalog's package hash."));
    const Offer offer{release.version, release.url, release.archiveHash, {}};
    auto members = unpack(zip, offer);
    if (!members)
        return std::unexpected(members.error());
    auto fields = manifest(members->value("PackageInfo.xml"));
    if (!fields)
        return std::unexpected(fields.error());
    const auto version = parseVersion(fields->value("Version").toStdString());
    const auto imageName = fields->value("FWFile");
    const auto notesName = fields->value("ReleaseNote");
    if (fields->value("Model") != "IMPACT 860T ANC" || !version || *version != release.version ||
        !imageName.endsWith(".bin") || !notesName.endsWith(".txt") || !members->contains(imageName) ||
        !members->contains(notesName))
        return std::unexpected(QStringLiteral("Firmware manifest does not match the selected release."));
    const auto image = members->value(imageName);
    if (auto records = imageRecords(image); !records)
        return std::unexpected(records.error());
    return PreparedUpdate{image, QString::fromUtf8(members->value(notesName)), *version};
}

bool isPackageUrl(const QUrl &url) {
    const auto prefix = QStringLiteral("/deviceupdates/");
    const auto path = url.path();
    return url.isValid() && url.scheme() == "https" && url.host() == "update.eposstorage.com" &&
           path.startsWith(prefix) && safeName(path.mid(prefix.size())) && path.endsWith(".zip") &&
           url.userInfo().isEmpty() && !url.hasQuery() && !url.hasFragment() && url.port(443) == 443;
}

Result<Offer> parseOffer(const QByteArray &json) {
    if (json.size() > 256 * 1024)
        return std::unexpected(QStringLiteral("Firmware offer is too large."));
    const auto document = QJsonDocument::fromJson(json);
    const auto object = document.object();
    const auto version = parseVersion(object["Version"].toString().toStdString());
    if (!document.isObject() || object["Type"] != "Firmware" || object["IProduct"] != "IMPACT_860T_ANC" ||
        !version)
        return std::unexpected(
            QStringLiteral("Expected an IMPACT_860T_ANC firmware offer with a three-part version."));
    QJsonObject selected;
    for (const auto &item : object["UpdatesURLs"].toArray()) {
        const auto variant = item.toObject();
        if (variant["UpdateVariant"] == "FW" && variant["IsDefault"].isBool() &&
            variant["IsDefault"].toBool()) {
            if (!selected.isEmpty())
                return std::unexpected(QStringLiteral("Ambiguous default firmware variants."));
            selected = variant;
        }
    }
    const QUrl url(selected["UpdatesURL"].toString(), QUrl::StrictMode);
    const auto archiveHash = hashValue(selected["Hash"]);
    if (selected.isEmpty() || !isPackageUrl(url) || archiveHash.isEmpty() ||
        object["UpdatesURL"].toString() != url.toString())
        return std::unexpected(
            QStringLiteral("Unsupported package URL, missing SHA-256 or inconsistent offer."));
    Offer offer{*version, url, archiveHash, {}};
    QSet<QString> folded;
    for (const auto &item : selected["Files"].toArray()) {
        const auto member = item.toObject();
        const auto name = member["Name"].toString();
        const auto hash = hashValue(member["Hash"]);
        if (!safeName(name) || hash.isEmpty() || folded.contains(name.toCaseFolded()))
            return std::unexpected(QStringLiteral("Invalid or duplicate firmware member name/hash."));
        folded.insert(name.toCaseFolded());
        offer.members.insert(name, hash);
    }
    if (offer.members.size() != 3 || !offer.members.contains("PackageInfo.xml"))
        return std::unexpected(QStringLiteral("Expected a manifest, one image and release notes."));
    return offer;
}

Result<QJsonObject> inspectPackage(const QByteArray &zip, const Offer &offer,
                                   std::optional<Version> installed) {
    if (offer.members.size() != 3)
        return std::unexpected(QStringLiteral("Offline inspection requires all three member hashes."));
    if (zip.isEmpty() || zip.size() > PackageSizeLimit || sha256(zip) != offer.archiveHash)
        return std::unexpected(
            QStringLiteral("Firmware ZIP size or SHA-256 does not match the supplied offer."));
    auto members = unpack(zip, offer);
    if (!members)
        return std::unexpected(members.error());
    auto fields = manifest(members->value("PackageInfo.xml"));
    if (!fields)
        return std::unexpected(fields.error());
    const auto version = parseVersion(fields->value("Version").toStdString());
    const auto imageName = fields->value("FWFile");
    const auto notesName = fields->value("ReleaseNote");
    if (fields->value("Model") != "IMPACT 860T ANC" || !version || *version != offer.version ||
        !safeName(imageName) || !imageName.endsWith(".bin") || !safeName(notesName) ||
        !notesName.endsWith(".txt") || !members->contains(imageName) || !members->contains(notesName))
        return std::unexpected(
            QStringLiteral("Manifest model, version or members disagree with the supplied offer."));
    auto records = imageRecords(members->value(imageName));
    if (!records)
        return std::unexpected(records.error());
    QJsonObject hashes;
    for (auto it = members->cbegin(); it != members->cend(); ++it)
        hashes.insert(it.key(), QString::fromLatin1(sha256(it.value())));
    QJsonObject result{{"schemaVersion", 1},
                       {"source", "offline package inspection against supplied offer"},
                       {"model", "IMPACT 860T ANC"},
                       {"packageVersion", QString::fromStdString(version->text())},
                       {"packageUrl", offer.url.toString()},
                       {"packageSha256", QString::fromLatin1(offer.archiveHash)},
                       {"packageBytes", zip.size()},
                       {"memberSha256", hashes},
                       {"imageName", imageName},
                       {"imageBytes", members->value(imageName).size()},
                       {"imageRecords", *records},
                       {"releaseNotes", QString::fromUtf8(members->value(notesName))},
                       {"integrity", "ZIP and all member SHA-256 values match the supplied offer"},
                       {"publisherSignatureVerified", false},
                       {"deviceCompatibilityVerified", false},
                       {"liveCatalogChecked", false},
                       {"installationSupported", false}};
    if (installed) {
        result.insert("installedVersionProvided", QString::fromStdString(installed->text()));
        result.insert("versionComparison", *version > *installed    ? "newer"
                                           : *version == *installed ? "same"
                                                                    : "older");
    }
    return result;
}
Result<QJsonObject> inspectFiles(const QString &zipPath, const QString &offerPath,
                                 std::optional<Version> installed) {
    auto json = readFile(offerPath, 256 * 1024);
    if (!json)
        return std::unexpected(json.error());
    auto offer = parseOffer(*json);
    if (!offer)
        return std::unexpected(offer.error());
    auto zip = readFile(zipPath, PackageSizeLimit);
    if (!zip)
        return std::unexpected(zip.error());
    return inspectPackage(*zip, *offer, installed);
}
} // namespace Firmware
