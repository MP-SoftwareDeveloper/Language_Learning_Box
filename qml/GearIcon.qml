import QtQuick

// Settings gear drawn with Canvas, so it has exactly `color` on every phone
// (the ⚙ character is drawn as a coloured emoji by many Android fonts).
Canvas {
    id: gear
    property color color: "white"
    implicitWidth: 24
    implicitHeight: 24
    onColorChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()

    onPaint: {
        const ctx = getContext("2d")
        ctx.reset()
        const s = Math.min(width, height)
        const cx = width / 2, cy = height / 2
        const teeth = 8
        const rOuter = s * 0.46, rInner = s * 0.36, rHole = s * 0.15
        ctx.fillStyle = gear.color
        ctx.beginPath()
        for (let i = 0; i < teeth * 2; ++i) {
            // each tooth: two points on the outer radius, then two on the inner radius
            const a0 = (i / (teeth * 2)) * 2 * Math.PI
            const a1 = ((i + 1) / (teeth * 2)) * 2 * Math.PI
            const r = (i % 2 === 0) ? rOuter : rInner
            if (i === 0)
                ctx.moveTo(cx + r * Math.cos(a0), cy + r * Math.sin(a0))
            else
                ctx.lineTo(cx + r * Math.cos(a0), cy + r * Math.sin(a0))
            ctx.lineTo(cx + r * Math.cos(a1), cy + r * Math.sin(a1))
        }
        ctx.closePath()
        // hole in the middle (even-odd: drawn as a second, reversed circle)
        ctx.moveTo(cx + rHole, cy)
        ctx.arc(cx, cy, rHole, 0, 2 * Math.PI, true)
        ctx.fill()
    }
}
