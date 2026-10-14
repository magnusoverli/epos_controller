pragma ComponentBehavior: Bound
import QtQuick
import HeadsetController
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: window
    required property DiscoveryModel discovery
    required property DiagnosticsController diagnosticsController
    required property SettingsController settingsController
    required property FirmwareController firmwareController
    required property NotificationController notificationController
    required property DeviceAccessSetup accessSetup
    property int page: Pages.Home
    property bool deviceSettings: false
    property bool automaticSettingsReads: true
    onClosing: function(close) {
        if (firmwareController.installing) {
            close.accepted = false
            page = Pages.Updates
        }
    }
    readonly property SelectedDevice device: discovery.selectedDevice
    Binding {
        target: window.settingsController
        property: "active"
        value: window.automaticSettingsReads && window.page === Pages.Settings && window.deviceSettings
    }
    function showDiagnostics() {
        diagnostics.returnFocus = activeFocusItem
        diagnostics.open()
    }

    width: 1440
    height: 810
    minimumWidth: 960
    minimumHeight: 650
    visible: true
    title: Qt.application.name + qsTr(" — Linux preview")
    color: Theme.surface
    font.family: "Sans Serif"
    font.pixelSize: Theme.bodySize

    Label { x: 44; y: 26; text: Qt.application.name.toUpperCase(); font.pixelSize: 25; color: Theme.ink; textFormat: Text.PlainText }
    Label { anchors.right: parent.right; anchors.rightMargin: 44; y: 36; text: qsTr("LINUX  /  SETTINGS PREVIEW"); color: Theme.muted; font.pixelSize: Theme.smallSize }
    RowLayout {
        anchors { left: parent.left; right: parent.right; leftMargin: 45; rightMargin: 70; top: parent.top; topMargin: 64 }
        Label {
            objectName: "discoveryStatus"
            text: window.accessSetup.busy || (window.settingsController.needsAccess && window.accessSetup.message.length > 0)
                  ? window.accessSetup.message : window.discovery.message
            textFormat: Text.PlainText
            color: Theme.muted
            Layout.fillWidth: true
            elide: Text.ElideRight
            ToolTip.visible: statusHover.hovered && text.length > 0
            ToolTip.text: text
            HoverHandler { id: statusHover }
        }
        OutlineButton {
            objectName: "globalFirmwareUpdateNotice"
            visible: window.firmwareController.updateAvailable || window.firmwareController.installing
            text: window.firmwareController.installing ? qsTr("Firmware installation…") : qsTr("Firmware %1 available").arg(window.firmwareController.latestVersion)
            onClicked: window.page = Pages.Updates
        }
        OutlineButton {
            objectName: "retryAccessSetup"
            visible: window.accessSetup.canRetry
            enabled: !window.settingsController.busy
            text: qsTr("Retry access setup")
            onClicked: window.accessSetup.enableAccess()
        }
        ComboBox {
            id: selector
            objectName: "deviceSelector"
            visible: window.discovery.devices.length > 0 && (!window.device.connected || window.discovery.devices.length > 1)
            enabled: !window.discovery.stale && !window.firmwareController.installing
            Layout.maximumWidth: 300
            model: window.discovery.devices
            textRole: "name"
            valueRole: "connectionId"
            currentIndex: {
                const list = window.discovery.devices
                for (let i = 0; i < list.length; ++i)
                    if (list[i].connectionId === window.device.connectionId) return i
                return -1
            }
            displayText: currentIndex < 0 ? qsTr("Select a connected headset") : currentText
            contentItem: Label {
                text: selector.displayText
                textFormat: Text.PlainText
                elide: Text.ElideRight
                verticalAlignment: Text.AlignVCenter
                color: Theme.ink
                rightPadding: selector.indicator ? selector.indicator.width + 8 : 8
            }
            delegate: ItemDelegate {
                id: choice
                required property var modelData
                required property int index
                width: selector.width
                highlighted: selector.highlightedIndex === index
                text: modelData.name
                contentItem: Label { text: choice.text; textFormat: Text.PlainText; elide: Text.ElideRight }
            }
            Accessible.name: qsTr("Selected headset")
            onActivated: window.discovery.selectDevice(String(currentValue))
        }
    }

    StackLayout {
        id: pages
        objectName: "pages"
        anchors { left: parent.left; right: parent.right; top: parent.top; bottom: navigation.top
            leftMargin: 45; rightMargin: 70; topMargin: 112; bottomMargin: 60 }
        currentIndex: window.page
        LazyPage {
            objectName: "homeLoader"
            selected: window.page === Pages.Home
            sourceComponent: HomePage {
                device: window.device
                controller: window.settingsController
                firmware: window.firmwareController
                onRefreshRequested: window.discovery.refresh(true)
                onSettingsRequested: { window.deviceSettings = true; window.page = Pages.Settings }
                onUpdatesRequested: window.page = Pages.Updates
            }
        }
        LazyPage {
            objectName: "updatesLoader"
            selected: window.page === Pages.Updates
            sourceComponent: UpdatesPage {
                device: window.device
                controller: window.settingsController
                firmware: window.firmwareController
            }
        }
        LazyPage {
            objectName: "settingsLoader"
            selected: window.page === Pages.Settings
            sourceComponent: SettingsPage {
                device: window.device
                controller: window.settingsController
                notifications: window.notificationController
                deviceSettings: window.deviceSettings
                onToggleRequested: window.deviceSettings = !window.deviceSettings
            }
        }
        LazyPage {
            objectName: "helpLoader"
            selected: window.page === Pages.Help
            sourceComponent: HelpPage { device: window.device; onDiagnosticsRequested: window.showDiagnostics() }
        }
    }
    DiagnosticsDialog {
        id: diagnostics
        objectName: "diagnosticsDialog"
        controller: window.diagnosticsController
        anchors.centerIn: parent
        width: Math.min(820, window.width - 80)
        height: Math.min(640, window.height - 80)
    }
    Row {
        id: navigation
        anchors { bottom: parent.bottom; bottomMargin: 25; horizontalCenter: parent.horizontalCenter }
        spacing: 48
        Repeater {
            model: [
                { icon: "⌂", label: qsTr("Home"), page: Pages.Home },
                { icon: "⟳", label: qsTr("Updates"), page: Pages.Updates },
                { icon: "⚙", label: qsTr("Settings"), page: Pages.Settings },
                { icon: "?", label: qsTr("Help"), page: Pages.Help }
            ]
            delegate: Button {
                id: navButton
                required property var modelData
                objectName: "nav-" + modelData.page
                width: 88
                height: 64
                focusPolicy: Qt.StrongFocus
                hoverEnabled: true
                Accessible.name: modelData.label
                Accessible.checkable: true
                Accessible.checked: window.page === modelData.page
                ToolTip.visible: hovered
                ToolTip.text: Accessible.name
                background: Rectangle {
                    color: navButton.down ? Theme.pressed : window.page === navButton.modelData.page ? Theme.selected : navButton.hovered ? Theme.hover : Theme.surface
                    radius: 26
                    border.width: navButton.visualFocus ? 3 : 0
                    border.color: Theme.focus
                }
                contentItem: Column {
                    spacing: 2
                    Text { width: parent.width; text: navButton.modelData.icon; font.pixelSize: 25; color: Theme.ink; horizontalAlignment: Text.AlignHCenter }
                    Text { width: parent.width; text: navButton.modelData.label; font.pixelSize: Theme.smallSize; color: Theme.ink; horizontalAlignment: Text.AlignHCenter }
                }
                onClicked: { window.page = modelData.page; if (modelData.page === Pages.Settings) window.deviceSettings = window.device.connected }
            }
        }
    }
}
