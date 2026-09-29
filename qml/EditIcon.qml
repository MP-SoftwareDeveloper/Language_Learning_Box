import QtQuick

// Pencil drawn with Canvas: the ✎ character is missing from many Android fonts.
Canvas {
    id: icon
    property color color: "white"
    implicitWidth: 20
    implicitHeight: 20
    onColorChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()

    onPaint: {
        const ctx = getContext("2d")
        ctx.reset()
        const s = Math.min(width, height)
        ctx.save()
        ctx.translate(width / 2, height / 2)
        ctx.rotate(Math.PI / 4)          // pencil at 45°, tip bottom-left
        ctx.fillStyle = icon.color
        const w = s * 0.24, L = s * 0.9
        // body
        ctx.fillRect(-w / 2, -L / 2, w, L * 0.62)
        // tip
        ctx.beginPath()
        ctx.moveTo(-w / 2, -L / 2 + L * 0.68)
        ctx.lineTo(w / 2, -L / 2 + L * 0.68)
        ctx.lineTo(0, L / 2)
        ctx.closePath()
        ctx.fill()
        ctx.restore()
    }
}
