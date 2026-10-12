import QtQuick
import HeadsetController
import QtQuick.Controls

Button {
    id: control
    padding: 12
    background: Rectangle {
        color: control.down ? "#edf3f4" : Theme.surface
        border.color: Theme.ink
    }
    contentItem: Text {
        text: control.text
        color: control.enabled ? Theme.ink : "#888888"
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}
