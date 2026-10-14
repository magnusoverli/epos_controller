pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import HeadsetController

Dialog {
    id: root
    objectName: "factoryResetDialog"
    required property SelectedDevice device
    required property SettingsController controller
    property string confirmedConnectionId: ""
    property string confirmedName: ""
    function request() {
        confirmedConnectionId = device.connectionId
        confirmedName = device.name
        open()
    }
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(520, parent ? parent.width - 40 : 520)
    height: Math.min(implicitHeight, parent ? parent.height - 40 : implicitHeight)
    modal: true
    title: qsTr("Factory reset headset?")
    contentItem: ScrollView {
        id: messageScroll
        implicitWidth: 440
        implicitHeight: 220
        clip: true
        Label {
            width: messageScroll.availableWidth
            text: qsTr("Reset %1 to its factory settings? This can erase settings beyond those shown here and interrupt audio. Use this only when the headset is idle. There is no automatic undo; the headset may reconnect.").arg(root.confirmedName)
            textFormat: Text.PlainText
            wrapMode: Text.WordWrap
        }
    }
    footer: DialogButtonBox {
        Button {
            id: cancelReset
            objectName: "cancelFactoryReset"
            text: qsTr("Cancel")
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
        }
        Button {
            objectName: "confirmFactoryReset"
            text: qsTr("Reset headset")
            enabled: root.controller.ready && !root.controller.busy && root.device.connected
                     && root.device.connectionId === root.confirmedConnectionId
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
        }
        onAccepted: root.accept()
        onRejected: root.reject()
    }
    onOpened: cancelReset.forceActiveFocus(Qt.PopupFocusReason)
    onAccepted: controller.factoryReset(confirmedConnectionId)
    Connections {
        target: root.device
        function onChanged() {
            if (root.visible && (!root.device.connected || root.device.connectionId !== root.confirmedConnectionId))
                root.reject()
        }
    }
}
