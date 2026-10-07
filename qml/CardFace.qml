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
    readonly property bool meaningSpeakable: back !== "" && Translator.meaningLanguage !== "fa" && !/[؀-ۿ]/.test(back)
    readonly property string meaningTag: Translator.meaningLanguage === "en" ? "en-US" : "de-DE"

    spacing: 2

    // Grammar lines (plural, male / female forms) for cards that have none stored yet: looked up in
    // Wiktionary when the card is shown (saved on the phone after the first time); display only.
    readonly property string backText: back.replace(/\s*\u00b7\s*Pl\./, "\nPlural")
    readonly property bool hasForms: /(^|\n)\s*(Pl\.|Plural|Sg\.|Singular|Mask\.|Fem\.)/.test(backText)
    property var grammar: ({})
    property int grammarRequest: -1
    readonly property string formsText: hasForms ? backText : (grammar.forms ?? "")
    readonly property string markWord: /^(der|die|das)\s/i.test(front) ? front : (grammar.front ?? "")
    function lookupForms() {
        grammar = ({})
        grammarRequest = -1
        const w = front.trim()
        if (hasForms || CardStore.learningLanguage !== "de" || !/^((der|die|das)\s+)?[A-Z\u00C4\u00D6\u00DC]\S*$/.test(w))
            return
        grammarRequest = Translator.lookupGrammar(w)
    }
    onFrontChanged: Qt.callLater(lookupForms)
    Component.onCompleted: Qt.callLater(lookupForms)
    Connections {
        target: Translator
        function onGrammarFound(requestId, grammar) {
            if (requestId === root.grammarRequest) {
                root.grammarRequest = -1
                root.grammar = grammar
            }
        }
    }

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
        GenderMark { word: root.markWord }
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
        visible: root.back !== "" || root.formsText !== ""
        spacing: 4
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 1
            Repeater {
                model: root.back.replace(/\s*·\s*Pl\./, "\nPlural").split("\n").filter(l => !/^\s*(Pl\.|Plural|Sg\.|Singular|Mask\.|Fem\.)/.test(l))
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
            // "Pl. Hunde", "Fem. die Lehrerin", ...: each with 🔊 and its gender colour
            FormLines {
                Layout.fillWidth: true
                text: root.formsText
                word: root.markWord
                pixelSize: 13
            }
        }
        SpeakButton {
            id: meaningSpeaker
            Layout.alignment: Qt.AlignTop
            visible: root.meaningSpeakable
            speakText: root.back.split("\n")[0]
            languageTag: root.meaningTag
            iconSize: 19
            tint: "#e53935" // translation: red
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
