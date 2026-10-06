import QtQuick

// Brush-stroke colour mark after a German noun: blue = der (maskulin), red = die (feminin),
// green = das (neutral). Hidden when the text does not start with an article.
Canvas {
    id: root
    property string word: ""

    readonly property string article: {
        const m = /^(der|die|das)\s/i.exec(word)
        return m ? m[1].toLowerCase() : ""
    }
    readonly property color markColor: article === "der" ? "#1f4fc4" : article === "die" ? "#e31b1b" : "#4fa22e"

    visible: article !== ""
    implicitWidth: 38
    implicitHeight: 16

    onMarkColorChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    onPaint: {
        const ctx = getContext("2d")
        ctx.reset()
        const w = width, h = height
        ctx.fillStyle = markColor
        ctx.beginPath()
        ctx.moveTo(w * 0.04, h * 0.30)
        ctx.bezierCurveTo(w * 0.25, h * 0.05, w * 0.70, h * 0.18, w * 0.97, h * 0.08)
        ctx.lineTo(w * 0.93, h * 0.52)
        ctx.lineTo(w * 0.98, h * 0.92)
        ctx.bezierCurveTo(w * 0.70, h * 1.00, w * 0.30, h * 0.84, w * 0.03, h * 0.96)
        ctx.lineTo(w * 0.07, h * 0.62)
        ctx.closePath()
        ctx.fill()
    }
}
