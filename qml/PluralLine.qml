import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Controls.Material
import LearningBox

// The plural of a German noun ("Pl. Hunde") on its own line with 🔊. Only the plural word(s) are read
// aloud: no "Pl.", no article, several forms separated by a pause.
RowLayout {
    id: root
    property string line: ""
    property int pixelSize: 16

    readonly property string spoken: line.replace(/^\s*Pl\.\s*/i, "")
                                         .replace(/(^|\/\s*)die\s+/gi, "$1")
                                         .replace(/\s*\/\s*/g, ", ").trim()

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
    SpeakButton {
        Layout.alignment: Qt.AlignVCenter
        speakText: root.spoken
        languageTag: "de-DE"
        iconSize: 19
    }
}
