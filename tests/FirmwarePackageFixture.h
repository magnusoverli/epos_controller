#pragma once
#include <QByteArray>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace SyntheticFirmware {
// Generated once with Python zipfile. Fictional image and footer; no vendor bytes.
inline QByteArray zip() {
    return QByteArray::fromBase64(
        "UEsDBBQAAAAIAAAATl2hls4ySAAAAGABAAAJAAAAaW1hZ2UuYmlucwwICPVwCTJlYGBQCXR2NjW0iKhgwAMCHINCXBxD"
        "HIFMCRC/uDKvJCO1JDNZoSS1uEQhMzcxPdURaGaAW0gQAwMjPqNGBAAAUEsDBBQAAAAIAAAATl0GxyA5fQAAAL8AAAA"
        "PAAAAUGFja2FnZUluZm8ueG1sswlOdc7PdSwo8Ah2Cw+1s/HNT0nNsfP0DXB0DlGwMDMIUXD0c7bRhwjbuIWHpRYV"
        "Z+bn2dnAGIZ6pnpmhpY2+nAZfSRVbuFumTmpxTCGXWZuYnqqXlJmHkgVWATGAKoJSs1JTSxO9csvSbXLAxLFeiUVJTb"
        "6yMI2+qjuBQBQSwMEFAAAAAgAAABOXTFtuuFeAAAA1wMAAAkAAABub3Rlcy50eHTty7ENgCAQQNHeKW4AowsQeisL4wCA"
        "hxLxMOeR6PbiFDZUv3lfWT09JBtKcMAY0VwIlASv9gschvd8qt7qZvQ+BkLw4ZbMpYlhHsDQAi6RcIoRGcopgdau8sor"
        "/4u/UEsBAhQDFAAAAAgAAABOXaGWzjJIAAAAYAEAAAkAAAAAAAAAAAAAAKSBAAAAAGltYWdlLmJpblBLAQIUAxQAAAAIAAAA"
        "Tl0GxyA5fQAAAL8AAAAPAAAAAAAAAAAAAACkgW8AAABQYWNrYWdlSW5mby54bWxQSwECFAMUAAAACAAAAE5dMW264V4AAADX"
        "AwAACQAAAAAAAAAAAAAApIEZAQAAbm90ZXMudHh0UEsFBgAAAAADAAMAqwAAAJ4BAAAAAA==");
}
inline QJsonObject offer(const QString &version = QStringLiteral("1.5.619")) {
    const auto url = QStringLiteral("https://update.eposstorage.com/deviceupdates/synthetic-test.zip");
    const QJsonArray files{
        QJsonObject{{"Name", "image.bin"},
                    {"Hash", "89522a1436b090c0aee3097f16f1625706580f065bb75aab755fb5b297d2905e"}},
        QJsonObject{{"Name", "PackageInfo.xml"},
                    {"Hash", "075543ba6bdf0c3eafca440ccd68e97814d53fedb9e0e6baf5e8bf5981da1d6e"}},
        QJsonObject{{"Name", "notes.txt"},
                    {"Hash", "c080279d8c6134e41000e76ce7f8ebb6a53a9af492e185eb7d37faa7a06eaa1a"}}};
    return {{"Type", "Firmware"},
            {"IProduct", "IMPACT_860T_ANC"},
            {"Version", version},
            {"UpdatesURL", url},
            {"UpdatesURLs", QJsonArray{QJsonObject{
                                {"UpdateVariant", "FW"},
                                {"IsDefault", true},
                                {"UpdatesURL", url},
                                {"Hash", "dfb3f3fe7c8e63f80fd4ec78d761b1fc33e5e96773e743f1444bd521c9ff01db"},
                                {"Files", files}}}}};
}
inline bool write(const QString &path, const QByteArray &bytes) {
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.flush();
}
} // namespace SyntheticFirmware
