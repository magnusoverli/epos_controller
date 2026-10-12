import QtQuick
import HeadsetController

Item {
    implicitWidth: 220
    implicitHeight: 230
    Canvas {
        anchors.fill: parent
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        onPaint: {
            const c = getContext("2d")
            c.reset()
            c.strokeStyle = "#25383d"
            c.lineWidth = 17
            c.lineCap = "round"
            c.beginPath()
            c.moveTo(width * .23, height * .65)
            c.bezierCurveTo(width * .10, 0, width * .90, 0, width * .77, height * .65)
            c.stroke()
            c.fillStyle = Theme.ink
            c.fillRect(width * .13, height * .50, width * .23, height * .30)
            c.fillRect(width * .64, height * .50, width * .23, height * .30)
            c.lineWidth = 7
            c.beginPath()
            c.moveTo(width * .73, height * .73)
            c.lineTo(width * .30, height * .85)
            c.lineTo(width * .15, height * .85)
            c.stroke()
        }
    }
}
