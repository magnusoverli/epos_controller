pragma ComponentBehavior: Bound
import QtQuick
import HeadsetController
import QtQuick.Controls
import QtQuick.Layouts

RowLayout {
    id: root
    required property var device
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
            Heading { text: root.deviceSettings ? "IMPACT 860T ANC" : "Application Settings" }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: root.deviceSettings ? (root.device ? "Connected — configuration values cannot yet be read. Controls are unavailable." : "Headset disconnected — configuration unavailable.") : "Application preferences will be added in a later milestone."
                color: Theme.muted
                font.pixelSize: 12
            }
            RowLayout {
                Layout.fillWidth: true
                Label { text: "FEATURE"; Layout.fillWidth: true; font.pixelSize: 12 }
                Label { text: "SETTING"; Layout.preferredWidth: settingsScroll.width * .35; font.pixelSize: 12 }
            }
            Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.line }
            ScrollView {
                id: settingsScroll
                Layout.fillHeight: true
                Layout.fillWidth: true
                clip: true
                contentWidth: availableWidth
                Column {
                    width: settingsScroll.availableWidth
                    Repeater {
                        model: root.deviceSettings ? SettingsCatalog.device : SettingsCatalog.application
                        delegate: Item {
                            id: row
                            required property var modelData
                            readonly property bool isSection: modelData.section !== undefined
                            width: settingsScroll.availableWidth
                            height: Math.max(isSection ? 52 : 47, featureLabel.implicitHeight + 16)
                            RowLayout {
                                anchors { fill: parent; bottomMargin: 1 }
                                spacing: 12
                                Label {
                                    id: featureLabel
                                    text: row.isSection ? "⌄  " + row.modelData.section : row.modelData.label
                                    Layout.fillWidth: true
                                    wrapMode: Text.WordWrap
                                    color: Theme.ink
                                    font.pixelSize: row.isSection ? 15 : 13
                                }
                                Rectangle {
                                    visible: !row.isSection
                                    Layout.preferredWidth: settingsScroll.width * .35
                                    implicitHeight: 30
                                    color: "#fafafa"
                                    border.color: "#eeeeee"
                                    Label { anchors.centerIn: parent; text: "Unavailable"; color: "#888888"; font.pixelSize: 12 }
                                }
                            }
                            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.line }
                        }
                    }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                OutlineButton { text: root.deviceSettings ? "Application settings" : "Device settings"; onClicked: root.toggleRequested() }
                Item { Layout.fillWidth: true }
                OutlineButton { text: "Restore Settings"; enabled: false }
            }
        }
    }
    ColumnLayout {
        visible: root.deviceSettings
        Layout.preferredWidth: root.width > 1100 ? 365 : 220
        Layout.alignment: Qt.AlignVCenter
        spacing: 20
        HeadsetArt { Layout.alignment: Qt.AlignHCenter; Layout.preferredWidth: 220; Layout.preferredHeight: 230 }
        Label { Layout.alignment: Qt.AlignHCenter; text: root.device ? "USB ID: " + root.device.usbId : "Disconnected"; color: Theme.muted }
        Label { Layout.alignment: Qt.AlignHCenter; text: "Firmware: unknown"; color: Theme.muted }
    }
}
