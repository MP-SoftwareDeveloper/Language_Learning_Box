import QtQuick

// Five-pointed star drawn with Canvas (★/☆ are not in every font next to Persian text).
// filled: favorite; outline: not a favorite.
Canvas {
    id: icon
    property bool filled: false
    property color color: "#F5B301"
    implicitWidth: 22
    implicitHeight: 22
    onFilledChanged: requestPaint()
    onColorChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()

    onPaint: {
        const ctx = getContext("2d")
        ctx.reset()
        const cx = width / 2, cy = height / 2 + height * 0.04
        const R = Math.min(width, height) / 2 - 1.5, r = R * 0.45
        ctx.beginPath()
        for (let i = 0; i < 10; ++i) {
            const a = -Math.PI / 2 + i * Math.PI / 5
            const rad = i % 2 === 0 ? R : r
            const x = cx + rad * Math.cos(a), y = cy + rad * Math.sin(a)
            if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y)
        }
        ctx.closePath()
        ctx.lineJoin = "round"
        ctx.lineWidth = 1.8
        ctx.strokeStyle = icon.color
        if (icon.filled) {
            ctx.fillStyle = icon.color
            ctx.fill()
        }
        ctx.stroke()
    }
}
