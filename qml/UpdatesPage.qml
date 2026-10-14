import QtQuick
import HeadsetController
import QtQuick.Controls
import QtQuick.Layouts

Panel {
    id: root
    required property SelectedDevice device
    required property SettingsController controller
    required property FirmwareController firmware
    Dialog {
        id: confirmation
        objectName: "firmwareInstallDialog"
        property string connection: ""
        property string version: ""
        title: qsTr("Install firmware %1?").arg(version)
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(520, parent ? parent.width - 40 : 520)
        height: Math.min(implicitHeight, parent ? parent.height - 40 : implicitHeight)
        modal: true
        contentItem: ScrollView {
            id: confirmationScroll
            implicitWidth: 440
            implicitHeight: 180
            clip: true
            Label {
                width: confirmationScroll.availableWidth
                text: qsTr("Close EPOS Connect Web and other headset controllers. Keep the headset connected and this app open until installation is verified. Audio will stop while the headset reboots.")
                textFormat: Text.PlainText
                wrapMode: Text.WordWrap
            }
        }
        footer: DialogButtonBox {
            Button {
                id: cancelInstall
                objectName: "cancelFirmwareInstall"
                text: qsTr("Cancel")
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            }
            Button {
                objectName: "confirmFirmwareInstall"
                text: qsTr("Install update")
                enabled: root.firmware.canInstall && confirmation.connection === root.device.connectionId
                         && confirmation.version === root.firmware.latestVersion
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            }
            onAccepted: confirmation.accept()
            onRejected: confirmation.reject()
        }
        onOpened: cancelInstall.forceActiveFocus(Qt.PopupFocusReason)
        onAccepted: root.firmware.installUpdate(connection, version)
        Connections {
            target: root.device
            function onChanged() {
                if (confirmation.visible && (!root.device.connected || confirmation.connection !== root.device.connectionId))
                    confirmation.reject()
            }
        }
    }
    ScrollView {
        id: scroll
        anchors { fill: parent; margins: 32 }
        contentWidth: availableWidth
        clip: true
        ColumnLayout {
            width: scroll.availableWidth
            spacing: 24
            Heading { text: qsTr("Firmware updates") }
            Label {
                text: root.device.connected ? root.device.name : qsTr("No headset selected or connected")
                textFormat: Text.PlainText
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                maximumLineCount: 3
                elide: Text.ElideRight
                color: Theme.ink
            }
            Label {
                objectName: "firmwareVersion"
                text: qsTr("Installed firmware: %1").arg(root.controller.firmwareVersion || qsTr("unknown"))
                visible: root.device.connected
                textFormat: Text.PlainText
                color: Theme.ink
            }
            Label {
                text: root.controller.firmwareMessage
                visible: !root.firmware.installing && root.device.connected && root.controller.firmwareVersion.length === 0
                textFormat: Text.PlainText
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Theme.muted
            }
            Label {
                objectName: "latestFirmwareVersion"
                text: qsTr("Latest firmware: %1").arg(root.firmware.latestVersion)
                visible: root.firmware.latestVersion.length > 0
                textFormat: Text.PlainText
                color: Theme.ink
            }
            BusyIndicator { running: root.firmware.busy; visible: running; Accessible.name: qsTr("Checking for firmware updates") }
            Label {
                objectName: "firmwareUpdateStatus"
                text: root.firmware.message
                visible: !root.firmware.installing
                textFormat: Text.PlainText
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Theme.ink
            }
            ProgressBar {
                Layout.fillWidth: true
                visible: root.firmware.installing
                value: root.firmware.installProgress
                Accessible.name: qsTr("Firmware installation progress")
            }
            Label {
                objectName: "firmwareInstallationStatus"
                text: root.firmware.installationMessage
                visible: text.length > 0
                textFormat: Text.PlainText
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Theme.ink
            }
            Flow {
                Layout.fillWidth: true
                spacing: 12
                OutlineButton {
                    objectName: "checkFirmwareUpdates"
                    text: qsTr("Check for updates")
                    enabled: root.firmware.canCheck
                    onClicked: root.firmware.checkForUpdates()
                }
                OutlineButton {
                    objectName: "installFirmwareUpdate"
                    text: qsTr("Install update")
                    visible: root.firmware.updateAvailable && !root.firmware.installing
                    enabled: root.firmware.canInstall
                    onClicked: {
                        confirmation.connection = root.device.connectionId
                        confirmation.version = root.firmware.latestVersion
                        confirmation.open()
                    }
                }
                OutlineButton {
                    text: qsTr("Retry firmware read")
                    visible: root.device.connected && !root.controller.firmwareVersion
                    enabled: !root.controller.busy
                    onClicked: root.controller.refreshFirmware()
                }
            }
            Label {
                visible: root.firmware.updateAvailable && !root.firmware.canInstall && !root.firmware.installing && !root.controller.busy
                text: qsTr("Installation is currently unavailable. Check headset access and the installation status above.")
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Theme.muted
            }
            Label {
                visible: root.firmware.checkedAt.length > 0
                text: qsTr("Last checked: %1").arg(new Date(root.firmware.checkedAt).toLocaleString(Qt.locale(), Locale.ShortFormat))
                color: Theme.muted
                textFormat: Text.PlainText
            }
        }
    }
}
