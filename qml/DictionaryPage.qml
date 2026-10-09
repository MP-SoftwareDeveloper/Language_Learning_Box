import QtCore
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Controls.Material
import LearningBox

// Dictionary: look up a word or sentence between any two languages (Deutsch, English, فارسی), picked in
// two combo boxes at the top: the input language and the output language (remembered). The default is the
// learning language of the selected box and its meaning language. Cards can be added when one of the two
// languages is the learning language.
// Word pack (German → Persian, offline), saved translations, online translation (Full app),
// example sentences, 🔊 for German / English, and "Add to learning box".
Page {
    id: page
    title: qsTr("Dictionary")

    readonly property string learn: CardStore.learningLanguage   // "de" / "en"
    readonly property string boxMeaning: Translator.meaningLanguage // "fa" / "en" / "de"
    readonly property var languages: [
        { code: "de", name: "Deutsch" }, { code: "en", name: "English" }, { code: "fa", name: "فارسی" }
    ]
    // Input and output language: the saved choice when it is valid, else the box's languages
    readonly property bool savedPairValid: prefs.fromLang !== prefs.toLang
                                           && languages.some(l => l.code === prefs.fromLang)
                                           && languages.some(l => l.code === prefs.toLang)
    readonly property string from: savedPairValid ? prefs.fromLang : learn
    readonly property string to: savedPairValid ? prefs.toLang
                                 : (boxMeaning !== learn ? boxMeaning : (learn === "en" ? "de" : "en"))
    function setPair(f, t) {
        prefs.fromLang = f
        prefs.toLang = t
    }
    // Input language picked: when it is the output language, the two swap. The typed text was in the old language.
    function setFrom(c) {
        if (c === from)
            return
        setPair(c, c === to ? from : to)
        field.text = ""
        lookup()
    }
    function setTo(c) {
        if (c === to)
            return
        if (c === from) {
            setPair(to, from)
            field.text = ""
        } else {
            setPair(from, c)
        }
        lookup()
    }
    function swap() {
        // Look the answer up the other way (first line: the meaning without grammar notes)
        const answer = result.split("\n")[0].split(/[،,]/)[0].trim()
        setPair(to, from)
        if (answer !== "")
            field.text = answer
        lookup()
    }
    Settings {
        id: prefs
        category: "dictionary"
        property string fromLang: ""
        property string toLang: ""
    }
    // learn → other language (the card's front is the typed word) / other language → learn (the front is the answer)
    // / free: neither is the learning language (no cards, no grammar block)
    readonly property bool reverse: from !== learn && to === learn
    readonly property bool free: from !== learn && to !== learn
    // The language the meaning (card back) is in
    readonly property string meaning: reverse ? from : to

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
    readonly property string chosenExample: wordInfo.chosen // sentences that go on the card, one per line (check boxes)

    // Save the looked-up word as a card in the selected learning box, with the chosen sentence.
    // A word that is already a card is only touched when the user says so (updateExisting).
    function quickAdd(updateExisting) {
        const front = cardFront.trim()
        if (front === "" || result === "")
            return
        const id = CardStore.findByFront(front)
        if (id >= 0 && !updateExisting) {
            existsDialog.existing = CardStore.card(id)
            existsDialog.open()
            return
        }
        if (id >= 0) {
            const c = CardStore.card(id)
            const ok = CardStore.updateCard(id, c.front, cardBack !== "" ? cardBack : c.back,
                                            chosenExample !== "" ? chosenExample : (c.example ?? ""), c.image ?? "")
            toast.show(ok ? qsTr("Card updated") : qsTr("Could not save"))
        } else {
            toast.show(CardStore.addCard(front, cardBack, chosenExample) >= 0 ? qsTr("Card added")
                                                                                 : qsTr("Could not save"))
        }
    }

    // The word in the learning language and its meaning, whichever way it was looked up
    readonly property string learnWord: free ? "" : reverse ? result.split("\n")[0] : (headword || query)
    readonly property string meaningText: reverse ? query : result

    // Word-pack meanings keep the plural on the same line ("Brot · Pl. die Brote"): show it on its own line
    readonly property string meaningLines: meaningText.replace(/\s*·\s*Pl\./, "\nPlural")
    // Back of the card: the meaning, then "Pl. die Hunde" on a new line (unless the meaning has it already)
    readonly property string cardBack: {
        if (reverse || free || meaningLines === "" || wordInfo.forms === "")
            return meaningLines
        // the online forms (Mask./Fem./Pl.) replace the word pack's plural
        return meaningLines.replace(/(\n?(Pl\.|Plural|Sg\.|Singular|Mask\.|Fem\.)[^\n]*)+$/, "") + "\n" + wordInfo.forms
    }
    // Front of the card: "der Hund" (article from the word pack or from Wiktionary)
    readonly property string cardFront: {
        if (!reverse && /^(der|die|das)\s/i.test(headword))
            return headword
        return wordInfo.front !== "" ? wordInfo.front : learnWord
    }

    // "Pl. Hunde" on its own line (with 🔊) and the rest of the back without it
    readonly property string cardForms: reverse ? "" : cardBack
    readonly property string cardBody: reverse ? result : cardBack.split("\n").filter(l => !/^\s*(Pl\.|Plural|Sg\.|Singular|Mask\.|Fem\.)/.test(l)).join("\n")

    function clearResult() {
        headword = ""; result = ""; alternatives = []; source = ""; error = ""
        requestId = -1
    }
    function lookup() {
        clearResult()
        query = field.text.trim()
        suggestions = []
        if (query === "")
            return
        // Offline first: the German word pack with its curated Persian meaning
        if (from === learn && learn === "de" && to === "fa") {
            const known = WordPacks.lookup(query)
            if (known.back) {
                headword = known.front
                result = known.back
                source = "pack"
            }
        }
        requestId = Translator.translateBetween(query, from, to) // online, else saved
        wordInfo.refreshNow() // article, forms and example sentences of the word
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
        }
        function onSettingsChanged() { Qt.callLater(page.lookup) } // another learning box / meaning language
    }

    // German words from the word pack while typing
    property var suggestions: []

    // Article, forms, example sentences and the capital letter: the same WordLookup as in Add card
    WordLookup {
        id: wordInfo
        word: page.free ? "" : page.reverse ? page.result.split("\n")[0] : page.query
        meaning: page.meaning
        onCapitalise: (text) => {
            if (page.reverse || page.free)
                return
            field.text = text
            typing.stop()
            page.lookup() // translate the word with its capital letter ("kellner" is a verb for the translator)
        }
    }
    Timer {
        id: typing
        interval: 600
        onTriggered: page.lookup()
    }

    Component { id: editPage; CardEditPage {} }
    StackView.onDeactivating: Speaker.stop() // leaving the page: stop reading aloud

    // Tap outside the text field or swipe the page: drop the focus and close the keyboard.
    function dismissKeyboard(scenePoint) {
        if (scenePoint !== undefined) {
            const q = field.mapFromItem(null, scenePoint.x, scenePoint.y)
            if (q.x >= 0 && q.y >= 0 && q.x <= field.width && q.y <= field.height)
                return
        }
        page.forceActiveFocus()
        Qt.inputMethod.hide()
    }
    // Passive handler on the page (empty area below the content) ...
    TapHandler {
        onTapped: (eventPoint) => page.dismissKeyboard(eventPoint.scenePosition)
    }

    ScrollView {
        id: scroller
        anchors.fill: parent
        contentWidth: availableWidth

        // Swiping up or down also closes the keyboard (its contentItem is the Flickable).
        Connections {
            target: scroller.contentItem
            function onMovementStarted() { page.dismissKeyboard() }
        }

        ColumnLayout {
            width: parent.width
            spacing: 8

            // ... and inside the scrolled content: the Flickable swallows touches before the
            // page-level handler sees them, a handler inside the content sees them first.
            TapHandler {
                onTapped: (eventPoint) => page.dismissKeyboard(eventPoint.scenePosition)
            }

            // Input language -> output language
            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 8
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                spacing: 8
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    spacing: 0
                    Label { text: qsTr("From"); font.pixelSize: 12; opacity: 0.6 }
                    ComboBox {
                        objectName: "fromLanguage"
                        Layout.fillWidth: true
                        model: page.languages
                        textRole: "name"
                        currentIndex: page.languages.findIndex(l => l.code === page.from)
                        onActivated: (i) => page.setFrom(page.languages[i].code)
                    }
                }
                RoundButton {
                    Layout.alignment: Qt.AlignBottom
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
                    onClicked: page.swap()
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    spacing: 0
                    Label { text: qsTr("To"); font.pixelSize: 12; opacity: 0.6 }
                    ComboBox {
                        objectName: "toLanguage"
                        Layout.fillWidth: true
                        model: page.languages
                        textRole: "name"
                        currentIndex: page.languages.findIndex(l => l.code === page.to)
                        onActivated: (i) => page.setTo(page.languages[i].code)
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
                        typing.restart()
                    }
                    onAccepted: { typing.stop(); page.lookup(); page.dismissKeyboard() }
                }
                SpeakButton {
                    visible: page.from !== "fa"
                    speakText: field.text
                    languageTag: page.tag(page.from)
                }
            }

            // Suggestions while typing (word pack + Wiktionary), like a search box
            WordSuggestions {
                id: wordSuggestions
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                text: field.text
                language: page.from
                onPicked: (entry) => {
                    field.text = entry.word
                    typing.stop()
                    page.lookup()
                    page.dismissKeyboard()
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
                            text: page.reverse ? (page.headword || page.query) : page.cardFront || page.query
                            wrapMode: Text.WordWrap
                            font.pixelSize: 22
                            font.bold: true
                            horizontalAlignment: page.persianText(text) ? Text.AlignRight : Text.AlignLeft
                        }
                        GenderMark { word: page.reverse ? "" : page.cardFront }
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
                            text: page.cardBody
                            pixelSize: 20
                        }
                        SpeakButton {
                            Layout.alignment: Qt.AlignTop
                            visible: page.to !== "fa" && !page.persianText(page.result)
                            speakText: page.result.split("\n")[0]
                            languageTag: page.tag(page.to)
                            iconSize: 19
                            tint: "#e53935" // translation: red
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

                    // Like Lens: save straight into the learning box (asks if the word is already a card)
                    Button {
                        Layout.topMargin: 4
                        highlighted: true
                        visible: page.result !== "" && page.learnWord !== ""
                        text: qsTr("+ Add to “%1”").arg(CardStore.currentCollectionName)
                        onClicked: page.quickAdd(false)
                    }
                }
            }

            // ---- Forms and example sentences: the same block as in Add card ----
            WordDetails {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 8
                info: wordInfo
            }
            Item { Layout.preferredHeight: 16 }
        }
    }

    Dialog {
        id: existsDialog
        property var existing: ({})
        anchors.centerIn: parent
        width: Math.min(page.width - 32, 400)
        modal: true
        title: qsTr("Already in your box")
        ColumnLayout {
            anchors.fill: parent
            spacing: 8
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                font.bold: true
                text: existsDialog.existing.front ?? ""
            }
            MeaningText {
                Layout.fillWidth: true
                text: existsDialog.existing.back ?? ""
                visible: text !== ""
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                opacity: 0.7
                text: (existsDialog.existing.box ?? 1) > 5
                      ? BoxNames.name(6)
                      : qsTr("%1 · reviewed %2×").arg(BoxNames.name(existsDialog.existing.box ?? 1)).arg(existsDialog.existing.reviews ?? 0)
            }
            Label {
                Layout.fillWidth: true
                Layout.topMargin: 4
                wrapMode: Text.WordWrap
                text: qsTr("Update it with this meaning and sentence? Its box and progress are kept.")
            }
            Button {
                Layout.fillWidth: true
                highlighted: true
                text: qsTr("Update existing card")
                onClicked: { existsDialog.close(); page.quickAdd(true) }
            }
            Button {
                Layout.fillWidth: true
                text: qsTr("Open existing card")
                onClicked: {
                    existsDialog.close()
                    page.StackView.view.push(editPage, { cardId: existsDialog.existing.id })
                }
            }
            Button {
                Layout.fillWidth: true
                flat: true
                text: qsTr("Cancel")
                onClicked: existsDialog.close()
            }
        }
    }

    ToolTip {
        id: toast
        function show(msg) { text = msg; open() }
        timeout: 2000
        x: (parent.width - width) / 2
        y: parent.height / 2
    }
}
