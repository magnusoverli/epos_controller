import QtQuick
import HeadsetController
import QtQuick.Controls
import QtQuick.Layouts

Panel {
    id: root
    required property string heading
    required property string detail
    property bool diagnostics: false
    signal diagnosticsRequested()
    Layout.fillWidth: true
    Layout.fillHeight: true
    Layout.preferredWidth: 400
    ColumnLayout {
        anchors { fill: parent; margins: Theme.panelPadding }
        spacing: 16
        ScrollView {
            id: scroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            clip: true
            Column {
                width: scroll.availableWidth
                spacing: 24
                Heading { width: parent.width; wrapMode: Text.WordWrap; text: root.heading }
                Label { width: parent.width; text: root.detail; textFormat: Text.PlainText; wrapMode: Text.Wrap; maximumLineCount: 30; elide: Text.ElideRight; color: Theme.muted; lineHeight: 1.3 }
            }
        }
        OutlineButton {
            objectName: "inspectInterfaces"
            Layout.fillWidth: true
            visible: root.diagnostics
            text: qsTr("Inspect USB interfaces")
            onClicked: root.diagnosticsRequested()
        }
    }
}
