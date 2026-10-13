pragma ComponentBehavior: Bound
import QtQuick
import HeadsetController
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: window
    required property DiscoveryModel discovery
    required property DiagnosticsController diagnosticsController
    property int page: Pages.Home
    property bool deviceSettings: false
    readonly property SelectedDevice device: discovery.selectedDevice
    function showDiagnostics() {
        diagnostics.returnFocus = activeFocusItem
        diagnostics.open()
    }

    width: 1440
    height: 810
    minimumWidth: 960
    minimumHeight: 650
    visible: true
    title: Qt.application.name + qsTr(" — read-only prototype")
    color: Theme.surface
    font.family: "Sans Serif"
    font.pixelSize: Theme.bodySize

    Label { x: 44; y: 26; text: Qt.application.name.toUpperCase(); font.pixelSize: 25; color: Theme.ink; textFormat: Text.PlainText }
    Label { anchors.right: parent.right; anchors.rightMargin: 44; y: 36; text: qsTr("LINUX  /  READ-ONLY PREVIEW"); color: Theme.muted; font.pixelSize: Theme.smallSize }
    RowLayout {
        anchors { left: parent.left; right: parent.right; leftMargin: 45; rightMargin: 70; top: parent.top; topMargin: 64 }
        Label {
            objectName: "discoveryStatus"
            text: window.discovery.message
            textFormat: Text.PlainText
            color: Theme.muted
            Layout.fillWidth: true
            elide: Text.ElideRight
        }
        ComboBox {
            id: selector
            objectName: "deviceSelector"
            visible: window.discovery.devices.length > 0 && (!window.device.connected || window.discovery.devices.length > 1)
            enabled: !window.discovery.stale
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
                onRefreshRequested: window.discovery.refresh(true)
                onSettingsRequested: { window.deviceSettings = true; window.page = Pages.Settings }
            }
        }
        LazyPage {
            objectName: "updatesLoader"
            selected: window.page === Pages.Updates
            sourceComponent: UpdatesPage { device: window.device }
        }
        LazyPage {
            objectName: "settingsLoader"
            selected: window.page === Pages.Settings
            sourceComponent: SettingsPage {
                device: window.device
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
                width: 65
                height: 52
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
                contentItem: Text { text: navButton.modelData.icon; font.pixelSize: 29; color: Theme.ink; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                onClicked: { window.page = modelData.page; if (modelData.page === Pages.Settings) window.deviceSettings = window.device.connected }
            }
        }
    }
}
