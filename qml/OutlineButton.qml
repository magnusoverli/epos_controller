import QtQuick
import HeadsetController
import QtQuick.Controls

Button {
    id: control
    padding: 12
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    background: Rectangle {
        objectName: "buttonBackground"
        color: !control.enabled ? Theme.unavailable : control.down ? Theme.pressed : control.hovered ? Theme.hover : Theme.surface
        border.color: control.visualFocus ? Theme.focus : control.enabled ? Theme.ink : Theme.line
        border.width: control.visualFocus ? 3 : 1
    }
    contentItem: Text {
        text: control.text
        font: control.font
        textFormat: Text.PlainText
        wrapMode: Text.WordWrap
        color: control.enabled ? Theme.ink : Theme.disabled
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}
