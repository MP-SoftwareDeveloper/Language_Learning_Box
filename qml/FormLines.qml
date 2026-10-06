import QtQuick
import QtQuick.Layouts
import LearningBox

// The grammar lines of a noun (one per line: "Pl. Hunde", "Sg. der Hund", "Mask. ...", "Fem. ..."),
// each with its own 🔊. `text` may hold other lines too: only the grammar lines are shown.
ColumnLayout {
    id: root
    property string text: ""
    property int pixelSize: 16
    property string word: ""   // the noun itself ("der Hund"): its colour goes with its own "Pl." / "Sg." line

    readonly property var lines: text.split("\n").filter(l => /^\s*(Pl|Sg|Mask|Fem)\./.test(l))
    // Colour of a line: masculine forms blue, feminine red, the noun's own forms by its article
    function markFor(line) {
        if (/^\s*Mask\./.test(line)) return "der "
        if (/^\s*Fem\./.test(line)) return "die "
        const m = /^\s*Sg\.\s*(der|die|das)\s/i.exec(line)
        return m ? m[1] + " " : root.word
    }

    visible: lines.length > 0
    spacing: 0

    Repeater {
        model: root.lines
        delegate: PluralLine {
            required property string modelData
            Layout.fillWidth: true
            line: modelData
            markWord: root.markFor(modelData)
            pixelSize: root.pixelSize
        }
    }
}
