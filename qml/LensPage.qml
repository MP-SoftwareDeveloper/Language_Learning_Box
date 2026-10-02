import QtCore
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Controls.Material
import QtQuick.Dialogs
import QtMultimedia
import LearningBox

// "Lens": take a photo (or pick one), recognize the German text, show it translated
// (Persian or English, see Settings), tap words or press-and-hold for a whole sentence,
// and add the selection to the box with its translation.
Page {
    id: page
    title: qsTr("Lens")
    background: Rectangle { color: page.mode === "camera" && engine.available ? "black" : Material.backgroundColor }

    // "camera" -> viewfinder, "result" -> recognized picture with word boxes
    property string mode: "camera"
    property var selected: []          // word indices
    readonly property alias ocr: engine
    readonly property string selectedText: engine.joinSelected(selected)
    readonly property var selectedList: selectedWords()

    // ---------- translations ----------
    // key (German text) -> {text, alternatives, source, pending}
    property var tr: ({})
    property var requests: ({})        // Translator request id -> key
    property string fullText: ""
    property bool showFull: true
    readonly property bool anyPending: selectedList.some(w => entry(w).pending === true)
                                       || (selected.length > 1 && entry(selectedText).pending === true)

    function norm(t) { return (t ?? "").trim().replace(/\s+/g, " ") }
    function entry(t) { return tr[norm(t)] ?? ({}) }
    function request(t) {
        const k = norm(t)
        // Skip what is pending or already translated; failed lookups are retried on the next request.
        if (k === "" || (tr[k] !== undefined && (tr[k].pending || tr[k].text))) return
        const next = Object.assign({}, tr)
        next[k] = { text: "", alternatives: [], source: "", pending: true }
        tr = next
        requests[Translator.translate(k)] = k
    }
    function resetTranslations() {
        tr = ({}); requests = ({})
        if (fullText !== "") request(fullText)
        for (const w of selectedList) request(w)
        if (selected.length > 1) request(selectedText)
    }
    // Translation to show for a word: translator result, else the word pack's Persian meaning.
    function meaningOf(w) {
        const e = entry(w)
        if (e.text) return e.text
        if (Translator.meaningLanguage === "fa") {
            const known = WordPacks.lookup(w)
            if (known.back) return known.back.split("\n")[0]
        }
        return ""
    }
    function sourceLabel(t) {
        const e = entry(t)
        if (e.pending) return qsTr("translating…")
        if (e.source === "online") return qsTr("online")
        if (e.source === "saved") return qsTr("saved")
        if (!Translator.useOnline) return qsTr("offline · no saved translation")
        return e.error ? qsTr("no connection · no saved translation") : qsTr("no translation")
    }
    // Back side for a word card. Persian: the word pack's curated meaning wins when it has one.
    function backFor(w) {
        const known = WordPacks.lookup(w)
        if (Translator.meaningLanguage === "fa" && known.back) return known.back
        const e = entry(w)
        let back = e.text ?? ""
        if (back !== "" && e.alternatives && e.alternatives.length > 0)
            back += (Translator.rightToLeft ? "، " : ", ") + e.alternatives.join(Translator.rightToLeft ? "، " : ", ")
        const grammar = known.back ? known.back.split("\n")[1] : undefined // pack's German grammar note
        if (back !== "" && grammar) back += "\n" + grammar
        return back // English with no translation: left empty for the user to fill in
    }

    // Card front a word would get (the pack's "das Brot" for "Brot").
    function frontFor(w) { return WordPacks.lookup(w).front ?? w }

    // "Add N words": new words are added; words already in the box are updated only when
    // the user says so (updateExisting), keeping their box and progress.
    function addSelectedWords(updateExisting) {
        const fresh = [], existing = []
        for (const w of selectedList) {
            const id = CardStore.findByFront(frontFor(w))
            if (id >= 0) existing.push({ id: id, word: w }); else fresh.push(w)
        }
        if (existing.length > 0 && updateExisting === false) {
            existsDialog.words = existing.map(e => frontFor(e.word))
            existsDialog.open()
            return
        }
        const added = WordPacks.addTranslatedWords(fresh.map(w => ({ word: w, back: backFor(w) })), qsTr("Lens"))
        let updated = 0
        if (updateExisting === true) {
            for (const e of existing) {
                const c = CardStore.card(e.id)
                const back = backFor(e.word)
                const example = WordPacks.lookup(e.word).example ?? ""
                if (CardStore.updateCard(e.id, c.front, back !== "" ? back : c.back,
                                         (c.example ?? "") !== "" ? c.example : example, c.image ?? ""))
                    ++updated
            }
        }
        const msgs = []
        if (added > 0) msgs.push(qsTr("%n card(s) added", "", added))
        if (updated > 0) msgs.push(qsTr("%n updated", "", updated))
        toast.show(msgs.length > 0 ? msgs.join(" · ") : qsTr("Already in your box"))
        selected = []
    }

    // ★ Favorite words: every selected word becomes a starred card (new words are added to the
    // learning box first, like "Add N words"). All already starred: the stars are removed.
    property bool selectionStarred: false
    function refreshStar() {
        let all = selectedList.length > 0
        for (const w of selectedList) {
            const id = CardStore.findByFront(frontFor(w))
            if (id < 0 || !CardStore.isFavorite(id)) { all = false; break }
        }
        selectionStarred = all
    }
    Connections {
        target: CardStore
        function onFavoritesChanged() { page.refreshStar() }
    }
    function starSelectedWords() {
        const on = !selectionStarred
        let added = 0, starred = 0
        for (const w of selectedList) {
            let id = CardStore.findByFront(frontFor(w))
            if (id < 0 && on) {
                added += WordPacks.addTranslatedWords([{ word: w, back: backFor(w) }], qsTr("Lens"))
                id = CardStore.findByFront(frontFor(w))
            }
            if (id >= 0 && CardStore.setFavorite(id, on))
                ++starred
        }
        if (!on)
            toast.show(qsTr("Removed from Favorite words"))
        else
            toast.show(qsTr("%n word(s) in Favorite words", "", starred)
                       + (added > 0 ? " · " + qsTr("%n card(s) added", "", added) : ""))
    }

    Connections {
        target: Translator
        function onTranslated(requestId, text, alternatives, source, error) {
            const k = page.requests[requestId]
            if (k === undefined) return
            delete page.requests[requestId]
            const next = Object.assign({}, page.tr)
            next[k] = { text: text, alternatives: alternatives, source: source, pending: false, error: error }
            page.tr = next
        }
        function onSettingsChanged() { page.resetTranslations() }
    }
    onSelectedListChanged: { for (const w of selectedList) request(w); refreshStar() }
    onSelectedTextChanged: if (selected.length > 1) request(selectedText)

    OcrEngine {
        id: engine
        onResultChanged: {
            page.selected = []
            const all = []
            for (let i = 0; i < engine.words.length; ++i) all.push(i)
            page.fullText = engine.joinSelected(all)
            page.showFull = true
            page.resetTranslations()
            if (engine.imageUrl.toString() !== "") {
                page.mode = "result"
                zoom.value = 1
            }
        }
    }

    CameraPermission { id: cameraPermission }

    // How the phone is held (accelerometer, works with rotation lock): a photo taken in
    // landscape is turned upright before OCR, like Google Lens.
    DeviceTilt {
        id: tilt
        active: cameraLoader.active
    }
    property int shotRotation: -1
    Component.onCompleted: {
        if (engine.available && cameraPermission.status !== Qt.PermissionStatus.Granted)
            cameraPermission.request()
    }

    // On some Android devices ImageCapture occasionally never reaches "ready" after the
    // camera is (re)created, leaving the shutter stuck disabled. Toggling this briefly
    // true forces the Loader below to destroy and recreate the whole camera session.
    property bool cameraKick: false
    StackView.onRemoved: engine.clear()
    // Leaving Lens (back, or on to the card editor): stop reading aloud
    StackView.onDeactivating: Speaker.stop()

    Component { id: editPage; CardEditPage {} }

    function isSelected(i) { return selected.indexOf(i) >= 0 }
    function toggle(i) {
        const s = selected.slice()
        const at = s.indexOf(i)
        if (at >= 0) s.splice(at, 1); else s.push(i)
        selected = s
    }
    // Press-and-hold: the sentence replaces the current selection.
    function selectSentence(i) {
        selected = engine.sentenceAt(i)
    }
    function selectedWords() {
        const list = []
        for (const i of selected.slice().sort((a, b) => a - b)) {
            const w = engine.cleanWord(engine.words[i].text)
            if (w.length > 0 && list.indexOf(w) < 0) list.push(w)
        }
        return list
    }

    // ---------- dependency / permission problems ----------
    ColumnLayout {
        anchors.centerIn: parent
        width: parent.width - 48
        spacing: 12
        visible: !engine.available

        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            font.pixelSize: 17
            font.bold: true
            text: qsTr("Text recognition is not available")
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: engine.unavailableReason
        }
        Button {
            text: qsTr("Download Tesseract OCR 5.5.2")
            onClicked: Qt.openUrlExternally("https://github.com/tesseract-ocr/tesseract/releases/tag/5.5.2")
        }
        Button {
            flat: true
            text: qsTr("German language data (deu.traineddata)")
            onClicked: Qt.openUrlExternally("https://github.com/tesseract-ocr/tessdata_fast/raw/main/deu.traineddata")
        }
        Button {
            flat: true
            visible: Qt.platform.os === "windows"
            text: qsTr("Tesseract installer for Windows")
            onClicked: Qt.openUrlExternally("https://github.com/UB-Mannheim/tesseract/wiki")
        }
    }

    // ---------- camera ----------
    Item {
        anchors.fill: parent
        visible: engine.available && page.mode === "camera"

        // The camera is re-created every time the viewfinder is shown. Re-activating one Camera
        // object on Android often leaves ImageCapture never "ready" again, so the second photo
        // could not be taken.
        Loader {
            id: cameraLoader
            anchors.fill: parent
            active: page.mode === "camera" && engine.available && page.visible
                    && cameraPermission.status === Qt.PermissionStatus.Granted
                    && !page.cameraKick
            onLoaded: readyWatchdog.restart()
            sourceComponent: Item {
                readonly property alias capture: imageCapture

                CaptureSession {
                    camera: Camera {
                        active: true
                        focusMode: Camera.FocusModeAutoNear
                        onErrorOccurred: (error, message) => status.text = message
                    }
                    imageCapture: ImageCapture {
                        id: imageCapture
                        // path is "/data/..." on Android, "C:/..." on Windows
                        onImageSaved: (id, path) => engine.recognize(Qt.url((path.startsWith("/") ? "file://" : "file:///") + path),
                                                                     page.shotRotation)
                        onErrorOccurred: (id, error, message) => status.text = message
                    }
                    videoOutput: viewfinder
                }

                VideoOutput {
                    id: viewfinder
                    anchors.fill: parent
                    fillMode: VideoOutput.PreserveAspectCrop
                }
            }
        }

        Label {
            anchors.centerIn: parent
            width: parent.width - 48
            visible: cameraPermission.status !== Qt.PermissionStatus.Granted
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: qsTr("Camera access is needed to photograph text.\nYou can also pick a photo from your gallery.")
        }

        Label {
            id: status
            anchors { top: parent.top; horizontalCenter: parent.horizontalCenter; topMargin: 12 }
            padding: 8
            color: "white"
            background: Rectangle { color: "#99000000"; radius: 6 }
            text: qsTr("Hold the text flat and well lit")
        }
        // Landscape shots are turned upright automatically; say so, readable the way the phone is held.
        Label {
            anchors.centerIn: parent
            visible: tilt.available && tilt.rotation !== 0
            rotation: (360 - tilt.rotation) % 360
            padding: 8
            color: "white"
            background: Rectangle { color: "#99000000"; radius: 6 }
            text: qsTr("\u21BB Landscape \u2013 the photo will be turned upright")
            Behavior on rotation { RotationAnimation { duration: 200; direction: RotationAnimation.Shortest } }
        }

        RowLayout {
            anchors { bottom: parent.bottom; horizontalCenter: parent.horizontalCenter; bottomMargin: 24 }
            spacing: 40

            RoundButton {
                text: "🖼" // gallery
                font.pixelSize: 22
                Layout.preferredWidth: 56; Layout.preferredHeight: 56
                onClicked: galleryDialog.open()
            }
            RoundButton {
                id: shutter
                Layout.preferredWidth: 76; Layout.preferredHeight: 76
                readonly property var capture: cameraLoader.item ? cameraLoader.item.capture : null
                enabled: capture !== null && capture.readyForCapture && !engine.busy
                Material.background: "white"
                contentItem: Rectangle { radius: width / 2; color: shutter.enabled ? Material.accent : "#9e9e9e"; anchors.margins: 6 }
                onClicked: {
                    status.text = qsTr("Hold the text flat and well lit")
                    // Tilt at the moment of the shot; -1 (detect from the picture) without a sensor.
                    page.shotRotation = tilt.available ? tilt.rotation : -1
                    capture.captureToFile(engine.captureFilePath())
                }
            }
            Item { Layout.preferredWidth: 56 }
        }

        // Self-healing: if the shutter is still disabled a few seconds after the camera
        // session was (re)created, restart the session once instead of leaving it stuck.
        Timer {
            id: readyWatchdog
            interval: 4000
            repeat: false
            onTriggered: {
                const cap = cameraLoader.item ? cameraLoader.item.capture : null
                if (cap && !cap.readyForCapture) {
                    status.text = qsTr("Camera is taking a moment — restarting it…")
                    page.cameraKick = true
                    Qt.callLater(() => page.cameraKick = false)
                }
            }
        }
    }

    // ---------- recognized page ----------
    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        visible: engine.available && page.mode === "result"

        Flickable {
            id: flick
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: content.width
            contentHeight: content.height
            boundsBehavior: Flickable.StopAtBounds

            QtObject { id: zoom; property real value: 1 }

            Item {
                id: content
                width: flick.width * zoom.value
                height: engine.imageWidth > 0 ? width * engine.imageHeight / engine.imageWidth : 0
                readonly property real scale: engine.imageWidth > 0 ? width / engine.imageWidth : 1

                Image {
                    anchors.fill: parent
                    source: engine.imageUrl
                    fillMode: Image.Stretch
                    asynchronous: true
                    cache: false
                }

                Repeater {
                    model: engine.words
                    delegate: Rectangle {
                        id: box
                        required property var modelData
                        readonly property bool on: page.isSelected(modelData.index)
                        x: modelData.x * content.scale - 2
                        y: modelData.y * content.scale - 2
                        width: modelData.w * content.scale + 4
                        height: modelData.h * content.scale + 4
                        radius: 3
                        // Unselected words: a light marker so you can see what was recognized.
                        color: on ? Qt.rgba(0, 0.59, 0.53, 0.35) : Qt.rgba(1, 0.92, 0.23, 0.22)
                        border.width: on ? 2 : 0
                        border.color: "#00796B"

                        TapHandler {
                            onTapped: page.toggle(box.modelData.index)
                            onLongPressed: page.selectSentence(box.modelData.index)
                        }
                    }
                }
            }

            PinchHandler {
                target: null
                property real startZoom: 1
                onActiveChanged: if (active) startZoom = zoom.value
                onActiveScaleChanged: zoom.value = Math.max(1, Math.min(4, startZoom * activeScale))
            }
        }

        // ---------- translation of the whole text ----------
        Pane {
            Layout.fillWidth: true
            visible: page.fullText !== ""
            padding: 0
            Material.elevation: 2

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                // The recognized text itself, read aloud in the learning language
                RowLayout {
                    objectName: "originalRow"
                    Layout.fillWidth: true
                    spacing: 0
                    Label {
                        Layout.fillWidth: true
                        leftPadding: 16
                        font.pixelSize: 13
                        elide: Text.ElideRight
                        text: qsTr("Text in the photo · %1").arg(CardStore.learningLanguage === "en" ? "English" : "Deutsch")
                    }
                    SpeakButton {
                        speakText: page.fullText
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 0
                ItemDelegate {
                    Layout.fillWidth: true
                    topPadding: 6; bottomPadding: 6
                    text: (page.showFull ? "\u2212  " : "+  ")
                          + qsTr("Translation · %1").arg(Translator.meaningLanguage === "fa" ? "فارسی"
                                                         : Translator.meaningLanguage === "de" ? "Deutsch" : "English")
                          + "  (" + page.sourceLabel(page.fullText) + ")"
                    font.pixelSize: 13
                    onClicked: {
                        page.showFull = !page.showFull
                        page.request(page.fullText) // retries if it failed before
                    }
                }
                // English / German translation read aloud (no Persian voice)
                SpeakButton {
                    visible: page.showFull && Translator.meaningLanguage !== "fa"
                    speakText: page.entry(page.fullText).text ?? ""
                    languageTag: Translator.meaningLanguage === "en" ? "en-US" : "de-DE"
                }
                }
                ScrollView {
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.min(fullLabel.implicitHeight + 12, page.height * 0.28)
                    visible: page.showFull
                    contentWidth: availableWidth
                    Label {
                        id: fullLabel
                        width: parent.width
                        leftPadding: 12; rightPadding: 12; bottomPadding: 8
                        wrapMode: Text.WordWrap
                        font.pixelSize: 15
                        horizontalAlignment: Translator.rightToLeft ? Text.AlignRight : Text.AlignLeft
                        text: page.entry(page.fullText).text
                              || (page.entry(page.fullText).pending ? "…"
                                  : Translator.useOnline
                                    ? qsTr("Could not reach the translation service (%1). Saved translations are used when available.").arg(page.entry(page.fullText).error || "?")
                                    : qsTr("No translation available offline for this text. Switch on online translation in Settings or pick single words."))
                        opacity: page.entry(page.fullText).text ? 1 : 0.6
                    }
                }
            }
        }

        // Which reader was used (only worth saying when online recognition is switched on)
        Label {
            Layout.fillWidth: true
            Layout.leftMargin: 12
            Layout.rightMargin: 12
            Layout.topMargin: 4
            visible: CloudOcr.enabled && page.mode === "result" && engine.source !== ""
            wrapMode: Text.WordWrap
            font.pixelSize: 12
            opacity: 0.7
            text: engine.source === "cloud"
                  ? qsTr("Read online (Azure AI Vision)")
                  : qsTr("Read offline \u2013 online failed: %1").arg(engine.cloudProblem || qsTr("not configured"))
        }

        Label {
            Layout.fillWidth: true
            Layout.margins: 8
            visible: engine.words.length === 0
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
            text: engine.error !== "" ? qsTr("Recognition failed: %1").arg(engine.error)
                                      : qsTr("No text found. Try again closer, with more light.")
        }

        // ---------- selection bar ----------
        Pane {
            Layout.fillWidth: true
            Material.elevation: 6
            padding: 12

            ColumnLayout {
                anchors.fill: parent
                spacing: 8

                RowLayout {
                    Layout.fillWidth: true
                    Label {
                        Layout.fillWidth: true
                        text: page.selected.length === 0
                              ? qsTr("Tap words to select · hold for a whole sentence")
                              : page.selectedText
                        font.pixelSize: page.selected.length === 0 ? 14 : 18
                        font.bold: page.selected.length > 0
                        opacity: page.selected.length === 0 ? 0.7 : 1
                        wrapMode: Text.WordWrap
                        maximumLineCount: 4
                        elide: Text.ElideRight
                    }
                    SpeakButton { speakText: page.selectedText; visible: page.selected.length > 0 }
                    ToolButton {
                        objectName: "lensStar"
                        visible: page.selected.length > 0
                        enabled: !page.anyPending
                        focusPolicy: Qt.NoFocus
                        contentItem: Item {
                            implicitWidth: 24
                            implicitHeight: 24
                            StarIcon {
                                anchors.centerIn: parent
                                width: 24
                                height: 24
                                filled: page.selectionStarred
                                color: page.selectionStarred ? "#F5B301" : Material.foreground
                                opacity: parent.parent.enabled ? (page.selectionStarred ? 1 : 0.6) : 0.3
                            }
                        }
                        ToolTip.visible: hovered || pressed
                        ToolTip.text: page.selectionStarred ? qsTr("Remove from Favorite words")
                                                            : qsTr("Add to Favorite words")
                        onClicked: page.starSelectedWords()
                    }
                }

                // Several words / a sentence: its translation as a whole
                Label {
                    Layout.fillWidth: true
                    visible: page.selected.length > 1
                    wrapMode: Text.WordWrap
                    maximumLineCount: 3
                    elide: Text.ElideRight
                    font.pixelSize: 17
                    color: Material.accent
                    horizontalAlignment: Translator.rightToLeft ? Text.AlignRight : Text.AlignLeft
                    text: page.entry(page.selectedText).text || page.sourceLabel(page.selectedText)
                    opacity: page.entry(page.selectedText).text ? 1 : 0.6
                }

                // Each selected word with its own translation (this becomes the card's back)
                ListView {
                    id: wordList
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.min(contentHeight, page.height * 0.22)
                    visible: page.selectedList.length > 0
                    clip: true
                    interactive: contentHeight > height
                    model: page.selectedList
                    delegate: RowLayout {
                        required property string modelData
                        width: wordList.width
                        spacing: 8
                        Label {
                            text: modelData
                            font.bold: true
                            font.pixelSize: 15
                        }
                        Label {
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignRight
                            elide: Text.ElideRight
                            font.pixelSize: 15
                            readonly property string meaning: page.meaningOf(modelData)
                            text: meaning !== "" ? meaning : page.sourceLabel(modelData)
                            opacity: meaning !== "" ? 1 : 0.55
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Button {
                        text: qsTr("Retake")
                        flat: true
                        onClicked: {
                            engine.clear()
                            page.fullText = ""
                            status.text = qsTr("Hold the text flat and well lit")
                            page.mode = "camera"
                        }
                    }
                    Item { Layout.fillWidth: true }
                    Button {
                        visible: page.selectedList.length > 1
                        enabled: !page.anyPending
                        text: qsTr("Add %n word(s)", "", page.selectedList.length)
                        onClicked: page.addSelectedWords(false)
                    }
                    Button {
                        highlighted: true
                        enabled: page.selected.length > 0 && !page.anyPending
                        text: qsTr("Create card")
                        onClicked: {
                            // A single word: pack article/example + its translation. Several words: the phrase translation.
                            const single = page.selected.length === 1
                                    ? engine.cleanWord(engine.words[page.selected[0]].text) : ""
                            const known = single !== "" ? WordPacks.lookup(single) : ({})
                            const front = known.front ?? (single !== "" ? single : page.selectedText)
                            const back = single !== "" ? page.backFor(single) : (page.entry(page.selectedText).text ?? "")
                            page.StackView.view.push(editPage, {
                                initialFront: front,
                                initialBack: back,
                                initialExample: known.example ?? ""
                            })
                            page.selected = [] // fresh start when coming back
                        }
                    }
                }
            }
        }
    }

    // ---------- busy ----------
    Rectangle {
        anchors.fill: parent
        visible: engine.busy
        color: "#aa000000"
        ColumnLayout {
            anchors.centerIn: parent
            BusyIndicator { Layout.alignment: Qt.AlignHCenter; running: engine.busy }
            Label { color: "white"; text: qsTr("Reading the text…") }
        }
        TapHandler {} // swallow taps
    }

    FileDialog {
        id: galleryDialog
        title: CardStore.learningLanguage === "en" ? qsTr("Choose a photo with English text")
                                                   : qsTr("Choose a photo with German text")
        nameFilters: [qsTr("Images (*.jpg *.jpeg *.png *.webp *.bmp)")]
        onAccepted: engine.recognize(selectedFile)
    }

    Dialog {
        id: existsDialog
        property var words: []
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
                text: qsTr("%n of the selected words are already cards:", "", existsDialog.words.length)
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                font.bold: true
                maximumLineCount: 4
                elide: Text.ElideRight
                text: existsDialog.words.join(", ")
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                opacity: 0.8
                text: qsTr("Update their meaning with the new translation? Their box and progress are kept.")
            }
            Button {
                Layout.fillWidth: true
                highlighted: true
                text: qsTr("Update them")
                onClicked: { existsDialog.close(); page.addSelectedWords(true) }
            }
            Button {
                Layout.fillWidth: true
                text: qsTr("Keep them, add only new words")
                onClicked: { existsDialog.close(); page.addSelectedWords("skip") }
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
