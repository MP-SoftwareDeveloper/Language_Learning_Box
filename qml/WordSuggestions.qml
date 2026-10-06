import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Controls.Material
import LearningBox

// Suggestions while typing, like a search box: words that start with what was typed. Word-pack words first
// (offline, with their meaning), then Wiktionary's prefix search (online, any word). Tap one to use it.
Pane {
    id: root
    // What the user typed and its language ("de", "en", "fa")
    property string text: ""
    property string language: "de"
    // A tap on a suggestion: {front, word, back, example} (word pack) or {front, word} (online).
    // `word` is the word without article: that is what goes into the text box (the article is shown apart).
    signal picked(var entry)
    function plain(w) { return (w ?? "").replace(/^(der|die|das)\s+/i, "") }

    // After a tap the list stays hidden until the text changes again
    property string dismissedFor: ""
    function dismiss(t) { dismissedFor = (t ?? text).trim() }

    // Suggestions are words without their article ("Brot", not "das Brot")
    function bare(w) { return (w ?? "").replace(/^(der|die|das)\s+/i, "") }

    property var entries: []
    property int requestId: -1
    readonly property string typed: bare(text.trim())

    function refresh() {
        requestId = -1
        const t = typed
        if (t.length < 2 || t === dismissedFor) {
            entries = []
            return
        }
        // Offline: the word pack (German)
        entries = language === "de"
                ? WordPacks.suggest(t, 5).map(e => Object.assign({}, e, { front: bare(e.front) })) : []
        online.restart()
    }
    onTextChanged: refresh()
    onLanguageChanged: refresh()

    Timer {
        id: online
        interval: 250 // wait for a short pause in typing
        onTriggered: if (root.typed.length >= 2 && root.typed !== root.dismissedFor)
                         root.requestId = Translator.suggestWords(root.typed, root.language)
    }
    Connections {
        target: Translator
        function onWordsSuggested(requestId, words) {
            if (requestId !== root.requestId)
                return
            root.requestId = -1
            const have = root.entries.map(e => e.front.toLowerCase())
            const merged = root.entries.slice()
            for (const w of words)
                if (merged.length < 8 && have.indexOf(w.toLowerCase()) < 0)
                    merged.push({ front: w })
            root.entries = merged
        }
    }

    visible: entries.length > 0 && typed !== dismissedFor
    padding: 0
    Material.elevation: 2
    Material.background: Qt.alpha(Material.foreground, 0.06)

    contentItem: Column {
        Repeater {
            model: root.entries
            delegate: ItemDelegate {
                required property var modelData
                required property int index
                width: root.width
                topPadding: 8
                bottomPadding: 8
                contentItem: RowLayout {
                    spacing: 8
                    Label {
                        text: "🔍"
                        opacity: 0.5
                        font.pixelSize: 13
                    }
                    Label {
                        text: root.plain(modelData.front)
                        font.pixelSize: 17
                        elide: Text.ElideRight
                        Layout.maximumWidth: root.width * 0.6
                    }
                    Label {
                        Layout.fillWidth: true
                        text: (modelData.back ?? "").split("\n")[0].replace(/\s*·\s*Pl\..*$/, "")
                        elide: Text.ElideRight
                        opacity: 0.55
                        font.pixelSize: 14
                        horizontalAlignment: Text.AlignRight
                    }
                }
                onClicked: {
                    root.dismiss(root.plain(modelData.front))
                    root.picked(Object.assign({ word: root.plain(modelData.front) }, modelData))
                }
            }
        }
    }
}
