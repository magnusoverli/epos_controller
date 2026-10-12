import QtQuick
import HeadsetController
import QtQuick.Controls
import QtQuick.Layouts

RowLayout {
    id: root
    required property var device
    spacing: 32
    Panel {
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.preferredWidth: 850
        ColumnLayout {
            anchors { fill: parent; margins: 32 }
            spacing: 30
            Heading { text: "Update Overview" }
            RowLayout {
                Layout.fillWidth: true
                Label { text: "PRODUCT"; Layout.fillWidth: true; color: Theme.muted }
                Label { text: "VERSION"; color: Theme.muted }
            }
            Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.line }
            RowLayout {
                Layout.fillWidth: true
                Label { text: Qt.application.name; Layout.fillWidth: true; color: Theme.ink }
                Label { text: Qt.application.version; color: Theme.ink }
            }
            Label { text: root.device ? root.device.name + " — Firmware unknown" : "No headset connected"; color: Theme.ink }
            Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.line }
            Item { Layout.fillHeight: true }
        }
    }
    Panel {
        Layout.preferredWidth: 380
        Layout.fillHeight: true
        Column {
            anchors { fill: parent; margins: 32 }
            spacing: 40
            Heading { text: "Firmware updates" }
            Label {
                width: parent.width
                wrapMode: Text.WordWrap
                text: "Firmware checking and installation are not implemented in this prototype. No update requests are sent."
                color: Theme.muted
                font.pixelSize: 18
            }
        }
    }
}
