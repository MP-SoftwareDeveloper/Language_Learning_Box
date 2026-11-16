import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Controls.Material
import LearningBox

// One grammar line of a German noun with 🔊: "Plural Hunde", "Singular der Hund", "Singular die Ärztin" (the colour mark shows the gender).
// Or of a verb: "Partizip II ist gegangen · Ich bin gegangen." - the participle with 🔊, and under it the simple
// example sentence with its own 🔊.
// Only the word(s) are read aloud: no label; a plural without its article, several forms separated by a pause.
ColumnLayout {
    id: root
    property string line: ""
    property int pixelSize: 16
    // Word whose article gives the colour mark after the line ("der Hund": blue, "die ...": red, "das ...": green)
    property string markWord: ""

    // Verb: "Partizip II <participle> · <example>"
    readonly property bool isParticiple: /^\s*Partizip II(\s|$)/.test(line)
    readonly property var participleParts: isParticiple ? line.replace(/^\s*Partizip II\s*/, "").split(/\s+\u00b7\s+/) : []
    readonly property string participleText: (participleParts[0] ?? "").trim()
    readonly property string exampleText: (participleParts[1] ?? "").trim()

    readonly property bool isPlural: /^\s*((Mask|Fem)\.\s*)?(Pl\.|Plural)/.test(line)
    readonly property string spoken: {
        if (isParticiple)
            return participleText.replace(/\s*\/\s*/g, ", ") // "ist gefahren, hat gefahren": a pause between the forms
        const t = line.replace(/^\s*((Pl\.|Plural|Sg\.|Singular|Mask\.|Fem\.)\s*)+/, "")
        return (isPlural ? t.replace(/(^|\/\s*)die\s+/gi, "$1") : t).replace(/\s*\/\s*/g, ", ").trim()
    }

    // Shown with the full word: old cards store "Pl. Hunde" / "Sg. der Hund"
    readonly property string shown: isParticiple ? "Partizip II " + participleText : line
        .replace(/^\s*(?:Mask|Fem)\.\s+(?:Pl\.|Plural)\s*/, "Plural ")
        .replace(/^\s*(?:Mask|Fem)\.\s+/, "Singular ")
        .replace(/^\s*Pl\.\s*/, "Plural ")
        .replace(/^\s*Sg\.\s*/, "Singular ")

    visible: spoken !== ""
    spacing: 0

    TextMetrics {
        id: labelWidth
        font.pixelSize: 13
        text: "Singular:        " // 8 spaces
    }

    // Same look as the rows of the Dictionary (WordDetails): small dim label, then the word in bold
    readonly property bool hasLabel: /^(Plural|Singular|Partizip II)\s/.test(shown)
    readonly property string labelText: hasLabel ? /^(Plural|Singular|Partizip II)/.exec(shown)[1] : ""
    RowLayout {
        Layout.fillWidth: true
        spacing: 6
        Label {
            textFormat: Text.PlainText
            visible: root.hasLabel
            // "Singular:" + 8 spaces wide, so the words of all lines start at the same place
            Layout.preferredWidth: labelWidth.advanceWidth
            text: root.hasLabel ? root.labelText + ":" : ""
            font.pixelSize: 13
            opacity: 0.6
        }
        Label {
            textFormat: Text.PlainText
            Layout.fillWidth: true
            text: root.hasLabel ? root.shown.slice(root.labelText.length).trim() : root.shown
            wrapMode: Text.WordWrap
            font.pixelSize: root.pixelSize + 1
            font.bold: true
        }
        GenderMark { word: root.markWord }
        SpeakButton {
            Layout.alignment: Qt.AlignVCenter
            speakText: root.spoken
            languageTag: "de-DE"
            iconSize: 19
        }
    }

    // Verb: the simple example sentence under the participle, with its own 🔊
    RowLayout {
        Layout.fillWidth: true
        Layout.leftMargin: labelWidth.advanceWidth + 6 // under the participle, not under the label
        visible: root.exampleText !== ""
        spacing: 6
        Label {
            textFormat: Text.PlainText
            Layout.fillWidth: true
            text: root.exampleText
            wrapMode: Text.WordWrap
            font.pixelSize: root.pixelSize
            opacity: 0.85
        }
        SpeakButton {
            Layout.alignment: Qt.AlignVCenter
            speakText: root.exampleText
            languageTag: "de-DE"
            iconSize: 17
        }
    }
}
