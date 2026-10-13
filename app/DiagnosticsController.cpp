#include "DiagnosticsController.h"
#include "Diagnostics.h"
#include <QClipboard>
#include <QGuiApplication>

DiagnosticsController::DiagnosticsController(DeviceDiscovery &discovery, QObject *parent)
    : QObject(parent), m_discovery(discovery) {
    connect(&discovery, &DeviceDiscovery::scanFinished, this, [this] {
        if (!m_pending)
            return;
        m_snapshot = QString::fromUtf8(Diagnostics::toJson(m_discovery.snapshot()));
        m_pending = false;
        emit snapshotChanged();
        emit busyChanged();
    });
}

void DiagnosticsController::refresh() {
    if (m_pending)
        return;
    m_pending = true;
    emit busyChanged();
    m_discovery.refresh(true);
}

void DiagnosticsController::copy(const QString &snapshot) {
    QGuiApplication::clipboard()->setText(snapshot);
}
