import QtCore
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Controls.Material
import LearningBox

// Dictionary: look up a word or sentence in both directions between the learning language of the
// selected learning box and a second language: the box's meaning language by default, or any other
// one picked at the top (e.g. Deutsch ↔ فارسی or Deutsch ↔ English; remembered per learning language).
// Word pack (German → Persian, offline), saved translations, online translation (Full app),
// example sentences, 🔊 for German / English, and "Add to learning box".
Page {
    id: page
    title: qsTr("Dictionary")

    readonly property string learn: CardStore.learningLanguage   // "de" / "en"
    readonly property string boxMeaning: Translator.meaningLanguage // "fa" / "en" / "de"
    // Second language of the dictionary: never the learning language
    readonly property var otherLanguages: ["fa", "en", "de"].filter(c => c !== learn)
    readonly property string chosen: learn === "en" ? prefs.otherForEnglish : prefs.otherForGerman
    readonly property string meaning: otherLanguages.indexOf(chosen) >= 0 ? chosen : boxMeaning
    function choose(c) {
        if (learn === "en") prefs.otherForEnglish = c
        else prefs.otherForGerman = c
        if (reverse) { reverse = false; field.text = "" }   // the typed text was in the old language
        lookup()
    }
    Settings {
        id: prefs
        category: "dictionary"
        property string otherForGerman: ""
        property string otherForEnglish: ""
    }
    property bool reverse: false                                 // false: learn → meaning
    readonly property string from: reverse ? meaning : learn
    readonly property string to: reverse ? learn : meaning

    function langName(c) { return c === "fa" ? "فارسی" : c === "de" ? "Deutsch" : "English" }
    function tag(c) { return c === "en" ? "en-US" : c === "de" ? "de-DE" : "" }
    function persianText(t) { return /[؀-ۿ]/.test(t) }

    property string query: ""        // what was looked up
    property string headword: ""     // pack form with article, e.g. "das Brot"
    property string result: ""
    property var alternatives: []
    property string source: ""       // "pack", "online", "saved"
    property string error: ""
    property int requestId: -1
    property var examples: []
    property int examplesRequest: -1

    // The word in the learning language and its meaning, whichever way it was looked up
    readonly property string learnWord: reverse ? result.split("\n")[0] : (headword || query)
    readonly property string meaningText: reverse ? query : result

    function clearResult() {
        headword = ""; result = ""; alternatives = []; source = ""; error = ""
        requestId = -1; examples = []; examplesRequest = -1
    }
    function lookup() {
        clearResult()
        query = field.text.trim()
        suggestions = []
        if (query === "")
            return
        // Offline first: the German word pack with its curated Persian meaning
        if (!reverse && learn === "de" && meaning === "fa") {
            const known = WordPacks.lookup(query)
            if (known.back) {
                headword = known.front
                result = known.back
                source = "pack"
            }
        }
        requestId = Translator.translateBetween(query, from, to) // online, else saved
        if (!reverse)
            findExamples(query)
    }
    function findExamples(word) {
        if (word.trim() === "" || word.split(" ").length > 3)
            return
        examplesRequest = Translator.suggestExamples(word) // learning-language sentences
    }

    Connections {
        target: Translator
        function onTranslated(requestId, text, alternatives, source, error) {
            if (requestId !== page.requestId)
                return
            page.requestId = -1
            page.error = error ?? ""
            if (text !== "") {
                if (page.source === "pack") {
                    // Keep the word pack's meaning first; the translator's words become extras
                    page.alternatives = [text].concat(alternatives ?? []).filter(a => page.result.indexOf(a) < 0)
                } else {
                    page.result = text
                    page.alternatives = alternatives ?? []
                    page.source = source
                }
            }
            if (page.reverse && page.result !== "")
                page.findExamples(page.result.split("\n")[0])
        }
        function onExamplesSuggested(requestId, examples) {
            if (requestId === page.examplesRequest) {
                page.examples = examples
                page.examplesRequest = -1
            }
        }
        function onSettingsChanged() { Qt.callLater(page.lookup) } // another learning box / meaning language
    }

    // German words from the word pack while typing
    property var suggestions: []
    Timer {
        id: typing
        interval: 600
        onTriggered: page.lookup()
    }

    Component { id: editPage; CardEditPage {} }
    StackView.onDeactivating: Speaker.stop() // leaving the page: stop reading aloud

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth

        ColumnLayout {
            width: parent.width
            spacing: 8

            // Direction and swap
            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 8
                spacing: 8
                Pane {
                    padding: 8
                    leftPadding: 16
                    rightPadding: 16
                    background: Rectangle {
                        radius: height / 2
                        color: "transparent"
                        border.width: 1
                        border.color: Material.accentColor
                    }
                    contentItem: DirectionLabel {
                        from: page.langName(page.from)
                        to: page.langName(page.to)
                        pixelSize: 15
                        bold: true
                        color: Material.foreground
                    }
                }
                RoundButton {
                    objectName: "swapButton"
                    // Swap arrows drawn (a "⇄" character can be missing from the phone's fonts)
                    contentItem: Canvas {
                        implicitWidth: 22
                        implicitHeight: 22
                        property color c: Material.foreground
                        onCChanged: requestPaint()
                        onPaint: {
                            const ctx = getContext("2d")
                            ctx.reset()
                            ctx.strokeStyle = c
                            ctx.fillStyle = c
                            ctx.lineWidth = 2
                            const w = width, h = height
                            // → on top, ← below
                            ctx.beginPath(); ctx.moveTo(2, h * 0.3); ctx.lineTo(w - 7, h * 0.3); ctx.stroke()
                            ctx.beginPath(); ctx.moveTo(w - 2, h * 0.3); ctx.lineTo(w - 8, h * 0.3 - 5); ctx.lineTo(w - 8, h * 0.3 + 5); ctx.closePath(); ctx.fill()
                            ctx.beginPath(); ctx.moveTo(w - 2, h * 0.7); ctx.lineTo(7, h * 0.7); ctx.stroke()
                            ctx.beginPath(); ctx.moveTo(2, h * 0.7); ctx.lineTo(8, h * 0.7 - 5); ctx.lineTo(8, h * 0.7 + 5); ctx.closePath(); ctx.fill()
                        }
                    }
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Swap languages")
                    onClicked: {
                        // Look the answer up the other way (first line: the meaning without grammar notes)
                        const answer = page.result.split("\n")[0].split(/[،,]/)[0].trim()
                        page.reverse = !page.reverse
                        if (answer !== "")
                            field.text = answer
                        page.lookup()
                    }
                }
            }

            // Second language: Persian / English / German (not the learning language)
            RowLayout {
                objectName: "languageChooser"
                Layout.alignment: Qt.AlignHCenter
                spacing: 4
                Label {
                    text: qsTr("With:")
                    opacity: 0.7
                }
                Repeater {
                    model: page.otherLanguages
                    delegate: Button {
                        required property string modelData
                        checkable: false
                        flat: modelData !== page.meaning
                        highlighted: modelData === page.meaning
                        font.pixelSize: 14
                        text: page.langName(modelData)
                        onClicked: if (modelData !== page.meaning) page.choose(modelData)
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 8
                TextField {
                    id: field
                    Layout.fillWidth: true
                    placeholderText: qsTr("Type a word or sentence in %1").arg(page.langName(page.from))
                    font.pixelSize: 18
                    inputMethodHints: Qt.ImhNoPredictiveText
                    horizontalAlignment: page.persianText(text) ? TextInput.AlignRight : TextInput.AlignLeft
                    onTextEdited: {
                        const t = text.trim()
                        page.suggestions = !page.reverse && page.learn === "de" && t.length >= 2 ? WordPacks.suggest(t, 5) : []
                        typing.restart()
                    }
                    onAccepted: { typing.stop(); page.lookup() }
                }
                SpeakButton {
                    visible: page.from !== "fa"
                    speakText: field.text
                    languageTag: page.tag(page.from)
                }
            }

            // Word pack suggestions
            Flow {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                spacing: 6
                visible: page.suggestions.length > 0
                Repeater {
                    model: page.suggestions
                    delegate: Button {
                        required property var modelData
                        flat: true
                        font.pixelSize: 13
                        text: modelData.front
                        onClicked: {
                            field.text = modelData.front
                            typing.stop()
                            page.lookup()
                        }
                    }
                }
            }

            HintLabel {
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                visible: page.query === ""
                text: (Translator.useOnline
                       ? qsTr("Look words up in both directions — tap the arrows to swap. Everything you look up is saved, so it also works offline later.")
                       : qsTr("Offline: only the word pack and translations saved earlier are found. Online translation is part of the Full app (Settings → App)."))
            }

            // ---- Result ----
            Pane {
                Layout.fillWidth: true
                Layout.leftMargin: 12
                Layout.rightMargin: 12
                visible: page.query !== ""
                Material.elevation: 2

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 6

                    // Looked-up word
                    RowLayout {
                        Layout.fillWidth: true
                        Label {
                            Layout.fillWidth: true
                            text: page.headword || page.query
                            wrapMode: Text.WordWrap
                            font.pixelSize: 22
                            font.bold: true
                            horizontalAlignment: page.persianText(text) ? Text.AlignRight : Text.AlignLeft
                        }
                        SpeakButton {
                            visible: page.from !== "fa"
                            speakText: page.headword || page.query
                            languageTag: page.tag(page.from)
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        visible: page.requestId >= 0 && page.result === ""
                        opacity: 0.6
                        text: qsTr("Looking up…")
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: page.requestId < 0 && page.result === ""
                        wrapMode: Text.WordWrap
                        opacity: 0.7
                        text: Translator.useOnline
                              ? qsTr("No translation found%1.").arg(page.error ? " (" + page.error + ")" : "")
                              : qsTr("Not in the word pack and not saved yet. Online translation is part of the Full app.")
                    }

                    // Translation
                    RowLayout {
                        Layout.fillWidth: true
                        visible: page.result !== ""
                        MeaningText {
                            Layout.fillWidth: true
                            text: page.result
                            pixelSize: 20
                        }
                        SpeakButton {
                            Layout.alignment: Qt.AlignTop
                            visible: page.to !== "fa" && !page.persianText(page.result)
                            speakText: page.result.split("\n")[0]
                            languageTag: page.tag(page.to)
                        }
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: page.alternatives.length > 0
                        wrapMode: Text.WordWrap
                        opacity: 0.75
                        horizontalAlignment: page.to === "fa" ? Text.AlignRight : Text.AlignLeft
                        text: qsTr("Also:") + " " + page.alternatives.join(page.to === "fa" ? "، " : ", ")
                    }
                    HintLabel {
                        visible: page.source !== ""
                        text: page.source === "pack" ? qsTr("From the word pack")
                              : page.source === "online" ? qsTr("Online translation (saved for offline use)")
                              : qsTr("Saved translation")
                    }

                    Button {
                        Layout.topMargin: 4
                        highlighted: true
                        visible: page.result !== "" && page.learnWord !== ""
                        text: qsTr("+ Add to “%1”").arg(CardStore.currentCollectionName)
                        // Meaning in another language than the box's: the editor fills in the box's meaning
                        onClicked: page.StackView.view.push(editPage, {
                            initialFront: page.learnWord,
                            initialBack: page.meaning === page.boxMeaning ? page.meaningText : "",
                            initialExample: page.examples.length > 0 ? page.examples[0].text : ""
                        })
                    }
                }
            }

            // ---- Example sentences (learning language, with translation) ----
            Label {
                Layout.leftMargin: 16
                Layout.topMargin: 4
                visible: page.examples.length > 0 || page.examplesRequest >= 0
                text: qsTr("Example sentences")
                font.bold: true
            }
            Label {
                Layout.leftMargin: 16
                visible: page.examplesRequest >= 0 && page.examples.length === 0
                opacity: 0.6
                text: qsTr("Searching…")
            }
            Repeater {
                model: page.examples
                delegate: RowLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.leftMargin: 16
                    Layout.rightMargin: 8
                    ExampleText {
                        Layout.fillWidth: true
                        example: modelData.text
                        target: page.meaning
                        knownTranslation: page.meaning === page.boxMeaning ? (modelData.translation ?? "") : ""
                        pixelSize: 15
                    }
                    SpeakButton {
                        Layout.alignment: Qt.AlignTop
                        speakText: modelData.text
                    }
                }
            }
            Item { Layout.preferredHeight: 16 }
        }
    }
}
