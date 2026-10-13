import QtQuick
import HeadsetController
import QtQuick.Layouts

RowLayout {
    id: root
    required property SelectedDevice device
    signal diagnosticsRequested()
    spacing: 32
    HelpCard {
        heading: qsTr("About")
        detail: Qt.application.name + " " + Qt.application.version + qsTr("\n\nIndependent Linux prototype\nC++23 · Qt 6 / QML\n\nVisual reference: EPOS Connect 8.7.0\n\nNot an official EPOS application.")
    }
    HelpCard {
        heading: qsTr("Connection Status")
        detail: (root.device.connected ? qsTr("%1 connected\nUSB: %2\nHID interfaces detected: %3").arg(root.device.name).arg(root.device.usbId).arg(root.device.interfaceCount) : qsTr("No headset selected or connected")) + qsTr("\n\nDiscovery: Linux sysfs + udev\n\nNo SDK loaded\nNo USB commands sent\nNo cloud connection")
        diagnostics: true
        onDiagnosticsRequested: root.diagnosticsRequested()
    }
    HelpCard {
        heading: qsTr("Prototype Scope")
        detail: qsTr("Live device discovery\nRead-only device identity\nScreenshot-inspired navigation\n\nSettings transport: unverified\nFirmware updates: not implemented\n\nScreenshot values are never presented as live device values.")
    }
}
