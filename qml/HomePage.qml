import QtQuick
import HeadsetController
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    required property var device
    signal refreshRequested()
    signal settingsRequested()

    Column {
        visible: !root.device
        anchors.centerIn: parent
        spacing: 24
        Heading { text: "No supported headset connected"; anchors.horizontalCenter: parent.horizontalCenter }
        Label { text: "Connect an IMPACT 860T ANC via USB to begin."; color: Theme.muted; anchors.horizontalCenter: parent.horizontalCenter }
        OutlineButton { text: "Refresh devices"; onClicked: root.refreshRequested(); anchors.horizontalCenter: parent.horizontalCenter }
    }
    Panel {
        visible: !!root.device
        width: 410
        height: parent.height
        ColumnLayout {
            anchors { fill: parent; margins: 24 }
            spacing: 14
            HeadsetArt { Layout.alignment: Qt.AlignHCenter; Layout.fillHeight: true; Layout.minimumHeight: 100 }
            Heading { text: root.device ? root.device.name : "" }
            Label { text: "Connected · " + (root.device ? root.device.usbId : ""); color: Theme.muted }
            OutlineButton { text: "Device settings  →"; onClicked: root.settingsRequested() }
            Label { text: "Settings access not yet implemented"; color: Theme.muted; font.pixelSize: 12 }
        }
    }
}
