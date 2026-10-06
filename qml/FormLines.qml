import QtQuick
import QtQuick.Layouts
import LearningBox

// The grammar lines of a noun (one per line: "Pl. Hunde", "Sg. der Hund", "Mask. ...", "Fem. ..."),
// each with its own 🔊. `text` may hold other lines too: only the grammar lines are shown.
ColumnLayout {
    id: root
    property string text: ""
    property int pixelSize: 16

    readonly property var lines: text.split("\n").filter(l => /^\s*(Pl|Sg|Mask|Fem)\./.test(l))

    visible: lines.length > 0
    spacing: 0

    Repeater {
        model: root.lines
        delegate: PluralLine {
            required property string modelData
            Layout.fillWidth: true
            line: modelData
            pixelSize: root.pixelSize
        }
    }
}
