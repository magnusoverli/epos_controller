pragma ComponentBehavior: Bound
import QtQuick
import HeadsetController
import QtQuick.Controls
import QtQuick.Layouts

RowLayout {
    id: root
    required property SelectedDevice device
    required property SettingsController controller
    required property NotificationController notifications
    required property bool deviceSettings
    readonly property real editorWidth: settingsList.width * .35
    readonly property var catalog: deviceSettings ? SettingsCatalog.device : SettingsCatalog.application
    property var expandedSections: ({})
    function sectionKeyAt(index: int): string {
        for (let i = index; i >= 0; --i) {
            if (catalog[i].sectionKey !== undefined)
                return (deviceSettings ? "device-" : "application-") + catalog[i].sectionKey
        }
        return ""
    }
    function toggleSection(key: string) {
        const next = Object.assign({}, expandedSections)
        next[key] = !expandedSections[key]
        expandedSections = next
    }
    signal toggleRequested()
    spacing: 32
    FactoryResetDialog {
        id: resetDialog
        device: root.device
        controller: root.controller
    }
    Panel {
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.preferredWidth: 850
        ColumnLayout {
            anchors { fill: parent; margins: 20 }
            spacing: 8
            RowLayout {
                Layout.fillWidth: true
                Heading { text: root.deviceSettings ? qsTr("Device settings") : qsTr("Application settings"); Layout.fillWidth: true; wrapMode: Text.WordWrap }
                Label {
                    objectName: "settingsFirmwareVersion"
                    visible: root.deviceSettings
                    text: qsTr("Firmware: %1").arg(root.controller.firmwareVersion || qsTr("unknown"))
                    textFormat: Text.PlainText
                    color: Theme.muted
                    font.pixelSize: Theme.smallSize
                }
                OutlineButton {
                    objectName: "readSettings"
                    visible: root.deviceSettings
                    padding: 8
                    text: root.controller.status === "error" ? qsTr("Retry") : qsTr("Refresh")
                    enabled: root.controller.available && !root.controller.busy
                    onClicked: root.controller.refresh()
                }
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                objectName: "settingsStatus"
                visible: !root.deviceSettings || !root.controller.hasSnapshot || root.controller.status === "error"
                text: root.deviceSettings ? root.controller.message : qsTr("Application preferences will be added in a later milestone.")
                textFormat: Text.PlainText
                color: root.deviceSettings && root.controller.status === "error" ? Theme.error : Theme.muted
                font.pixelSize: Theme.smallSize
                Layout.preferredHeight: font.pixelSize * 2.5
                maximumLineCount: 2
                elide: Text.ElideRight
                ToolTip.visible: statusHover.hovered && text.length > 0
                ToolTip.text: text
                HoverHandler { id: statusHover }
            }
            RowLayout {
                visible: root.deviceSettings
                Layout.fillWidth: true
                Label {
                    objectName: "notificationStatus"
                    Layout.fillWidth: true
                    text: root.notifications.message
                    textFormat: Text.PlainText
                    wrapMode: Text.WordWrap
                    maximumLineCount: 1
                    elide: Text.ElideRight
                    font.pixelSize: Theme.smallSize
                    color: Theme.muted
                    ToolTip.visible: notificationHover.hovered
                    ToolTip.text: text
                    HoverHandler { id: notificationHover }
                }
                OutlineButton {
                    objectName: "retryNotifications"
                    padding: 8
                    visible: root.notifications.canRetry
                    text: qsTr("Retry notifications")
                    onClicked: root.notifications.retry()
                }
            }
            RowLayout {
                Layout.fillWidth: true
                Label { text: qsTr("FEATURE"); Layout.fillWidth: true; font.pixelSize: Theme.smallSize }
                Label { text: qsTr("SETTING"); Layout.preferredWidth: root.editorWidth; font.pixelSize: Theme.smallSize }
            }
            Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.line }
            ListView {
                id: settingsList
                objectName: "settingsList"
                Layout.fillHeight: true
                Layout.fillWidth: true
                clip: true
                // A focused dropdown must never be rebound to a different feature.
                // This small catalog still virtualizes rows, without recycling controls.
                reuseItems: false
                boundsBehavior: Flickable.StopAtBounds
                model: root.catalog
                ScrollBar.vertical: ScrollBar {}
                delegate: Item {
                    id: row
                    required property var modelData
                    required property int index
                    readonly property bool isSection: modelData.section !== undefined
                    readonly property string sectionKey: root.sectionKeyAt(index)
                    readonly property bool expanded: root.expandedSections[sectionKey] === true
                    readonly property bool shown: isSection || expanded
                    readonly property bool isNotification: !isSection && root.deviceSettings && (modelData.key === "muteNotification" || modelData.key === "muteInterval")
                    readonly property var control: !isSection && root.deviceSettings
                                                   ? isNotification ? root.notifications.controls[modelData.key] : root.controller.controls[modelData.key]
                                                   : undefined
                    readonly property bool isSidetone: !isSection && root.deviceSettings && modelData.key === "sidetone"
                    readonly property bool isReset: !isSection && root.deviceSettings && modelData.key === "factoryReset"
                    width: settingsList.width
                    visible: shown
                    height: !shown ? 0 : isSection ? Math.max(44, sectionHeader.implicitHeight)
                            : Math.max(isSidetone ? 66 : 47, featureLabel.implicitHeight + 16, editor.implicitHeight + 12)
                    Button {
                        id: sectionHeader
                        objectName: row.isSection ? "settings-section-" + row.sectionKey : ""
                        visible: row.isSection
                        anchors.fill: parent
                        checkable: true
                        checked: row.expanded
                        focusPolicy: Qt.StrongFocus
                        hoverEnabled: true
                        padding: 8
                        Accessible.name: row.modelData.section || ""
                        Accessible.description: row.expanded ? qsTr("Expanded. Activate to collapse.") : qsTr("Collapsed. Activate to expand.")
                        background: Rectangle {
                            color: sectionHeader.down ? Theme.pressed : sectionHeader.hovered ? Theme.hover : "transparent"
                            border.width: sectionHeader.visualFocus ? 2 : 0
                            border.color: Theme.focus
                        }
                        contentItem: Text {
                            text: (row.expanded ? "▾  " : "▸  ") + (row.modelData.section || "")
                            textFormat: Text.PlainText
                            color: Theme.ink
                            font.pixelSize: 15
                            verticalAlignment: Text.AlignVCenter
                            wrapMode: Text.WordWrap
                        }
                        onClicked: root.toggleSection(row.sectionKey)
                    }
                    RowLayout {
                        visible: !row.isSection && row.shown
                        anchors { fill: parent; bottomMargin: row.shown ? 1 : 0 }
                        spacing: 12
                        Label {
                            id: featureLabel
                            text: row.modelData.label || ""
                            textFormat: Text.PlainText
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            color: Theme.ink
                            font.pixelSize: 13
                        }
                        Rectangle {
                            visible: !row.isSection && !row.isReset && row.control === undefined
                            Layout.preferredWidth: root.editorWidth
                            Layout.minimumWidth: root.editorWidth
                            Layout.maximumWidth: root.editorWidth
                            implicitHeight: 30
                            color: Theme.unavailable
                            border.color: Theme.line
                            Label { anchors.centerIn: parent; text: qsTr("Unavailable"); color: Theme.disabled; font.pixelSize: Theme.smallSize }
                        }
                        SettingEditor {
                            id: editor
                            visible: row.control !== undefined
                            Layout.preferredWidth: root.editorWidth
                            Layout.minimumWidth: root.editorWidth
                            Layout.maximumWidth: root.editorWidth
                            settingKey: row.modelData.key || "section"
                            label: row.modelData.label || ""
                            control: row.control
                            onValueRequested: value => {
                                if (row.isNotification)
                                    root.notifications.apply(row.modelData.key, value)
                                else
                                    root.controller.apply(row.modelData.key, value)
                            }
                            onPercentRequested: percent => root.controller.applySidetonePercent(percent)
                        }
                        OutlineButton {
                            objectName: row.isReset ? "factoryReset" : ""
                            visible: row.isReset
                            enabled: root.controller.ready && !root.controller.busy
                            Layout.preferredWidth: root.editorWidth
                            text: qsTr("Reset headset…")
                            onClicked: resetDialog.request()
                        }
                    }
                    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.line }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                OutlineButton { objectName: "toggleSettings"; padding: 8; text: root.deviceSettings ? qsTr("Application settings") : qsTr("Device settings"); onClicked: root.toggleRequested() }
                Item { Layout.fillWidth: true }
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
    }
}
