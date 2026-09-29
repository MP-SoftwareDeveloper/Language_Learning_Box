import QtQuick
import QtQuick.Controls

// "from → to" built from separate labels, so mixed Persian / Latin text keeps its order and the
// arrow is always drawn with the normal font (Persian fonts on Android have no "→").
Row {
    id: root
    property string from
    property string to
    property bool arrow: true
    property bool bold: false
    property int pixelSize: 13
    property color color: "black"
    spacing: 5

    Label {
        anchors.verticalCenter: parent.verticalCenter
        text: root.from
        font.pixelSize: root.pixelSize
        font.bold: root.bold
        color: root.color
    }
    // Drawn, not a character: no font can be missing it.
    Canvas {
        id: arrowIcon
        anchors.verticalCenter: parent.verticalCenter
        visible: root.arrow
        width: root.pixelSize
        height: root.pixelSize * 0.7
        property color c: root.color
        onCChanged: requestPaint()
        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            ctx.strokeStyle = c
            ctx.fillStyle = c
            ctx.lineWidth = Math.max(1.5, height / 6)
            const y = height / 2
            ctx.beginPath()
            ctx.moveTo(0, y)
            ctx.lineTo(width - height / 2, y)
            ctx.stroke()
            ctx.beginPath()
            ctx.moveTo(width, y)
            ctx.lineTo(width - height / 1.6, 0)
            ctx.lineTo(width - height / 1.6, height)
            ctx.closePath()
            ctx.fill()
        }
    }
    Label {
        anchors.verticalCenter: parent.verticalCenter
        text: root.to
        font.pixelSize: root.pixelSize
        font.bold: root.bold
        color: root.color
    }
}
