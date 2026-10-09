import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LearningBox

// The example sentences of a card. A card keeps them all in its example field, one sentence per line (older
// cards have just one). Each sentence gets its own 🔊; `translated`: with its translation underneath (ExampleText).
ColumnLayout {
    id: root
    property string text: ""
    property bool translated: true      // false: just the sentence (small lists)
    property bool showGerman: true      // translated: also show the sentence itself
    property bool speak: true           // 🔊 for each sentence
    property int pixelSize: 16
    property bool onlineLookup: true    // see ExampleText
    readonly property var sentences: text.split("\n").map(s => s.trim()).filter(s => s !== "")

    visible: sentences.length > 0
    spacing: 6

    Repeater {
        model: root.sentences
        delegate: RowLayout {
            required property string modelData
            Layout.fillWidth: true
            spacing: 4
            ExampleText {
                Layout.fillWidth: true
                visible: root.translated
                example: root.translated ? modelData : ""
                showGerman: root.showGerman
                pixelSize: root.pixelSize
                onlineLookup: root.onlineLookup
            }
            Label {
                Layout.fillWidth: true
                visible: !root.translated
                text: modelData
                wrapMode: Text.WordWrap
                font.italic: true
                font.pixelSize: root.pixelSize
                opacity: 0.85
            }
            SpeakButton {
                Layout.alignment: Qt.AlignTop
                visible: root.speak
                speakText: modelData
            }
        }
    }
}
