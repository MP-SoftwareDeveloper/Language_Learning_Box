import QtQuick
import QtQuick.Layouts
import LearningBox

// The grammar lines of a noun (one per line: "Plural Hunde", "Singular die Lehrerin", ...),
// each with its own 🔊. `text` may hold other lines too: only the grammar lines are shown.
ColumnLayout {
    id: root
    property string text: ""
    property int pixelSize: 16
    property string word: ""   // the noun itself ("der Hund"): its colour goes with its own "Pl." / "Sg." line

    readonly property var lines: text.split("\n").filter(l => /^\s*(Pl\.|Plural|Sg\.|Singular|Mask\.|Fem\.)/.test(l))
    // Colour of each line (the gender is never written): a "Singular der/die/das ..." line and the "Plural ..." line
    // after it take that article; the noun's own lines (before any other singular) take its own article.
    // Old cards: "Mask. ..." blue, "Fem. ..." red.
    readonly property var marks: {
        const out = []
        let cur = ""
        for (const l of lines) {
            const m = /^\s*(?:Sg\.|Singular)\s*(der|die|das)\s/i.exec(l)
            if (/^\s*Mask\./.test(l)) cur = "der "
            else if (/^\s*Fem\./.test(l)) cur = "die "
            else if (m) cur = m[1].toLowerCase() + " "
            out.push(cur !== "" ? cur : root.word)
        }
        return out
    }

    visible: lines.length > 0
    spacing: 0

    Repeater {
        model: root.lines
        delegate: PluralLine {
            required property string modelData
            required property int index
            Layout.fillWidth: true
            line: modelData
            markWord: root.marks[index]
            pixelSize: root.pixelSize
        }
    }
}
