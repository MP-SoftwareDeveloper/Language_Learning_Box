import QtQuick

// Import (down) / export (up) icon: an arrow over an open tray.
// Used for "Get cards" (down) and "Send cards" (up).
Canvas {
    id: icon
    property color color: "white"
    property bool down: true
    implicitWidth: 24
    implicitHeight: 24
    onColorChanged: requestPaint()
    onDownChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()

    onPaint: {
        const ctx = getContext("2d")
        ctx.reset()
        const s = width / 24
        ctx.strokeStyle = icon.color
        ctx.fillStyle = icon.color
        ctx.lineWidth = 2.3 * s
        ctx.lineCap = "round"
        ctx.lineJoin = "round"

        // Tray: open-top U
        ctx.beginPath()
        ctx.moveTo(5 * s, 14 * s)
        ctx.lineTo(5 * s, 19 * s)
        ctx.lineTo(19 * s, 19 * s)
        ctx.lineTo(19 * s, 14 * s)
        ctx.stroke()

        // Arrow: shaft + chevron head, direction set by `down`
        const headY = icon.down ? 13 : 3
        const tailY = icon.down ? 3 : 13
        const tipY  = icon.down ? 13.5 : 2.5
        const wingY = icon.down ? 8.5 : 7.5

        ctx.beginPath()
        ctx.moveTo(12 * s, tailY * s)
        ctx.lineTo(12 * s, headY * s)
        ctx.stroke()

        ctx.beginPath()
        ctx.moveTo(7 * s, wingY * s)
        ctx.lineTo(12 * s, tipY * s)
        ctx.lineTo(17 * s, wingY * s)
        ctx.stroke()
    }
}
