#pragma once

#include "DeviceAccessSetup.h"
#include "DeviceDiscovery.h"
#include "DiagnosticsController.h"
#include "FirmwareController.h"
#include "NotificationController.h"
#include "Pages.h"
#include "SelectedDevice.h"
#include "SettingsController.h"
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

struct SettingsRegistration {
    Q_GADGET
    QML_FOREIGN(SettingsController)
    QML_NAMED_ELEMENT(SettingsController)
    QML_UNCREATABLE("Injected by the application")
};

struct FirmwareRegistration {
    Q_GADGET
    QML_FOREIGN(FirmwareController)
    QML_NAMED_ELEMENT(FirmwareController)
    QML_UNCREATABLE("Injected by the application")
};

struct DeviceAccessRegistration {
    Q_GADGET
    QML_FOREIGN(DeviceAccessSetup)
    QML_NAMED_ELEMENT(DeviceAccessSetup)
    QML_UNCREATABLE("Injected by the application")
};

struct NotificationRegistration {
    Q_GADGET
    QML_FOREIGN(NotificationController)
    QML_NAMED_ELEMENT(NotificationController)
    QML_UNCREATABLE("Injected by the application")
};
