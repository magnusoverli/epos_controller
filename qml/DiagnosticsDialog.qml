pragma ComponentBehavior: Bound
import QtQuick
import HeadsetController
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root
    required property DiagnosticsController controller
    property Item returnFocus
    onAboutToShow: controller.refresh()
    onClosed: if (returnFocus && returnFocus.visible) returnFocus.forceActiveFocus(Qt.TabFocusReason)
    title: qsTr("USB diagnostics — read-only")
    modal: true
    focus: true
    standardButtons: Dialog.Close
    contentItem: ColumnLayout {
        spacing: 12
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: qsTr("Report sizes describe the HID transport, not setting commands. Permission checks do not open device nodes. This report omits serial numbers.")
            color: Theme.ink
        }
        Label { visible: root.controller.busy; text: qsTr("Refreshing diagnostics…"); color: Theme.muted }
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            TextArea {
                objectName: "diagnosticText"
                text: root.controller.snapshot
                textFormat: TextEdit.PlainText
                readOnly: true
                selectByMouse: true
                font.family: "monospace"
                font.pixelSize: Theme.smallSize
                wrapMode: TextEdit.Wrap
            }
        }
        RowLayout {
            OutlineButton { objectName: "refreshDiagnostics"; text: qsTr("Refresh"); enabled: !root.controller.busy; onClicked: root.controller.refresh() }
            OutlineButton { objectName: "copyDiagnostics"; text: qsTr("Copy diagnostics"); enabled: root.controller.snapshot.length > 0; onClicked: root.controller.copy(root.controller.snapshot) }
        }
    }
}
