import QtCore
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Controls.Material
import LearningBox

// Dictionary: look up a word or sentence between any two languages (Deutsch, English, فارسی), picked in
// two combo boxes at the top: the input language and the output language (remembered). The default is the
// learning language of the selected box and its meaning language. Cards can be added when one of the two
// languages is the card language: the selected box's language, except that German is always a German card
// (cardLang), added to a German box whichever box is selected (targetBox).
// Word pack (German → Persian, offline), saved translations, online translation (Full app),
// example sentences, 🔊 for German / English, and "Add to learning box".
Page {
    id: page
    title: qsTr("Dictionary")

    readonly property string learn: CardStore.learningLanguage   // "de" / "en" / "sv"
    // Language of the card the Dictionary makes: the learning box's language, except for German. A German word is
    // always looked up as a German card (article, plural, German examples), also while an English or Swedish box
    // is selected or the German boxes are switched off in Settings; the card goes into a German box (targetBox).
    readonly property string cardLang: from === learn ? learn : ((from === "de" || to === "de") ? "de" : learn)
    readonly property string boxMeaning: Translator.meaningLanguage // "fa" / "en" / "de"
    readonly property var languages: [
        { code: "de", name: "Deutsch" }, { code: "en", name: "English" }, { code: "sv", name: "Svenska" }, { code: "fa", name: "فارسی" }
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
        property string saveBoxes: "{}" // JSON {"de": box id, ...}: the box chosen in "Save in", per language
    }
    // learn → other language (the card's front is the typed word) / other language → learn (the front is the answer)
    // / free: neither is the learning language (no cards, no grammar block)
    readonly property bool reverse: from !== cardLang && to === cardLang
    readonly property bool free: from !== cardLang && to !== cardLang
    // The language the meaning (card back) is in
    readonly property string meaning: reverse ? from : to

    function langName(c) { return c === "fa" ? "فارسی" : c === "de" ? "Deutsch" : c === "sv" ? "Svenska" : "English" }
    function tag(c) { return c === "en" ? "en-US" : c === "de" ? "de-DE" : c === "sv" ? "sv-SE" : "" }
    function persianText(t) { return /[؀-ۿ]/.test(t) }

    property string query: ""        // what was looked up
    property string headword: ""     // pack form with article, e.g. "das Brot"
    property string result: ""
    property var alternatives: []
    property string source: ""       // "pack", "online", "saved"
    property string error: ""
    property int requestId: -1
    readonly property string chosenExample: wordInfo.chosen // sentences that go on the card, one per line (check boxes)
    // Sentences typed by the user in the box above "Add", one per line
    readonly property string ownExample: ownExampleField.text.split("\n").map(l => l.trim()).filter(l => l !== "").join("\n")
    // What goes on the card: the ticked suggestions, then the user's own sentences (no duplicates)
    readonly property string cardExample: {
        const lines = chosenExample !== "" ? chosenExample.split("\n") : []
        for (const l of ownExample.split("\n"))
            if (l !== "" && lines.indexOf(l) < 0)
                lines.push(l)
        return lines.join("\n")
    }

    // Bumped whenever boxes, cards or favorites change: the bindings below that use it look again. (Each one uses
    // the number in its expression: a binding that only "reads" a property can be optimised away, see StarButton.)
    property int revision: 0
    Connections {
        target: CardStore
        function onChanged() { page.revision++ }
        function onFavoritesChanged() { page.revision++ }
    }

    // The learning box a card goes to. The user can choose among the boxes of the card's language ("Save in", also
    // the ones the box list hides, Settings); the choice is remembered per language. Without a choice: the selected
    // box, or - when the Dictionary works in another language than the selected box (German words while an English
    // box is selected) - the most recently used box of that language. With no such box, "Add" creates one.
    readonly property bool otherBox: cardLang !== learn
    readonly property var boxChoices: revision < 0 ? [] : CardStore.collectionsFor(cardLang) // [{id, name}]
    function savedBox(language) {
        try {
            return JSON.parse(prefs.saveBoxes)[language] ?? -1
        } catch (e) {
            return -1
        }
    }
    function chooseBox(id) {
        let all = ({})
        try {
            all = JSON.parse(prefs.saveBoxes)
        } catch (e) {
        }
        all[cardLang] = id
        prefs.saveBoxes = JSON.stringify(all)
    }
    readonly property int chosenBox: {
        const id = savedBox(cardLang)
        return boxChoices.some(b => b.id === id) ? id : -1 // -1: no choice, or that box is gone
    }
    readonly property int targetBox: chosenBox >= 0 ? chosenBox
        : revision < 0 ? -1 : (otherBox ? CardStore.latestCollection(cardLang) : CardStore.currentCollection)
    readonly property string targetName: revision < 0 ? ""
        : targetBox >= 0 ? CardStore.collectionName(targetBox) : qsTr("My German words")
    function targetId() {
        if (targetBox >= 0)
            return targetBox
        const created = CardStore.createCollection(qsTr("My German words"), cardLang)
        if (created >= 0 && !LevelPacks.germanBoxes)
            CardStore.setCollectionHidden(created, true) // German boxes are switched off (Settings): not in the list
        return created
    }

    // The word as a card in the target box (-1: not a card there), and whether it is a favorite (★)
    readonly property int cardId: {
        const front = cardFront.trim()
        return revision < 0 || targetBox < 0 || front === "" ? -1 : CardStore.findByFrontIn(targetBox, front)
    }
    readonly property bool starred: revision >= 0 && cardId >= 0 && CardStore.isFavorite(cardId)
    // ★ tapped: a card is starred / unstarred; a word that is not a card yet is added to the target box, starred
    function toggleFavorite() {
        const front = cardFront.trim()
        if (front === "")
            return
        if (cardId >= 0) {
            CardStore.setFavorite(cardId, !starred)
            return
        }
        const box = targetId()
        const id = box >= 0 ? CardStore.addCardIn(box, front, cardBack, cardExample) : -1
        if (id < 0) {
            toast.show(qsTr("Could not save"))
            return
        }
        CardStore.setFavorite(id, true)
        ownExampleField.clear()
        toast.show(qsTr("Card added to “%1” as a favorite").arg(CardStore.collectionName(box)))
    }

    // Save the looked-up word as a card in its learning box (targetBox), with the chosen sentence.
    // A word that is already a card is only touched when the user says so (updateExisting).
    // Also works without a translation (nothing found, offline, Google blocked): the card is saved with an
    // empty meaning, to be filled in later. Looked up from the meaning side with no German found, the card
    // editor opens with what was typed as the meaning.
    function quickAdd(updateExisting) {
        const front = cardFront.trim()
        if (front === "" && reverse && query !== "") {
            if (targetBox !== CardStore.currentCollection) { // the editor adds to the selected box only
                toast.show(qsTr("No word found to add"))
                return
            }
            page.StackView.view.push(editPage, { initialBack: query, initialExample: cardExample })
            return
        }
        if (front === "")
            return
        const box = targetId()
        if (box < 0) {
            toast.show(qsTr("Could not save"))
            return
        }
        const id = CardStore.findByFrontIn(box, front)
        if (id >= 0 && !updateExisting) {
            existsDialog.existing = CardStore.card(id)
            existsDialog.open()
            return
        }
        if (id >= 0) {
            const c = CardStore.card(id)
            const ok = CardStore.updateCard(id, c.front, cardBack !== "" ? cardBack : c.back,
                                            cardExample !== "" ? cardExample : (c.example ?? ""), c.image ?? "")
            toast.show(ok ? qsTr("Card updated") : qsTr("Could not save"))
            if (ok)
                ownExampleField.clear()
        } else {
            const added = CardStore.addCardIn(box, front, cardBack, cardExample) >= 0
            toast.show(added ? (cardBack === "" ? qsTr("Card added without a meaning")
                                                : box !== CardStore.currentCollection
                                                  ? qsTr("Card added to “%1”").arg(CardStore.collectionName(box))
                                                           : qsTr("Card added"))
                             : qsTr("Could not save"))
            if (added)
                ownExampleField.clear()
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

    // The "Add to learning box" button (and the own-example box) is shown
    readonly property bool canAdd: !free && requestId < 0
                                   && (learnWord !== "" || (reverse && query !== ""))

    // An ambiguous spelling (Berichten: noun or verb) is translated as the word the user chose, see WordLookup
    property string meaningQuery: ""
    property bool inLookup: false
    function requestMeaning() {
        const q = from === cardLang && wordInfo.meaningText !== "" ? wordInfo.meaningText : query
        meaningQuery = q
        requestId = Translator.translateBetween(q, from, to)
    }

    function clearResult() {
        headword = ""; result = ""; alternatives = []; source = ""; error = ""
        requestId = -1
    }
    function lookup() {
        inLookup = true // the word changes below: not an "ambiguous spelling" re-translation, see wordInfo
        clearResult()
        if (field.text.trim() !== query)
            ownExampleField.clear() // a different word: the typed sentences belonged to the old one
        query = field.text.trim()
        suggestions = []
        if (query === "") {
            inLookup = false
            return
        }
        // Offline first: the German word pack with its curated Persian meaning
        if (from === cardLang && cardLang === "de" && to === "fa") {
            const known = WordPacks.lookup(query)
            if (known.back) {
                headword = known.front
                result = known.back
                source = "pack"
            }
        }
        // The German level lists (A1 to C1) have a curated first meaning, better than the translator's guess for a
        // lone word ("Satz" -> "Set", not "sentence"); what the translator finds follows as "Also"
        if (source === "" && from === cardLang && cardLang === "de" && (to === "en" || to === "fa")) {
            const curated = LevelPacks.germanMeaning(query, to)
            if (curated.back) {
                headword = curated.front
                result = curated.back
                source = "pack"
            }
        }
        requestMeaning() // online, else saved
        wordInfo.refreshNow() // article, forms and example sentences of the word
        inLookup = false
    }

    Connections {
        target: Translator
        function onTranslated(requestId, text, alternatives, source, error) {
            if (requestId !== page.requestId)
                return
            page.requestId = -1
            page.error = error ?? ""
            if (text !== "") {
                text = wordInfo.meaningFix(text)
                alternatives = (alternatives ?? []).map(a => wordInfo.meaningFix(a))
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
        lang: page.cardLang
        onMeaningTextChanged: {
            // The word turned out to be two (noun / verb), or the choice changed: translate the chosen one
            if (page.inLookup || page.reverse || page.free || page.query === "" || page.source === "pack"
                    || page.from !== page.cardLang || meaningText === page.meaningQuery || word.trim() !== page.query)
                return
            page.result = ""; page.alternatives = []; page.source = ""
            page.requestMeaning()
        }
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
            for (const f of [field, ownExampleField, keysField, keysOwn]) {
                const q = f.mapFromItem(null, scenePoint.x, scenePoint.y)
                if (q.x >= 0 && q.y >= 0 && q.x <= f.width && q.y <= f.height)
                    return
            }
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

            // ä ö ü ß under the box (Settings → Keyboard), when the input language is German
            GermanKeys {
                id: keysField
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                target: field
                forLanguage: page.from === "de" || page.from === "sv"
                letterSet: page.from === "sv" ? "sv" : "de"
                onInserted: typing.restart()
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
                        // ★ Favorite words: for a card in the target box; a word that is not a card yet is added, starred
                        ToolButton {
                            visible: page.canAdd && page.cardFront.trim() !== ""
                            focusPolicy: Qt.NoFocus
                            implicitWidth: 40
                            contentItem: Item {
                                implicitWidth: 24
                                implicitHeight: 24
                                StarIcon {
                                    anchors.centerIn: parent
                                    width: 24
                                    height: 24
                                    filled: page.starred
                                    color: page.starred ? "#F5B301" : Material.foreground
                                    opacity: page.starred ? 1 : 0.6
                                }
                            }
                            ToolTip.visible: hovered
                            ToolTip.text: page.starred ? qsTr("Remove from Favorite words") : qsTr("Add to Favorite words")
                            onClicked: page.toggleFavorite()
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

                    // Own example sentences for the card (in addition to the ticked suggestions below)
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.topMargin: 4
                        visible: page.canAdd
                        TextArea {
                            id: ownExampleField
                            Layout.fillWidth: true
                            Layout.minimumHeight: 60
                            wrapMode: TextEdit.Wrap
                            placeholderText: qsTr("Your own example sentences (one per line)")
                        }
                        SpeakButton {
                            Layout.alignment: Qt.AlignTop
                            visible: page.cardLang !== "fa"
                            speakText: ownExampleField.text
                            languageTag: page.tag(page.cardLang)
                        }
                    }

                    GermanKeys {
                        id: keysOwn
                        Layout.fillWidth: true
                        target: ownExampleField
                        forLanguage: page.cardLang === "de" || page.cardLang === "sv"
                        letterSet: page.cardLang === "sv" ? "sv" : "de"
                    }

                    // Which learning box (of this language) the card goes to; the choice is remembered
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.topMargin: 4
                        visible: page.canAdd && page.boxChoices.length > 1
                        Label {
                            text: qsTr("Save in")
                            opacity: 0.7
                        }
                        ComboBox {
                            objectName: "saveBox"
                            Layout.fillWidth: true
                            model: page.boxChoices
                            textRole: "name"
                            currentIndex: page.boxChoices.findIndex(b => b.id === page.targetBox)
                            onActivated: (i) => page.chooseBox(page.boxChoices[i].id)
                        }
                    }

                    // Like Lens: save straight into the learning box (asks if the word is already a card)
                    Button {
                        Layout.topMargin: 4
                        highlighted: true
                        // also when nothing was found: the card is saved without a meaning (or, looked up from the
                        // meaning side, the editor opens with it)
                        visible: page.canAdd
                        text: page.result !== "" ? qsTr("+ Add to “%1”").arg(page.targetName)
                                                 : qsTr("+ Add without translation to “%1”").arg(page.targetName)
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
