#pragma once

#include "DeviceDiscovery.h"
#include <QObject>

// UI use cases live here, not in the hardware scanner or discovery state.
class DiagnosticsController final : public QObject {
    Q_OBJECT
public:
    explicit DiagnosticsController(DeviceDiscovery &discovery, QObject *parent = nullptr);
    Q_INVOKABLE QString refresh();
    Q_INVOKABLE void copy(const QString &snapshot);

private:
    DeviceDiscovery &m_discovery;
};
