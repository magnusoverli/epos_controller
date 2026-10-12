pragma ComponentBehavior: Bound
import QtQuick
import HeadsetController
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root
    required property DiagnosticsController controller
    property string snapshot: ""
    function refresh() { snapshot = controller.refresh() }
    onAboutToShow: refresh()
    title: "USB diagnostics — read-only"
    modal: true
    standardButtons: Dialog.Close
    contentItem: ColumnLayout {
        spacing: 12
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: "Report sizes describe the HID transport, not setting commands. Permission checks do not open device nodes. This report omits serial numbers."
            color: Theme.ink
        }
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            TextArea {
                text: root.snapshot
                readOnly: true
                selectByMouse: true
                font.family: "monospace"
                font.pixelSize: 12
                wrapMode: TextEdit.Wrap
            }
        }
        RowLayout {
            OutlineButton { text: "Refresh"; onClicked: root.refresh() }
            OutlineButton { text: "Copy diagnostics"; onClicked: root.controller.copy(root.snapshot) }
        }
    }
}
