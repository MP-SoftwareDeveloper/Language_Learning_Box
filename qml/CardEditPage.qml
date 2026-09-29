import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Controls.Material
import QtQuick.Dialogs
import LearningBox

// Add a new card (cardId < 0) or edit an existing one.
Page {
    id: page
    property int cardId: -1
    readonly property bool isNew: cardId < 0
    property var original: ({})
    // Picture file name as it will be saved; "" = no picture.
    property string image: ""
    property bool saved: false
    // Prefill for a new card (e.g. text picked with the camera)
    property string initialFront: ""
    property string initialBack: ""
    property string initialExample: ""
    // Box a new card starts in (1..5, 6 = Learned); e.g. preset by the Box page's "+"
    property int initialBox: 1

    // ---- Suggestions while typing a new card ----
    // Word list (word pack), meaning filled in automatically (word pack for Persian, saved or
    // online translation) and example sentences (word pack + Tatoeba when online).
    readonly property string typed: (frontField.text + frontField.preeditText).trim()
    property bool backAuto: true        // back is empty or was filled by the app: keep filling it
    property bool settingBack: false
    property int meaningRequest: -1
    property string meaningFor: ""
    property var wordSuggestions: []
    property var exampleSuggestions: [] // [{text, translation}]
    property int examplesRequest: -1
    property bool pickedWord: false

    function setBack(t) { settingBack = true; backField.text = t; settingBack = false }

    onTypedChanged: {
        if (!isNew || !AppMode.full) // Simple app: no suggestions
            return
        if (pickedWord) {
            pickedWord = false
            wordSuggestions = []
        } else {
            // The word pack is German: no pack suggestions in English learning boxes
            wordSuggestions = typed.length >= 2 && CardStore.learningLanguage === "de" ? WordPacks.suggest(typed, 5) : []
        }
        suggestTimer.restart()
    }
    Timer {
        id: suggestTimer
        interval: 700 // wait until typing pauses
        onTriggered: { page.fillMeaning(); page.findExamples() }
    }

    function pickWord(s) {
        pickedWord = true
        frontField.text = s.front
        wordSuggestions = []
        if (backAuto && Translator.meaningLanguage === "fa" && s.back)
            setBack(s.back)
        if (exampleField.text.trim() === "" && s.example)
            exampleField.text = s.example
        suggestTimer.restart()
    }
    function fillMeaning() {
        if (!isNew || !backAuto)
            return
        const w = typed
        meaningRequest = -1
        if (w === "") {
            setBack("")
            return
        }
        if (Translator.meaningLanguage === "fa" && CardStore.learningLanguage === "de") {
            const known = WordPacks.lookup(w)
            if (known.back) {
                setBack(known.back)
                return
            }
        }
        const saved = Translator.saved(w)
        if (saved !== "") {
            setBack(saved)
        } else if (Translator.useOnline) {
            meaningFor = w
            meaningRequest = Translator.translate(w)
        }
    }
    function findExamples() {
        exampleSuggestions = []
        examplesRequest = -1
        if (!isNew || typed.length < 2 || exampleField.text.trim() !== "")
            return
        const persian = Translator.meaningLanguage === "fa"
        exampleSuggestions = CardStore.learningLanguage !== "de" ? []
            : WordPacks.examplesContaining(typed, 3).map(e => ({ text: e.text, translation: persian ? e.translation : "" }))
        if (Translator.useOnline)
            examplesRequest = Translator.suggestExamples(typed)
    }
    Connections {
        target: Translator
        function onTranslated(requestId, text, alternatives) {
            if (requestId !== page.meaningRequest)
                return
            page.meaningRequest = -1
            if (page.backAuto && page.typed === page.meaningFor && text !== "") {
                const sep = Translator.rightToLeft ? "\u060C " : ", "
                const more = (alternatives ?? []).slice(0, 2)
                page.setBack(more.length > 0 ? text + sep + more.join(sep) : text)
            }
        }
        function onExamplesSuggested(requestId, examples) {
            if (requestId !== page.examplesRequest)
                return
            page.examplesRequest = -1
            const merged = page.exampleSuggestions.slice()
            const seen = merged.map(e => e.text.toLowerCase())
            for (const e of examples)
                if (merged.length < 5 && seen.indexOf(e.text.toLowerCase()) < 0)
                    merged.push(e)
            page.exampleSuggestions = merged
        }
    }

    title: isNew ? qsTr("Add card") : qsTr("Edit card")

    readonly property int duplicateId: {
        const id = CardStore.findByFront(frontField.text)
        return id === cardId ? -1 : id
    }

    Component.onCompleted: {
        if (!isNew) {
            original = CardStore.card(cardId)
            frontField.text = original.front ?? ""
            backField.text = original.back ?? ""
            exampleField.text = original.example ?? ""
            image = original.image ?? ""
            boxChoice.currentIndex = Math.max(0, Math.min(5, (original.box ?? 1) - 1))
        } else if (initialFront !== "") {
            frontField.text = initialFront
            backField.text = initialBack
            exampleField.text = initialExample
            addAnother.checked = false
            backField.forceActiveFocus()
        } else {
            // Only auto-focus for new cards: focusing a field makes Qt query the
            // clipboard, which Android reports with a "pasted from clipboard" toast.
            frontField.forceActiveFocus()
        }
    }

    // Replace the pending picture; a picture picked in this editor but not kept is deleted
    // (CardStore.discardImage never deletes a file that a saved card still uses).
    function setImage(name) {
        if (image !== "" && image !== (original.image ?? ""))
            CardStore.discardImage(image)
        image = name
    }

    // Leaving without saving: drop a newly picked picture.
    StackView.onRemoved: if (!saved && image !== (original.image ?? "")) CardStore.discardImage(image)

    function save() {
        // New card whose word is already in the box: ask instead of creating a duplicate.
        if (isNew && duplicateId >= 0) {
            duplicateDialog.existing = CardStore.card(duplicateId)
            duplicateDialog.open()
            return
        }
        let ok
        if (isNew) {
            const newId = CardStore.addCard(frontField.text, backField.text, exampleField.text, image)
            ok = newId >= 0
            // Chosen box other than 1: move it there, scheduled with that box's interval.
            if (ok && boxChoice.currentIndex > 0)
                CardStore.moveCard(newId, boxChoice.currentIndex + 1)
        } else {
            ok = CardStore.updateCard(cardId, frontField.text, backField.text, exampleField.text, image)
            // Box changed by hand: move it there, scheduled with that box's interval.
            if (ok && boxChoice.currentIndex + 1 !== Math.min(6, original.box ?? 1))
                CardStore.moveCard(cardId, boxChoice.currentIndex + 1)
        }
        if (!ok)
            return
        finishSave()
    }

    // Overwrite the existing card with what is typed here. Its box and review history stay;
    // an empty field or no new picture keeps the existing value.
    function updateExisting() {
        const e = duplicateDialog.existing
        const newImage = image !== "" ? image : (e.image ?? "")
        const ok = CardStore.updateCard(e.id,
                                        frontField.text,
                                        backField.text.trim() !== "" ? backField.text : (e.back ?? ""),
                                        exampleField.text.trim() !== "" ? exampleField.text : (e.example ?? ""),
                                        newImage)
        if (ok)
            finishSave()
    }

    function finishSave() {
        saved = true
        if (isNew && addAnother.checked) {
            savedHint.show(frontField.text.trim())
            frontField.clear(); backField.clear(); exampleField.clear()
            page.backAuto = true
            page.wordSuggestions = []
            page.exampleSuggestions = []
            image = ""
            saved = false
            frontField.forceActiveFocus()
        } else {
            page.StackView.view.pop()
        }
    }

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth

        ColumnLayout {
            width: parent.width
            spacing: 12

            Item { Layout.preferredHeight: 8 }

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 8
                TextField {
                    id: frontField
                    Layout.fillWidth: true
                    placeholderText: CardStore.learningLanguage === "en" ? qsTr("English word or sentence")
                                                                         : qsTr("German word or sentence")
                    onAccepted: backField.forceActiveFocus()
                }
                SpeakButton { speakText: frontField.text }
            }
            Label {
                Layout.leftMargin: 16
                visible: page.duplicateId >= 0
                color: Material.color(Material.Orange)
                text: page.isNew ? qsTr("This card already exists. Saving lets you update it.")
                                 : qsTr("Another card already has this word.")
            }

            // Matching words from the word pack: tap to fill in word, meaning and example
            Flow {
                Layout.fillWidth: true
                Layout.leftMargin: 12
                Layout.rightMargin: 12
                spacing: 4
                visible: page.isNew && page.wordSuggestions.length > 0
                Repeater {
                    model: page.wordSuggestions
                    delegate: Button {
                        required property var modelData
                        flat: true
                        font.capitalization: Font.MixedCase
                        font.pixelSize: 15
                        text: modelData.front
                        onClicked: page.pickWord(modelData)
                    }
                }
            }

            // Persian, English or anything else. Qt resolves bidi direction per paragraph.
            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 8
                TextArea {
                    id: backField
                    Layout.fillWidth: true
                    Layout.minimumHeight: 80
                    wrapMode: TextEdit.Wrap
                    placeholderText: qsTr("Meaning / notes")
                    // Set the alignment explicitly from the text direction: Material places the floating
                    // label by `horizontalAlignment` but cuts the border gap by the effective (text-direction)
                    // alignment, so with Persian text the label was drawn through the border line.
                    horizontalAlignment: /^[^A-Za-z\u00C0-\u024F\u0600-\u06FF\uFB50-\uFEFF]*[\u0600-\u06FF\uFB50-\uFEFF]/.test(text)
                                         ? TextEdit.AlignRight : TextEdit.AlignLeft
                    onTextChanged: if (!page.settingBack) page.backAuto = text.trim() === ""
                }
                // English / German meaning can be listened to (first line); no Persian voice
                SpeakButton {
                    Layout.alignment: Qt.AlignTop
                    visible: Translator.meaningLanguage !== "fa" && !/[\u0600-\u06FF]/.test(backField.text)
                    speakText: backField.text.split("\n")[0]
                    languageTag: Translator.meaningLanguage === "en" ? "en-US" : "de-DE"
                }
            }
            Label {
                Layout.leftMargin: 20
                Layout.topMargin: -8
                visible: page.isNew && (page.meaningRequest >= 0 || (page.backAuto && backField.text !== ""))
                font.pixelSize: 12
                opacity: 0.6
                text: page.meaningRequest >= 0 ? qsTr("Looking up the meaning\u2026")
                                               : qsTr("Meaning filled in automatically \u2013 edit it if you like")
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 8
                TextArea {
                    id: exampleField
                    Layout.fillWidth: true
                    Layout.minimumHeight: 60
                    wrapMode: TextEdit.Wrap
                    placeholderText: qsTr("Example sentence")
                }
                SpeakButton { speakText: exampleField.text }
            }
            // Persian translation of the example (when Persian is selected in Settings)
            ExampleText {
                Layout.fillWidth: true
                Layout.leftMargin: 20
                Layout.rightMargin: 56
                Layout.topMargin: -8
                example: exampleField.text
                showGerman: false
                pixelSize: 15
            }

            // Example sentences with the word: word pack + Tatoeba (online). Tap to use one.
            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                spacing: 2
                visible: page.isNew && exampleField.text.trim() === ""
                         && (page.exampleSuggestions.length > 0 || page.examplesRequest >= 0)
                Label {
                    font.pixelSize: 12
                    opacity: 0.6
                    text: page.examplesRequest >= 0 && page.exampleSuggestions.length === 0
                          ? qsTr("Finding example sentences\u2026") : qsTr("Example suggestions \u2013 tap to use")
                }
                Repeater {
                    model: page.exampleSuggestions
                    delegate: ItemDelegate {
                        required property var modelData
                        Layout.fillWidth: true
                        topPadding: 6
                        bottomPadding: 6
                        contentItem: ColumnLayout {
                            spacing: 1
                            Label {
                                Layout.fillWidth: true
                                text: modelData.text
                                wrapMode: Text.WordWrap
                                font.italic: true
                            }
                            Label {
                                Layout.fillWidth: true
                                visible: text !== ""
                                text: modelData.translation
                                wrapMode: Text.WordWrap
                                font.pixelSize: 13
                                opacity: 0.7
                                horizontalAlignment: Translator.rightToLeft ? Text.AlignRight : Text.AlignLeft
                            }
                        }
                        onClicked: {
                            if (modelData.translation)
                                Translator.remember(modelData.text, modelData.translation)
                            exampleField.text = modelData.text
                            page.exampleSuggestions = []
                        }
                    }
                }
            }

            // ---- Picture ----
            Image {
                id: preview
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: Math.min(page.width - 32, 320)
                Layout.preferredHeight: visible ? Math.min(implicitHeight * width / Math.max(1, implicitWidth), 240) : 0
                visible: page.image !== ""
                source: CardStore.imageUrl(page.image)
                fillMode: Image.PreserveAspectFit
                asynchronous: true
                cache: false
            }
            Flow {
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.fillWidth: true
                spacing: 8
                visible: AppMode.full // pictures are part of the Full app
                Button {
                    text: qsTr("\uD83D\uDCF7 Take photo")
                    onClicked: {
                        const cam = page.StackView.view.push(cameraShotPage)
                        cam.taken.connect(name => page.setImage(name))
                    }
                }
                Button {
                    text: page.image === "" ? qsTr("Add picture") : qsTr("Change picture")
                    onClicked: imageDialog.open()
                }
                Button {
                    flat: true
                    visible: page.image !== ""
                    text: qsTr("Remove picture")
                    Material.foreground: Material.color(Material.Red)
                    onClicked: page.setImage("")
                }
            }
            Label {
                id: imageError
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.fillWidth: true
                visible: text.length > 0
                wrapMode: Text.WordWrap
                color: Material.color(Material.Red)
            }

            // Which box a new card goes into
            RowLayout {
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Label { text: qsTr("Box") }
                ComboBox {
                    id: boxChoice
                    Layout.preferredWidth: 160
                    model: [qsTr("Box 1"), qsTr("Box 2"), qsTr("Box 3"), qsTr("Box 4"), qsTr("Box 5"), qsTr("Learned")]
                    currentIndex: Math.max(0, Math.min(5, page.initialBox - 1))
                }
            }

            CheckBox {
                id: addAnother
                Layout.leftMargin: 8
                visible: page.isNew
                checked: true
                text: qsTr("Add another after saving")
            }

            Button {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                highlighted: true
                enabled: frontField.text.trim().length > 0 && (page.isNew || page.duplicateId < 0)
                text: page.isNew && page.duplicateId >= 0 ? qsTr("Save…") : qsTr("Save")
                onClicked: page.save()
            }

            // Edit-only actions
            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                visible: !page.isNew
                Label {
                    Layout.fillWidth: true
                    opacity: 0.7
                    text: page.original.box > 5 ? qsTr("Learned")
                                                : qsTr("Box %1 · reviewed %2×").arg(page.original.box).arg(page.original.reviews)
                }
                Button {
                    flat: true
                    text: qsTr("Reset to box 1")
                    onClicked: { CardStore.resetCard(page.cardId); page.original = CardStore.card(page.cardId) }
                }
                Button {
                    flat: true
                    text: qsTr("Delete")
                    Material.foreground: Material.color(Material.Red)
                    onClicked: confirmDelete.open()
                }
            }
            Item { Layout.preferredHeight: 16 }
        }
    }

    // Android: opens the system photo/file picker (no extra permission needed).
    Component { id: cameraShotPage; CameraShotPage {} }

    FileDialog {
        id: imageDialog
        title: qsTr("Choose a picture")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("Images (*.jpg *.jpeg *.png *.webp *.gif *.bmp)")]
        onAccepted: {
            const name = CardStore.importImage(selectedFile)
            if (name === "") {
                imageError.text = qsTr("This picture could not be loaded. Please choose a JPG or PNG image.")
            } else {
                imageError.text = ""
                page.setImage(name)
            }
        }
    }

    Dialog {
        id: duplicateDialog
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
                text: duplicateDialog.existing.front ?? ""
            }
            MeaningText {
                Layout.fillWidth: true
                text: duplicateDialog.existing.back ?? ""
                visible: text !== ""
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                opacity: 0.7
                text: (duplicateDialog.existing.box ?? 1) > 5
                      ? qsTr("Learned")
                      : qsTr("Box %1 · reviewed %2×").arg(duplicateDialog.existing.box ?? 1).arg(duplicateDialog.existing.reviews ?? 0)
            }
            Label {
                Layout.fillWidth: true
                Layout.topMargin: 4
                wrapMode: Text.WordWrap
                text: qsTr("Update it with what you typed here? Its box and progress are kept.")
            }
            Button {
                Layout.fillWidth: true
                highlighted: true
                text: qsTr("Update existing card")
                onClicked: { duplicateDialog.close(); page.updateExisting() }
            }
            Button {
                Layout.fillWidth: true
                text: qsTr("Open existing card")
                onClicked: {
                    duplicateDialog.close()
                    page.StackView.view.replace(page, Qt.resolvedUrl("CardEditPage.qml"),
                                                { cardId: duplicateDialog.existing.id })
                }
            }
            Button {
                Layout.fillWidth: true
                flat: true
                text: qsTr("Cancel")
                onClicked: duplicateDialog.close()
            }
        }
    }

    Dialog {
        id: confirmDelete
        anchors.centerIn: parent
        modal: true
        title: qsTr("Delete this card?")
        standardButtons: Dialog.Yes | Dialog.No
        onAccepted: {
            page.saved = true // removeCard deletes the stored picture itself
            CardStore.removeCard(page.cardId)
            page.StackView.view.pop()
        }
    }

    ToolTip {
        id: savedHint
        function show(word) { text = qsTr("Saved: %1").arg(word); open() }
        timeout: 1500
        x: (parent.width - width) / 2
        y: parent.height - height - 24
    }
}
