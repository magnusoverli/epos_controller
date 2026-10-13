#pragma once

#include "DeviceDiscovery.h"
#include "DiagnosticsController.h"
#include "Pages.h"
#include "SelectedDevice.h"
#include <QtQml/qqmlregistration.h>

// Register presentation-facing types without coupling the Core library to QML.
struct DiscoveryModelRegistration {
    Q_GADGET
    QML_FOREIGN(DeviceDiscovery)
    QML_NAMED_ELEMENT(DiscoveryModel)
    QML_UNCREATABLE("Injected by the application")
};

struct SelectedDeviceRegistration {
    Q_GADGET
    QML_FOREIGN(SelectedDevice)
    QML_NAMED_ELEMENT(SelectedDevice)
    QML_UNCREATABLE("Owned by discovery")
};

struct PagesRegistration {
    Q_GADGET
    QML_FOREIGN_NAMESPACE(Pages)
    QML_NAMED_ELEMENT(Pages)
};

struct DiagnosticsRegistration {
    Q_GADGET
    QML_FOREIGN(DiagnosticsController)
    QML_NAMED_ELEMENT(DiagnosticsController)
    QML_UNCREATABLE("Injected by the application")
};
