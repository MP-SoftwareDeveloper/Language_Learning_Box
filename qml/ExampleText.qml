import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LearningBox

// A German example sentence with its translation underneath, in the translation language
// (Settings, or the switch on the review card: Persian or English). Source, first match wins:
//   1. `knownTranslation` (Persian; e.g. a word-pack preview row)
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
    spacing: 2

    readonly property bool persian: Translator.meaningLanguage === "fa"
    readonly property bool wanted: example.trim() !== ""
    readonly property string packTranslation: persian ? (knownTranslation || WordPacks.exampleTranslation(example)) : ""
    readonly property string translation: !wanted ? "" : (packTranslation || fetched)
    property string fetched: ""
    property int requestId: -1

    function lookup() {
        fetched = ""
        requestId = -1
        if (!wanted || packTranslation !== "")
            return
        const saved = Translator.saved(example)
        if (saved !== "")
            fetched = saved
        else if (Translator.useOnline && onlineLookup)
            debounce.restart() // don't translate every keystroke in the editor
    }
    onExampleChanged: lookup()
    onWantedChanged: lookup()
    Component.onCompleted: lookup()

    Timer {
        id: debounce
        interval: 700
        onTriggered: if (root.wanted) root.requestId = Translator.translate(root.example)
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
        Layout.fillWidth: true
        visible: root.showGerman && root.example !== ""
        text: root.example
        wrapMode: Text.WordWrap
        font.italic: true
        font.pixelSize: root.pixelSize
        opacity: root.germanOpacity
    }
    Label {
        Layout.fillWidth: true
        visible: root.translation !== ""
        text: root.translation
        wrapMode: Text.WordWrap
        horizontalAlignment: root.persian ? Text.AlignRight : Text.AlignLeft
        font.pixelSize: root.pixelSize - 1
        opacity: 0.75
    }
}
