#pragma once
#include "DeviceSnapshot.h"
#include <QHash>

// One enumeration for all USB devices; cached layouts are scoped to connection identity.
class HidInspection {
  public:
    struct Entry {
        QString name;
        QString path;
    };
    [[nodiscard]] static std::expected<QVector<Entry>, ScanIssue> enumerate(const QString &hidRoot);
    [[nodiscard]] QVector<HidInterface> scan(const QString &usbPath, const QString &connectionId,
                                             const QVector<Entry> &entries, const QString &devRoot,
                                             bool force);
    void prune(const QStringList &connections);

  private:
    struct Cached {
        QString connection;
        QString identity;
        HidInterface info;
    };
    QHash<QString, Cached> m_cache;
};
