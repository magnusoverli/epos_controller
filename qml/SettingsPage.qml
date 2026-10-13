pragma ComponentBehavior: Bound
import QtQuick
import HeadsetController
import QtQuick.Controls
import QtQuick.Layouts

RowLayout {
    id: root
    required property SelectedDevice device
    required property bool deviceSettings
    signal toggleRequested()
    spacing: 32
    Panel {
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.preferredWidth: 850
        ColumnLayout {
            anchors { fill: parent; margins: 32 }
            spacing: 14
            Heading { text: root.deviceSettings ? "IMPACT 860T ANC" : qsTr("Application Settings"); Layout.fillWidth: true; wrapMode: Text.WordWrap }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: root.deviceSettings ? (root.device.connected ? qsTr("Connected — configuration values cannot yet be read. Controls are unavailable.") : qsTr("Headset disconnected or not selected — configuration unavailable.")) : qsTr("Application preferences will be added in a later milestone.")
                color: Theme.muted
                font.pixelSize: Theme.smallSize
            }
            RowLayout {
                Layout.fillWidth: true
                Label { text: qsTr("FEATURE"); Layout.fillWidth: true; font.pixelSize: Theme.smallSize }
                Label { text: qsTr("SETTING"); Layout.preferredWidth: settingsList.width * .35; font.pixelSize: Theme.smallSize }
            }
            Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.line }
            ListView {
                id: settingsList
                objectName: "settingsList"
                Layout.fillHeight: true
                Layout.fillWidth: true
                clip: true
                reuseItems: true
                boundsBehavior: Flickable.StopAtBounds
                model: root.deviceSettings ? SettingsCatalog.device : SettingsCatalog.application
                ScrollBar.vertical: ScrollBar {}
                delegate: Item {
                    id: row
                    required property var modelData
                    readonly property bool isSection: modelData.section !== undefined
                    width: settingsList.width
                    height: Math.max(isSection ? 52 : 47, featureLabel.implicitHeight + 16)
                    RowLayout {
                        anchors { fill: parent; bottomMargin: 1 }
                        spacing: 12
                        Label {
                            id: featureLabel
                            text: row.isSection ? "⌄  " + row.modelData.section : row.modelData.label
                            textFormat: Text.PlainText
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            color: Theme.ink
                            font.pixelSize: row.isSection ? 15 : 13
                        }
                        Rectangle {
                            visible: !row.isSection
                            Layout.preferredWidth: settingsList.width * .35
                            implicitHeight: 30
                            color: Theme.unavailable
                            border.color: Theme.line
                            Label { anchors.centerIn: parent; text: qsTr("Unavailable"); color: Theme.disabled; font.pixelSize: Theme.smallSize }
                        }
                    }
                    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.line }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                OutlineButton { objectName: "toggleSettings"; text: root.deviceSettings ? qsTr("Application settings") : qsTr("Device settings"); onClicked: root.toggleRequested() }
                Item { Layout.fillWidth: true }
                OutlineButton { text: qsTr("Restore Settings"); enabled: false }
            }
        }
    }
    ColumnLayout {
        visible: root.deviceSettings && root.width > 1000
        Layout.preferredWidth: 300
        Layout.alignment: Qt.AlignVCenter
        spacing: 20
        HeadsetArt { Layout.alignment: Qt.AlignHCenter; Layout.preferredWidth: 220; Layout.preferredHeight: 230 }
        Label { Layout.alignment: Qt.AlignHCenter; text: root.device.connected ? qsTr("USB ID: %1").arg(root.device.usbId) : qsTr("Disconnected"); textFormat: Text.PlainText; color: Theme.muted }
        Label { Layout.alignment: Qt.AlignHCenter; text: qsTr("Firmware: unknown"); color: Theme.muted }
    }
}
