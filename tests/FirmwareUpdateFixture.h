#pragma once
#include "firmware/ReleaseCatalog.h"
#include <QCryptographicHash>
#include <QtEndian>
#include <archive.h>
#include <archive_entry.h>
#include <array>
#include <memory>

namespace SyntheticUpdate {
inline QByteArray record(const QByteArray &tag, const QByteArray &body) {
    QByteArray bytes = tag;
    bytes.resize(12);
    qToBigEndian<quint32>(quint32(body.size()), bytes.data() + 8);
    return bytes + body;
}
inline QByteArray image() {
    QByteArray header("QCC518Xx");
    header.resize(36, '\0');
    return record("APPUHDR5", header) + record("PARTDATA", QByteArray(4, '\0') + "fictional future image") +
           record("APPUPFTR", QByteArray(256, '\0'));
}
// Generate fictional packages with independent versions, hashes and sizes.
inline QByteArray zip(Firmware::Version version, const QString &model = QStringLiteral("IMPACT 860T ANC"),
                      const QByteArray &firmware = image()) {
    const auto xml = (QStringLiteral("<SeComAppHSFWU><Model>%1</Model><FWVersion><Version>%2</Version>"
                                     "</FWVersion><FWFiles><FWFile>image.bin</FWFile></FWFiles>"
                                     "<ReleaseNote>notes.txt</ReleaseNote></SeComAppHSFWU>")
                          .arg(model, QString::fromStdString(version.text())))
                         .toUtf8();
    const std::array<std::pair<QByteArray, QByteArray>, 3> members{
        {{"PackageInfo.xml", xml},
         {"image.bin", firmware},
         {"notes.txt", "Synthetic future release; not firmware."}}};
    const std::unique_ptr<archive, decltype(&archive_write_free)> writer(archive_write_new(),
                                                                         archive_write_free);
    QByteArray output(65536, '\0');
    size_t used = 0;
    if (!writer || archive_write_set_format_zip(writer.get()) != ARCHIVE_OK ||
        archive_write_open_memory(writer.get(), output.data(), size_t(output.size()), &used) != ARCHIVE_OK)
        return {};
    for (const auto &[name, content] : members) {
        const std::unique_ptr<archive_entry, decltype(&archive_entry_free)> entry(archive_entry_new(),
                                                                                  archive_entry_free);
        if (!entry)
            return {};
        archive_entry_set_pathname(entry.get(), name.constData());
        archive_entry_set_filetype(entry.get(), AE_IFREG);
        archive_entry_set_perm(entry.get(), 0600);
        archive_entry_set_size(entry.get(), content.size());
        if (archive_write_header(writer.get(), entry.get()) != ARCHIVE_OK ||
            archive_write_data(writer.get(), content.constData(), size_t(content.size())) != content.size())
            return {};
    }
    if (archive_write_close(writer.get()) != ARCHIVE_OK)
        return {};
    output.resize(qsizetype(used));
    return output;
}
inline Firmware::Release release(Firmware::Version version, const QByteArray &zip) {
    return {version,
            QUrl(QStringLiteral("https://update.eposstorage.com/deviceupdates/IMPACT_860T_ANC_%1.zip")
                     .arg(QString::fromStdString(version.text()))),
            QCryptographicHash::hash(zip, QCryptographicHash::Sha256).toHex()};
}
} // namespace SyntheticUpdate
