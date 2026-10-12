pragma ComponentBehavior: Bound
import QtQuick
import HeadsetController
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: window
    required property DiscoveryModel discovery
    required property DiagnosticsController diagnosticsController
    property int page: 0
    property bool deviceSettings: false
    readonly property var device: discovery.devices.length ? discovery.devices[0] : null
    function showDiagnostics() { diagnostics.open() }

    width: 1440
    height: 810
    minimumWidth: 960
    minimumHeight: 650
    visible: true
    title: Qt.application.name + " — read-only prototype"
    color: Theme.surface
    font.family: "Sans Serif"
    font.pixelSize: 14

    Label { x: 44; y: 26; text: "HEADSET CONTROLLER"; font.pixelSize: 25; color: "#222222" }
    Label { anchors.right: parent.right; anchors.rightMargin: 44; y: 36; text: "LINUX  /  READ-ONLY PREVIEW"; color: Theme.muted; font.pixelSize: 12 }

    StackLayout {
        anchors {
            left: parent.left; right: parent.right; top: parent.top; bottom: navigation.top
            leftMargin: 45; rightMargin: 70; topMargin: 103; bottomMargin: 70
        }
        currentIndex: window.page
        HomePage {
            device: window.device
            onRefreshRequested: window.discovery.refresh()
            onSettingsRequested: { window.deviceSettings = true; window.page = 2 }
        }
        UpdatesPage { device: window.device }
        SettingsPage {
            device: window.device
            deviceSettings: window.deviceSettings
            onToggleRequested: window.deviceSettings = !window.deviceSettings
        }
        HelpPage { device: window.device; onDiagnosticsRequested: window.showDiagnostics() }
    }
    DiagnosticsDialog {
        id: diagnostics
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
                { icon: "⌂", label: "Home" }, { icon: "⟳", label: "Updates" },
                { icon: "⚙", label: "Settings" }, { icon: "?", label: "Help" }
            ]
            delegate: Button {
                id: navButton
                required property int index
                required property var modelData
                width: 65
                height: 52
                Accessible.name: modelData.label
                ToolTip.visible: hovered
                ToolTip.text: Accessible.name
                background: Rectangle { color: window.page === navButton.index ? "#edf3f4" : Theme.surface; radius: 26 }
                contentItem: Text { text: navButton.modelData.icon; font.pixelSize: 29; color: Theme.ink; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                onClicked: { window.page = index; if (index === 2) window.deviceSettings = !!window.device }
            }
        }
    }
}
