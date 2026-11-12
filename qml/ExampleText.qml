import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LearningBox

// An example sentence (learning language) with its translation underneath, in the translation language
// (Settings, or the switch on the review card: Persian or English). Source, first match wins:
//   1. `knownTranslation` (already in the translation language; e.g. a word-pack preview row)
//   2. the word pack's curated Persian translation (also for cards already in the box)
//   3. a saved translation, else an online one (Settings: online on), fetched once and saved
ColumnLayout {
    id: root
    property string example: ""
    property string knownTranslation: ""
    property bool showGerman: true
    property int pixelSize: 16
    property bool onlineLookup: true // false in long lists (one request per row would be too many)
    property real germanOpacity: 0.85
    property bool speakTranslation: true // 🔊 on English / German translations
    property int clearToken: 0 // changes: remove the highlight of a selected word (the page saw a tap elsewhere)
    onClearTokenChanged: { germanEdit.deselect(); translationEdit.deselect() }
    property bool selectable: false // press and hold selects a word to copy (review card); off in lists, where a press must scroll / open
    spacing: 2

    property string target: ""    // translation language; "" = the learning box's meaning language
    property string sourceLang: "" // language of the sentence (with target); "" = the learning box's language
    readonly property string srcLang: sourceLang !== "" ? sourceLang : Translator.sourceLanguage
    readonly property string lang: target !== "" ? target : Translator.meaningLanguage
    readonly property bool persian: lang === "fa"
    readonly property bool wanted: example.trim() !== ""
    readonly property string packTranslation: knownTranslation !== "" ? knownTranslation
                                              : (persian ? WordPacks.exampleTranslation(example) : "")
    readonly property string translation: !wanted ? "" : (packTranslation || fetched)
    property string fetched: ""
    property int requestId: -1

    function lookup() {
        fetched = ""
        requestId = -1
        if (!wanted || packTranslation !== "")
            return
        const saved = root.target === "" ? Translator.saved(example)
                                         : Translator.savedBetween(example, root.srcLang, root.lang)
        if (saved !== "")
            fetched = saved
        else if (Translator.useOnline && onlineLookup)
            debounce.restart() // don't translate every keystroke in the editor
    }
    onExampleChanged: lookup()
    onLangChanged: lookup()
    onSrcLangChanged: lookup()
    onWantedChanged: lookup()
    Component.onCompleted: lookup()

    Timer {
        id: debounce
        interval: 700
        onTriggered: if (root.wanted) root.requestId = root.target === "" ? Translator.translate(root.example)
                    : Translator.translateBetween(root.example, root.srcLang, root.lang)
    }
    Connections {
        target: Translator
        function onSettingsChanged() { Qt.callLater(root.lookup) } // language switched; after bindings update
        function onTranslated(requestId, text) {
            if (requestId === root.requestId) {
                root.fetched = text
                root.requestId = -1
            }
        }
    }

    Label {
        textFormat: Text.PlainText
        id: germanLabel
        Layout.fillWidth: true
        visible: !root.selectable && root.showGerman && root.example !== ""
        text: root.example
        wrapMode: Text.WordWrap
        font.italic: true
        font.pixelSize: root.pixelSize
        opacity: root.germanOpacity
    }
    // Same look, but the text can be selected and copied (press and hold a word)
    TextEdit {
        id: germanEdit
        Layout.fillWidth: true
        visible: root.selectable && root.showGerman && root.example !== ""
        text: root.example
        readOnly: true
        selectByMouse: true
        textFormat: TextEdit.PlainText
        wrapMode: TextEdit.Wrap
        color: germanLabel.color
        selectionColor: germanLabel.palette.highlight
        selectedTextColor: germanLabel.palette.highlightedText
        onActiveFocusChanged: if (!activeFocus) deselect() // tapping elsewhere removes the highlight
        font.italic: true
        font.pixelSize: root.pixelSize
        opacity: root.germanOpacity
    }
    RowLayout {
        Layout.fillWidth: true
        visible: root.translation !== ""
        spacing: 0
        Label {
            textFormat: Text.PlainText
            id: translationLabel
            Layout.fillWidth: true
            visible: !root.selectable
            text: root.translation
            wrapMode: Text.WordWrap
            horizontalAlignment: root.persian ? Text.AlignRight : Text.AlignLeft
            font.pixelSize: root.pixelSize - 1
            opacity: 0.75
        }
        TextEdit {
            id: translationEdit
            Layout.fillWidth: true
            visible: root.selectable
            text: root.translation
            readOnly: true
            selectByMouse: true
                textFormat: TextEdit.PlainText
            wrapMode: TextEdit.Wrap
            color: translationLabel.color
            selectionColor: translationLabel.palette.highlight
            selectedTextColor: translationLabel.palette.highlightedText
            onActiveFocusChanged: if (!activeFocus) deselect()
            horizontalAlignment: root.persian ? TextEdit.AlignRight : TextEdit.AlignLeft
            font.pixelSize: root.pixelSize - 1
            opacity: 0.75
        }
        // English / German translations can be heard too (no Persian voice)
        SpeakButton {
            Layout.alignment: Qt.AlignTop
            visible: !root.persian && root.speakTranslation && !/[\u0600-\u06FF]/.test(root.translation)
            speakText: root.translation
            languageTag: root.lang === "en" ? "en-US" : "de-DE"
            iconSize: 19
            tint: "#e53935" // translation: red
        }
    }
}
