pragma ComponentBehavior: Bound
import QtQuick
import HeadsetController
import QtQuick.Controls
import QtQuick.Layouts

RowLayout {
    id: root
    required property var device
    signal diagnosticsRequested()
    spacing: 32
    Repeater {
        model: [
            { heading: "About", diagnostics: false, detail: Qt.application.name + " " + Qt.application.version + "\n\nIndependent Linux prototype\nC++23 · Qt 6 / QML\n\nVisual reference: EPOS Connect 8.7.0\n\nNot an official EPOS application." },
            { heading: "Connection Status", diagnostics: true, detail: (root.device ? root.device.name + " connected\nUSB: " + root.device.usbId + "\nHID interfaces detected: " + root.device.hidInterfaces.length : "No supported headset connected") + "\n\nDiscovery: Linux sysfs\n\nNo SDK loaded\nNo USB commands sent\nNo cloud connection" },
            { heading: "Prototype Scope", diagnostics: false, detail: "Live device discovery\nRead-only device identity\nScreenshot-inspired navigation\n\nSettings transport: unverified\nFirmware updates: not implemented\n\nScreenshot values are never presented as live device values." }
        ]
        delegate: Panel {
            id: card
            required property var modelData
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredWidth: 400
            ScrollView {
                id: scroll
                anchors { fill: parent; margins: 24 }
                contentWidth: availableWidth
                clip: true
                Column {
                    width: scroll.availableWidth
                    spacing: 24
                    Heading { width: parent.width; wrapMode: Text.WordWrap; text: card.modelData.heading }
                    Label { width: parent.width; text: card.modelData.detail; wrapMode: Text.WordWrap; color: "#607277"; lineHeight: 1.3 }
                    OutlineButton { visible: card.modelData.diagnostics; text: "Inspect USB interfaces"; onClicked: root.diagnosticsRequested() }
                }
            }
        }
    }
}
