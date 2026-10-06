import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Controls.Material
import LearningBox

// One grammar line of a German noun with 🔊: "Pl. Hunde", "Sg. der Hund", "Mask. der Arzt", "Fem. die Ärztin".
// Only the word(s) are read aloud: no label; a plural without its article, several forms separated by a pause.
RowLayout {
    id: root
    property string line: ""
    property int pixelSize: 16
    // Word whose article gives the colour mark after the line ("der Hund": blue, "die ...": red, "das ...": green)
    property string markWord: ""

    readonly property bool isPlural: /^\s*((Mask|Fem)\.\s*)?Pl\./.test(line)
    readonly property string spoken: {
        const t = line.replace(/^\s*((Pl|Sg|Mask|Fem)\.\s*)+/, "")
        return (isPlural ? t.replace(/(^|\/\s*)die\s+/gi, "$1") : t).replace(/\s*\/\s*/g, ", ").trim()
    }

    visible: spoken !== ""
    spacing: 4

    Label {
        Layout.fillWidth: true
        text: root.line
        wrapMode: Text.WordWrap
        font.pixelSize: root.pixelSize
        font.italic: true
        color: Material.accent
    }
    GenderMark { word: root.markWord }
    SpeakButton {
        Layout.alignment: Qt.AlignVCenter
        speakText: root.spoken
        languageTag: "de-DE"
        iconSize: 19
    }
}
