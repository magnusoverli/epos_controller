import QtQuick
import HeadsetController
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    required property SelectedDevice device
    signal refreshRequested()
    signal settingsRequested()

    Column {
        visible: !root.device.connected
        anchors.centerIn: parent
        spacing: 24
        Heading { text: qsTr("No headset selected or connected"); anchors.horizontalCenter: parent.horizontalCenter }
        Label { text: qsTr("Connect an IMPACT 860T ANC, then select it above."); color: Theme.muted; anchors.horizontalCenter: parent.horizontalCenter }
        OutlineButton { objectName: "refreshDevices"; text: qsTr("Refresh devices"); onClicked: root.refreshRequested(); anchors.horizontalCenter: parent.horizontalCenter }
    }
    Panel {
        visible: root.device.connected
        width: 410
        height: parent.height
        ColumnLayout {
            anchors { fill: parent; margins: 24 }
            spacing: 14
            HeadsetArt { Layout.alignment: Qt.AlignHCenter; Layout.fillHeight: true; Layout.minimumHeight: 100 }
            Heading { objectName: "deviceName"; text: root.device.name; Layout.fillWidth: true; wrapMode: Text.Wrap; maximumLineCount: 3; elide: Text.ElideRight }
            Label { text: qsTr("Connected · %1").arg(root.device.usbId); textFormat: Text.PlainText; color: Theme.muted }
            OutlineButton { objectName: "deviceSettingsButton"; text: qsTr("Device settings  →"); onClicked: root.settingsRequested() }
            Label { text: qsTr("Settings access not yet implemented"); color: Theme.muted; font.pixelSize: Theme.smallSize }
        }
    }
}
