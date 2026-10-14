#pragma once
#include "DeviceDiscovery.h"
#include "SettingsController.h"
#include <QProcess>
#include <QTimer>

// Unprivileged orchestration. Only the fixed helper installs the OS-level rule.
class DeviceAccessSetup final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool canRetry READ canRetry NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)
  public:
    struct Options {
        bool automatic = true;
        QString program{};
        QStringList arguments{};
        QString stateFile{};
        int settleMilliseconds = 2500;
    };
    DeviceAccessSetup(DeviceDiscovery &discovery, SettingsController &settings, Options options,
                      QObject *parent = nullptr);
    ~DeviceAccessSetup() override;
    bool busy() const { return m_busy; }
    bool canRetry() const { return m_attempted && !m_busy && m_settings.needsAccess(); }
    QString message() const { return m_message; }
    Q_INVOKABLE void enableAccess();
  signals:
    void changed();
    void installed();

  private:
    void maybeStartAutomatically();
    void start(bool automatic);
    void finish(int code, QProcess::ExitStatus status);
    void collectOutput();
    DeviceDiscovery &m_discovery;
    SettingsController &m_settings;
    Options m_options;
    QProcess m_process;
    QTimer m_settle;
    QString m_message;
    QByteArray m_output;
    bool m_attempted = false;
    bool m_busy = false;
    bool m_refreshAfterScan = false;
};
