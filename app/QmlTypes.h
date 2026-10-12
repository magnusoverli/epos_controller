#pragma once

#include "DeviceDiscovery.h"
#include "DiagnosticsController.h"
#include <QtQml/qqmlregistration.h>

// Register presentation-facing types without coupling the Core library to QML.
struct DiscoveryModelRegistration {
    Q_GADGET
    QML_FOREIGN(DeviceDiscovery)
    QML_NAMED_ELEMENT(DiscoveryModel)
    QML_UNCREATABLE("Injected by the application")
};

struct DiagnosticsRegistration {
    Q_GADGET
    QML_FOREIGN(DiagnosticsController)
    QML_NAMED_ELEMENT(DiagnosticsController)
    QML_UNCREATABLE("Injected by the application")
};
