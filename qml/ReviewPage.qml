import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Controls.Material
import LearningBox

Page {
    id: page
    title: session.finished ? qsTr("Done")
                            : qsTr("Review · %1 left").arg(session.remaining)

    property bool revealed: false
    // Meanings in English or German can be heard (American / German voice); Persian has no voice.
    readonly property bool meaningSpeakable: Translator.meaningLanguage !== "fa"
                                             && !/[\u0600-\u06FF]/.test(primaryMeaning.split("\n")[0])
    readonly property string meaningTag: Translator.meaningLanguage === "en" ? "en-US" : "de-DE"
    property bool autoSpeak: true

    // ---- Direction (remembered per learning box) ----
    // "german": German first, the meaning is the answer. "meaning": meaning first, say the German
    // word (with article and plural). "mixed": each card randomly one way or the other.
    readonly property string direction: CardStore.reviewDirection
    property bool germanFirst: true
    function pickSide() {
        germanFirst = direction === "german" || (direction === "mixed" && Math.random() < 0.5)
    }
    // A card without any meaning can only be asked German first.
    function ensureQuestion() {
        if (!germanFirst && !revealed && primaryMeaning === "" && frontRequest < 0) {
            germanFirst = true
            speakQuestion()
        }
    }
    function speakQuestion() {
        if (autoSpeak && session.hasCard && germanFirst)
            Speaker.speak(session.front)
    }
    onDirectionChanged: {
        revealed = false
        pickSide()
        refreshFrontTranslation()
        speakQuestion()
        ensureQuestion()
    }

    // ---- Meaning in the chosen language (switch on the answer side, same setting as Settings) ----
    // The stored back is used when it is in that language; otherwise the German front is
    // translated (word pack for Persian, saved or online translation). Only if no translation is
    // available (offline, nothing saved) the stored back is shown instead.
    readonly property bool backIsPersian: /[\u0600-\u06FF]/.test(session.back.split("\n")[0])
    readonly property bool backMatches: session.back !== ""
                                        && (Translator.meaningLanguage === "fa") === backIsPersian
    property string frontTranslation: ""
    property int frontRequest: -1
    readonly property string primaryMeaning: backMatches ? session.back
                                             : frontTranslation !== "" ? frontTranslation
                                             : frontRequest >= 0 ? "" // still translating: show nothing yet
                                             : session.back           // no translation available


    function refreshFrontTranslation() {
        frontTranslation = ""
        frontRequest = -1
        // The meaning is needed once the answer is shown, or at once when it is the question.
        if ((!revealed && germanFirst) || !session.hasCard || backMatches)
            return
        if (Translator.meaningLanguage === "fa") {
            const known = WordPacks.lookup(session.front) // German boxes only (empty otherwise)
            if (known.back) {
                frontTranslation = known.back.split("\n")[0]
                return
            }
        }
        const saved = Translator.saved(session.front)
        if (saved !== "")
            frontTranslation = saved
        else if (Translator.useOnline)
            frontRequest = Translator.translate(session.front)
    }
    onRevealedChanged: {
        refreshFrontTranslation()
        // Meaning first: the German word is the answer, so it is read aloud now.
        if (revealed && !germanFirst && autoSpeak && session.hasCard)
            Speaker.speak(session.front)
    }
    Connections {
        target: Translator
        // callLater: run after the bindings that depend on the new language have updated
        function onSettingsChanged() { Qt.callLater(page.refreshFrontTranslation) }
        function onTranslated(requestId, text) {
            if (requestId === page.frontRequest) {
                page.frontTranslation = text
                page.frontRequest = -1
                page.ensureQuestion()
            }
        }
    }

    // Editing the card being read: reload it afterwards and keep the answer visible.
    property bool editing: false
    property bool reloading: false
    function editCurrent() {
        editing = true
        page.StackView.view.push(editPageComponent, { cardId: session.cardId })
    }
    StackView.onActivated: {
        if (!editing)
            return
        editing = false
        const id = session.cardId
        reloading = true
        session.reloadCurrent()
        reloading = false
        if (session.cardId !== id) // the card was deleted: next one, answer hidden
            revealed = false
        refreshFrontTranslation()
    }
    Component { id: editPageComponent; CardEditPage {} }

    ReviewSession {
        id: session
        onCurrentChanged: {
            if (page.reloading)
                return
            page.revealed = false
            page.pickSide()
            page.refreshFrontTranslation()
            page.speakQuestion()
            page.ensureQuestion()
        }
    }
    Component.onCompleted: session.start()
    StackView.onRemoved: Speaker.stop()

    // ---- Card ----
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 22 // keep clear of the screen edges
        spacing: 12
        visible: session.hasCard

        RowLayout {
            Layout.fillWidth: true
            // Auto-speak on the left, Edit and the favorite star on the right
            Switch {
                text: qsTr("Auto-speak")
                checked: page.autoSpeak
                onToggled: page.autoSpeak = checked
            }
            Item { Layout.fillWidth: true }
            ToolButton {
                focusPolicy: Qt.NoFocus
                onClicked: page.editCurrent()
                contentItem: Row {
                    spacing: 6
                    EditIcon {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 18; height: 18
                        color: Material.foreground
                    }
                    Label {
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("Edit")
                        font.bold: true
                    }
                }
            }
            // ★ Favorite words
            StarButton {
                objectName: "reviewStar"
                visible: session.hasCard
                cardId: session.hasCard ? session.cardId : -1
            }
        }

        // Which side comes first: a segmented switch, the chosen way explained underneath
        Rectangle {
            id: directionSwitch
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: -6
            implicitWidth: directionRow.implicitWidth + 6
            implicitHeight: 40
            radius: height / 2
            color: "transparent"
            border.width: 1
            border.color: Material.accentColor

            readonly property string meaningName: Translator.meaningLanguage === "fa" ? "فارسی"
                                                : Translator.meaningLanguage === "de" ? "Deutsch" : "English"
            // Language being learned in this learning box
            readonly property string learnName: CardStore.learningLanguage === "en" ? "English" : "Deutsch"
            Row {
                id: directionRow
                anchors.centerIn: parent
                spacing: 0
                Repeater {
                    // Each part is its own label: next to Persian text Android picks a Persian font for
                    // the arrow, which has no "→" (drawn as a box), and the text order could flip.
                    model: [ { code: "german", from: directionSwitch.learnName, to: directionSwitch.meaningName },
                             { code: "meaning", from: directionSwitch.meaningName, to: directionSwitch.learnName },
                             { code: "mixed", from: "\uD83D\uDD00", to: qsTr("Mix") } ]
                    delegate: AbstractButton {
                        id: segment
                        required property var modelData
                        readonly property bool selected: page.direction === modelData.code
                        height: 34
                        leftPadding: 12
                        rightPadding: 12
                        focusPolicy: Qt.NoFocus
                        onClicked: CardStore.reviewDirection = modelData.code
                        background: Rectangle {
                            radius: height / 2
                            color: segment.selected ? Material.accentColor
                                                    : (segment.pressed ? Qt.rgba(0.5, 0.5, 0.5, 0.2) : "transparent")
                            Behavior on color { ColorAnimation { duration: 150 } }
                        }
                        contentItem: DirectionLabel {
                            from: segment.modelData.from
                            to: segment.modelData.to
                            arrow: segment.modelData.code !== "mixed"
                            bold: segment.selected
                            color: segment.selected ? "white" : Material.foreground
                        }
                    }
                }
            }
        }
        Label {
            Layout.fillWidth: true
            Layout.topMargin: -6
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            font.pixelSize: 13
            opacity: 0.7
            readonly property bool english: CardStore.learningLanguage === "en"
            text: page.direction === "meaning" ? (english ? qsTr("See the meaning — can you say it in English?")
                                                          : qsTr("See the meaning — can you say it in German?"))
                : page.direction === "mixed" ? qsTr("Surprise me: every card comes from either side.")
                : (english ? qsTr("Read the English word — do you know what it means?")
                           : qsTr("Read the German word — do you know what it means?"))
        }

        Pane {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Material.elevation: 4

            Flickable {
                anchors.fill: parent
                contentHeight: cardColumn.implicitHeight
                clip: true

                ColumnLayout {
                    id: cardColumn
                    width: parent.width
                    spacing: 16

                    // Mix: which way this card is asked
                    Pane {
                        Layout.alignment: Qt.AlignHCenter
                        visible: page.direction === "mixed"
                        topPadding: 3
                        bottomPadding: 3
                        leftPadding: 10
                        rightPadding: 10
                        background: Rectangle {
                            radius: height / 2
                            color: "transparent"
                            border.width: 1
                            border.color: Material.accentColor
                        }
                        contentItem: Row {
                            spacing: 6
                            Label {
                                anchors.verticalCenter: parent.verticalCenter
                                text: "\uD83D\uDD00"
                                font.pixelSize: 12
                            }
                            DirectionLabel {
                                anchors.verticalCenter: parent.verticalCenter
                                from: page.germanFirst ? directionSwitch.learnName : directionSwitch.meaningName
                                to: page.germanFirst ? directionSwitch.meaningName : directionSwitch.learnName
                                pixelSize: 12
                                color: Material.accentColor
                            }
                        }
                    }

                    // Meaning first: the question is the meaning (first line only: the plural line
                    // would give the German word away).
                    RowLayout {
                        Layout.fillWidth: true
                        visible: !page.germanFirst && page.primaryMeaning.length > 0
                        MeaningText {
                            Layout.fillWidth: true
                            text: page.primaryMeaning.split("\n")[0]
                            center: true
                            pixelSize: 26
                        }
                        SpeakButton {
                            visible: page.meaningSpeakable
                            speakText: page.primaryMeaning.split("\n")[0]
                            languageTag: page.meaningTag
                        }
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: !page.germanFirst && !page.revealed
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        opacity: 0.6
                        text: CardStore.learningLanguage === "en" ? qsTr("How do you say it in English?")
                              : /^(der|die|das) /i.test(session.front)
                              ? qsTr("How do you say it in German? Der, die or das — and the plural?")
                              : qsTr("How do you say it in German?")
                    }
                    MenuSeparator { Layout.fillWidth: true; visible: !page.germanFirst && page.revealed }

                    RowLayout {
                        Layout.fillWidth: true
                        visible: page.germanFirst || page.revealed
                        Label {
                            Layout.fillWidth: true
                            text: session.front
                            wrapMode: Text.WordWrap
                            horizontalAlignment: Text.AlignHCenter
                            font.pixelSize: 28
                            font.bold: true
                        }
                        GenderMark { word: session.front }
                        SpeakButton { speakText: session.front }
                    }

                    MenuSeparator { Layout.fillWidth: true; visible: page.germanFirst && page.revealed }

                    // Language of the meaning for this learning box (never the language being learned)
                    RowLayout {
                        Layout.alignment: Qt.AlignHCenter
                        visible: page.revealed || !page.germanFirst
                        spacing: 0
                        Repeater {
                            model: CardStore.learningLanguage === "en"
                                   ? [ { code: "fa", label: "فارسی" }, { code: "de", label: "Deutsch" } ]
                                   : [ { code: "fa", label: "فارسی" }, { code: "en", label: "English" } ]
                            delegate: Button {
                                required property var modelData
                                // Plain buttons: the highlight follows the setting (no checkable state to get out of sync)
                                readonly property bool selected: Translator.targetLanguage === modelData.code
                                flat: !selected
                                highlighted: selected
                                focusPolicy: Qt.NoFocus
                                text: modelData.label
                                font.pixelSize: 13
                                implicitHeight: 36
                                onClicked: Translator.targetLanguage = modelData.code
                            }
                        }
                    }

                    Image {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredWidth: parent.width
                        Layout.preferredHeight: visible ? Math.min(implicitHeight * width / Math.max(1, implicitWidth), 260) : 0
                        visible: (page.revealed || !page.germanFirst) && session.imageUrl.toString() !== ""
                        source: session.imageUrl
                        fillMode: Image.PreserveAspectFit
                        asynchronous: true
                    }

                    // Free-text back: Persian (RTL) and Latin text both lay out by their own direction.
                    RowLayout {
                        Layout.fillWidth: true
                        // Meaning first: the full meaning (with the plural line) once the answer is shown
                        visible: page.revealed && page.primaryMeaning.length > 0
                                 && (page.germanFirst || page.primaryMeaning.indexOf("\n") > 0)
                        MeaningText {
                            Layout.fillWidth: true
                            text: page.germanFirst ? page.primaryMeaning
                                                   : page.primaryMeaning.split("\n").slice(1).join("\n")
                            center: true
                            pixelSize: 22
                        }
                        // English / German meaning read aloud (first line: the meaning itself)
                        SpeakButton {
                            Layout.alignment: Qt.AlignTop
                            visible: page.meaningSpeakable && page.germanFirst
                            speakText: page.primaryMeaning.split("\n")[0]
                            languageTag: page.meaningTag
                        }
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: (page.revealed || !page.germanFirst) && page.frontRequest >= 0
                        horizontalAlignment: Text.AlignHCenter
                        opacity: 0.6
                        text: qsTr("translating\u2026")
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        visible: page.revealed && session.example.length > 0
                        ExampleText {
                            Layout.fillWidth: true
                            example: session.example
                            pixelSize: 17
                        }
                        SpeakButton {
                            Layout.alignment: Qt.AlignTop
                            speakText: session.example
                        }
                    }
                }
            }
        }

        Button {
            Layout.fillWidth: true
            visible: !page.revealed
            highlighted: true
            text: page.germanFirst ? qsTr("Show the meaning")
                                   : CardStore.learningLanguage === "en" ? qsTr("Show the English word")
                                   : qsTr("Show the German word")
            onClicked: page.revealed = true
        }
        RowLayout {
            Layout.fillWidth: true
            visible: page.revealed
            spacing: 12
            Button {
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                text: qsTr("\u2717  Not yet")
                Material.background: Material.color(Material.Red, Material.Shade400)
                Material.foreground: "white"
                onClicked: session.answer(false)
            }
            Button {
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                text: qsTr("\u2713  Got it!")
                Material.background: Material.color(Material.Green, Material.Shade500)
                Material.foreground: "white"
                onClicked: session.answer(true)
            }
        }
    }

    Label {
        id: movedHint
        visible: false
        anchors { bottom: parent.bottom; horizontalCenter: parent.horizontalCenter; bottomMargin: 88 }
        width: Math.min(implicitWidth, parent.width - 32)
        elide: Text.ElideRight
        padding: 10
        color: "white"
        background: Rectangle { color: "#CC333333"; radius: 8 }
        Timer { id: movedTimer; interval: 2000; onTriggered: movedHint.visible = false }
    }

    // ---- Summary ----
    ColumnLayout {
        anchors.centerIn: parent
        width: parent.width - 32
        spacing: 12
        visible: session.finished

        Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            font.pixelSize: 22
            font.bold: true
            wrapMode: Text.WordWrap
            text: session.total === 0 ? qsTr("Nothing due right now.") : qsTr("Session complete")
        }
        Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            visible: session.total > 0
            text: qsTr("%1 known · %2 to repeat").arg(session.correctCount).arg(session.wrongCount)
        }
        Button {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Back")
            onClicked: page.StackView.view.pop()
        }
    }
}
