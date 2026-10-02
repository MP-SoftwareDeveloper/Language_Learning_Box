import QtQuick
import QtQuick.Controls

Item {
    id: root
    property alias text: btn.text
    property alias enabled: btn.enabled
    property int baseHeight: 48
    property real tailAngle: 0

    implicitWidth: 200
    implicitHeight: baseHeight * 2

    signal clicked()

    // Drive tail 0 -> 360 in 2 s, looping
    NumberAnimation on tailAngle {
        from: 0; to: 360; duration: 2000
        loops: Animation.Infinite
        running: root.visible
    }

    // Actual button fills the whole item (handles press / disabled state)
    Button {
        id: btn
        anchors.fill: parent
        highlighted: true
        onClicked: root.clicked()
    }

    // Rainbow tail drawn on top (no mouse handlers → transparent to input)
    Canvas {
        id: tailCanvas
        anchors.fill: parent

        // Mirror the animated property so the change signal fires here
        property real angle: root.tailAngle
        onAngleChanged: requestPaint()

        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)

            var w   = width
            var h   = height
            var lw  = 5                     // tail stroke width
            var P   = 2 * (w + h)           // total perimeter
            var len = P * 0.4               // tail covers 40 % of perimeter
            var head = (root.tailAngle / 360) * P

            // Rainbow stops: red → orange → yellow → green → cyan → blue → violet
            var pal = [
                [255,   0,   0],
                [255, 128,   0],
                [255, 255,   0],
                [  0, 255,   0],
                [  0, 200, 255],
                [  0,   0, 255],
                [200,   0, 255]
            ]

            var N = 30  // segments

            for (var i = 0; i < N; i++) {
                var t0 = i / N
                var t1 = (i + 1) / N

                // perimeter positions for this segment
                var from = head - (1.0 - t0) * len
                var to   = head - (1.0 - t1) * len

                // colour: tail (t=0) is dim/red, head (t=1) is bright/violet
                var ci   = t0 * (pal.length - 1)
                var idx0 = Math.floor(ci)
                var idx1 = Math.min(idx0 + 1, pal.length - 1)
                var f    = ci - idx0
                var c0   = pal[idx0], c1 = pal[idx1]
                var R = Math.round(c0[0] + f * (c1[0] - c0[0]))
                var G = Math.round(c0[1] + f * (c1[1] - c0[1]))
                var B = Math.round(c0[2] + f * (c1[2] - c0[2]))
                var a = 0.2 + 0.8 * t0   // fade in from tail to head

                ctx.strokeStyle = "rgba(" + R + "," + G + "," + B + "," + a + ")"
                ctx.lineWidth   = lw
                ctx.lineCap     = (i === N - 1) ? "round" : "butt"

                var p0 = perimPt(from, w, h, lw)
                var p1 = perimPt(to,   w, h, lw)
                ctx.beginPath()
                ctx.moveTo(p0.x, p0.y)
                ctx.lineTo(p1.x, p1.y)
                ctx.stroke()
            }
        }

        // Convert a scalar position along the rectangle perimeter to {x, y}
        function perimPt(pos, w, h, lw) {
            var P    = 2 * (w + h)
            pos      = ((pos % P) + P) % P
            var half = lw / 2
            if (pos < w)       return { x: pos,      y: half     }
            pos -= w
            if (pos < h)       return { x: w - half, y: pos      }
            pos -= h
            if (pos < w)       return { x: w - pos,  y: h - half }
            pos -= w
            return { x: half, y: h - pos }
        }
    }
}
