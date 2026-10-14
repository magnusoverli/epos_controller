#pragma once
#include "ApplicationUpdate.h"
#include <QJsonObject>
#include <QLockFile>

namespace Firmware {
// Durable, per-unit operation record. An incomplete/invalid record is never
// replaced by a fresh transfer, even after a process restart or USB-port change.
class UpdateJournal {
  public:
    UpdateJournal(QString directory, const QByteArray &unitHash);
    Result<void> acquire();
    Result<void> write(const QJsonObject &record);
    QString path() const { return m_path; }

  private:
    QString m_directory;
    QString m_path;
    QLockFile m_lock;
};
} // namespace Firmware
