import QtCore
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Controls.Material
import QtQuick.Dialogs
import QtMultimedia
import LearningBox

// "Lens": take a photo (or pick one), recognize the German text, show it translated
// (Persian or English, see Settings), tap words or press-and-hold for a whole sentence
// and see their meaning; "Create card" opens Add card with the selection.
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
    readonly property bool anyPending: grammarPending || selectedList.some(w => entry(w).pending === true)
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
    // Article and plural of German nouns (Wiktionary): word -> {front, forms, pending}
    property var gram: ({})
    property var gramRequests: ({})
    function requestGrammar(w) {
        if (CardStore.learningLanguage !== "de" || gram[w] !== undefined)
            return
        if (w.length < 2) return // nouns only: Translator.lookupGrammar skips lowercase words that are no nouns
        const next = Object.assign({}, gram)
        next[w] = { pending: true }
        gram = next
        gramRequests[Translator.lookupGrammar(w)] = w
    }
    readonly property bool grammarPending: selectedList.some(w => gram[w]?.pending === true)
    function resetTranslations() {
        tr = ({}); requests = ({})
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
        return e.error ? qsTr("translation failed · no saved translation") : qsTr("no translation")
    }
    // Card front a word would get (the pack's "das Brot" for "Brot").
    function frontFor(w) {
        const f = WordPacks.lookup(w).front
        if (f !== undefined && /^(der|die|das)\s/i.test(f)) return f
        return gram[w]?.front ?? f ?? w
    }

    Connections {
        target: Translator
        function onGrammarFound(requestId, grammar, error) {
            const w = page.gramRequests[requestId]
            if (w === undefined) return
            delete page.gramRequests[requestId]
            const next = Object.assign({}, page.gram)
            if (error) { delete next[w]; page.gram = next; return } // retried on the next selection
            next[w] = { front: grammar.front ?? "", forms: grammar.forms ?? "", pending: false }
            if (!next[w].front) next[w].front = undefined
            page.gram = next
        }
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
    onSelectedListChanged: { for (const w of selectedList) { request(w); requestGrammar(w) } }
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

    Component { id: editPage; CardEditPage {} }

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
    // A shot that never completes: on some phones (seen on Xiaomi) the camera is closed by Android
    // right after the shutter was pressed, the picture is lost and the shutter stays disabled.
    // A retake after restarting the camera fails the same way, so at the moment of the shot a copy of
    // the viewfinder picture is kept; if the still capture fails, that copy is recognized instead.
    property bool shotInFlight: false
    onModeChanged: if (mode !== "camera") { shotInFlight = false; shotWatchdog.stop() }
    function startShot(capture) {
        shotInFlight = true
        shotWatchdog.restart()
        const ok = cameraLoader.item ? engine.grabFrame(cameraLoader.item.sink) : false
        console.log("Lens: viewfinder copy kept =", ok)
        capture.captureToFile(engine.captureFilePath())
    }
    // ---- debug.bat self-test: press the shutter by itself and log what happens ----
    readonly property bool autoTest: AppMode.lensAutoTest()
    readonly property int autoTotal: 10
    property int autoShots: 0
    property int autoIdle: 0
    Timer {
        interval: 1500
        repeat: true
        running: page.autoTest && page.autoShots <= page.autoTotal
        onTriggered: {
            if (page.autoShots >= page.autoTotal) {
                if (page.mode === "result" || !page.shotInFlight) {
                    console.log("Lens: AUTOTEST done, shots =", page.autoShots)
                    page.autoShots++
                }
                return
            }
            const cap = cameraLoader.item ? cameraLoader.item.capture : null
            if (page.mode === "result") {
                engine.clear()
                page.fullText = ""
                page.mode = "camera"
                return
            }
            if (page.mode === "camera" && !page.shotInFlight && !engine.busy && cap && cap.readyForCapture) {
                page.autoIdle = 0
                page.autoShots++
                console.log("Lens: AUTOTEST shot", page.autoShots, "of", page.autoTotal)
                shutter.clicked()
            } else if (++page.autoIdle % 8 === 0) {
                console.log("Lens: AUTOTEST waiting, mode =", page.mode, "inFlight =", page.shotInFlight,
                            "busy =", engine.busy, "ready =", cap ? cap.readyForCapture : "no capture")
            }
        }
    }
    function restartCamera() {
        cameraKick = true
        kickGap.restart()
    }
    property int kickCount: 0           // restarts since the viewfinder was shown
    readonly property int maxKicks: 2
    StackView.onRemoved: engine.clear()
    // Leaving Lens (back, or on to the card editor): stop reading aloud
    StackView.onDeactivating: Speaker.stop()


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
            onLoaded: { console.log("Lens: camera session created (kick " + page.kickCount + ")"); readyWatchdog.restart() }
            onActiveChanged: if (!active && !page.cameraKick) page.kickCount = 0 // shown again later: start fresh
            sourceComponent: Item {
                readonly property alias capture: imageCapture
                readonly property var sink: viewfinder.videoSink

                CaptureSession {
                    camera: Camera {
                        active: true
                        focusMode: Camera.FocusModeAutoNear
                        onActiveChanged: console.log("Lens: camera active =", active)
                        onErrorOccurred: (error, message) => { console.log("Lens: camera error", error, message); status.text = message }
                    }
                    imageCapture: ImageCapture {
                        id: imageCapture
                        onReadyForCaptureChanged: {
                            console.log("Lens: readyForCapture =", readyForCapture)
                        }
                        // path is "/data/..." on Android, "C:/..." on Windows
                        onImageSaved: (id, path) => { if (page.mode !== "camera") return; console.log("Lens: image saved"); page.shotInFlight = false; shotWatchdog.stop(); engine.recognize(Qt.url((path.startsWith("/") ? "file://" : "file:///") + path),
                                                                     page.shotRotation) }
                        onErrorOccurred: (id, error, message) => { console.log("Lens: capture error", id, error, message); status.text = message }
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
                    console.log("Lens: shutter pressed, readyForCapture =", capture.readyForCapture)
                    // Tilt at the moment of the shot; -1 (detect from the picture) without a sensor.
                    page.shotRotation = tilt.available ? tilt.rotation : -1
                    page.startShot(capture)
                }
            }
            Item { Layout.preferredWidth: 56 }
        }

        // Self-healing: if the shutter is still disabled a few seconds after the camera
        // session was (re)created, restart the session once instead of leaving it stuck.
        Timer {
            id: readyWatchdog
            // A stuck camera is restarted after 4 s (this recovers it); later tries wait a little longer,
            // and it gives up after maxKicks restarts instead of looping forever.
            interval: 4000 + page.kickCount * 4000
            repeat: false
            onTriggered: {
                if (page.shotInFlight)
                    return // the shot watchdog handles a picture that is being taken
                const cap = cameraLoader.item ? cameraLoader.item.capture : null
                console.log("Lens: watchdog, ready =", cap ? cap.readyForCapture : "no capture", "kicks =", page.kickCount)
                if (!cap || cap.readyForCapture)
                    return
                if (page.kickCount >= page.maxKicks) {
                    status.text = qsTr("The camera is not ready. Go back and open Lens again, or pick a photo from the gallery.")
                    return
                }
                page.kickCount++
                status.text = qsTr("Camera is taking a moment — restarting it…")
                page.cameraKick = true
                kickGap.restart()
            }
        }
        // Picture taken but never finished (shutter still disabled): use the viewfinder copy instead.
        Timer {
            id: shotWatchdog
            interval: 3500
            repeat: false
            onTriggered: {
                const cap = cameraLoader.item ? cameraLoader.item.capture : null
                console.log("Lens: shot watchdog, inFlight =", page.shotInFlight, "ready =", cap ? cap.readyForCapture : "no capture")
                if (!page.shotInFlight || page.mode !== "camera" || (cap && cap.readyForCapture))
                    return
                page.shotInFlight = false
                if (engine.recognizeGrabbed(page.shotRotation)) {
                    console.log("Lens: still capture failed, recognizing the viewfinder copy")
                    status.text = qsTr("Camera hiccup — using the viewfinder picture…")
                } else {
                    status.text = qsTr("The picture could not be taken. Please try again.")
                    page.restartCamera()
                }
            }
        }
        // Android closes a camera asynchronously: opening it again in the same moment often fails
        // (black preview, shutter stays grey). Leave it closed for a while before re-creating it.
        Timer {
            id: kickGap
            interval: 1500
            repeat: false
            onTriggered: page.cameraKick = false
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
                    SpeakButton { speakText: page.selectedText; visible: page.selected.length > 0; iconSize: 19 }
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

                // Each selected word with its translation
                ListView {
                    id: wordList
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.min(contentHeight, page.height * 0.22)
                    visible: page.selectedList.length > 0
                    clip: true
                    interactive: contentHeight > height
                    model: page.selectedList
                    delegate: ColumnLayout {
                        required property string modelData
                        width: wordList.width
                        spacing: 0
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            Label {
                                text: page.frontFor(modelData)
                                font.bold: true
                                font.pixelSize: 15
                            }
                            GenderMark { word: page.frontFor(modelData) }
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
                        highlighted: true
                        enabled: page.selected.length > 0 && !page.anyPending
                        text: qsTr("Create card")
                        onClicked: {
                            // The word goes into Add card like a typed one: that page finds article, forms,
                            // meaning and example sentences itself, exactly as in the Dictionary. A sentence keeps
                            // its translation from here.
                            const single = page.selected.length === 1
                                    ? engine.cleanWord(engine.words[page.selected[0]].text) : ""
                            page.StackView.view.push(editPage, {
                                initialFront: single !== "" ? single : page.selectedText,
                                initialBack: single !== "" ? "" : (page.entry(page.selectedText).text ?? "")
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
}
