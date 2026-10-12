#include "DiagnosticsController.h"
#include "Diagnostics.h"

#include <QClipboard>
#include <QGuiApplication>

DiagnosticsController::DiagnosticsController(DeviceDiscovery &discovery, QObject *parent)
    : QObject(parent), m_discovery(discovery) {}

QString DiagnosticsController::refresh() {
    m_discovery.refresh();
    return Diagnostics::toJson(m_discovery.devices());
}

void DiagnosticsController::copy(const QString &snapshot) {
    QGuiApplication::clipboard()->setText(snapshot);
}
