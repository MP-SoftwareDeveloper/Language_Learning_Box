import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LearningBox

// A whole card in a list (box pages, Favorite words): picture, front, every line of the meaning and
// the example sentence, each with 🔊 (the meaning only in English / German: no Persian voice).
// Items declared inside go next to the front's 🔊 (e.g. ★, ‹ ›); `note` is a small grey line below.
ColumnLayout {
    id: root
    property var card: ({})
    property string note: ""
    default property alias frontActions: actions.data

    readonly property string front: card.front ?? ""
    readonly property string back: card.back ?? ""
    readonly property string example: card.example ?? ""
    readonly property bool meaningSpeakable: Translator.meaningLanguage !== "fa" && !/[؀-ۿ]/.test(back)
    readonly property string meaningTag: Translator.meaningLanguage === "en" ? "en-US" : "de-DE"

    spacing: 2

    // Front
    RowLayout {
        Layout.fillWidth: true
        spacing: 4
        Image {
            Layout.preferredWidth: 40
            Layout.preferredHeight: 40
            visible: (root.card.imageUrl ?? "").toString() !== ""
            source: root.card.imageUrl ?? ""
            sourceSize: Qt.size(80, 80)
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            clip: true
        }
        Label {
            Layout.fillWidth: true
            text: root.front
            font.pixelSize: 17
            font.bold: true
            wrapMode: Text.WordWrap
        }
        SpeakButton { speakText: root.front }
        RowLayout {
            id: actions
            spacing: 0
        }
    }
    // Back: one label per line, each in its own direction (MeaningText, a Column, would make a
    // layout loop inside this row)
    RowLayout {
        Layout.fillWidth: true
        visible: root.back !== ""
        spacing: 4
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 1
            Repeater {
                model: root.back.split("\n")
                delegate: Label {
                    required property string modelData
                    required property int index
                    Layout.fillWidth: true
                    text: modelData
                    wrapMode: Text.WordWrap
                    font.pixelSize: index === 0 ? 15 : 12
                    opacity: index === 0 ? 0.9 : 0.65
                }
            }
        }
        SpeakButton {
            id: meaningSpeaker
            Layout.alignment: Qt.AlignTop
            visible: root.meaningSpeakable
            speakText: root.back.split("\n")[0]
            languageTag: root.meaningTag
        }
        // No voice for Persian: keep the same space on the right as the speaker button takes for
        // other meanings, so a Persian line does not touch the screen edge.
        Item {
            visible: !root.meaningSpeakable
            Layout.preferredWidth: meaningSpeaker.implicitWidth
            Layout.preferredHeight: 1
        }
    }
    // Example sentence
    RowLayout {
        Layout.fillWidth: true
        visible: root.example !== ""
        spacing: 4
        Label {
            Layout.fillWidth: true
            text: root.example
            wrapMode: Text.WordWrap
            font.italic: true
            font.pixelSize: 14
            opacity: 0.85
        }
        SpeakButton {
            Layout.alignment: Qt.AlignTop
            speakText: root.example
        }
    }
    Label {
        visible: text !== ""
        text: root.note
        font.pixelSize: 12
        opacity: 0.55
    }
}
