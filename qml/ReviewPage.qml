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
                                        && (Translator.targetLanguage === "fa") === backIsPersian
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
        if (Translator.targetLanguage === "fa") {
            const known = WordPacks.lookup(session.front)
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
        anchors.margins: 16
        spacing: 12
        visible: session.hasCard

        RowLayout {
            Layout.fillWidth: true
            // Box of this card; choose another to move it there by hand (then the next card comes).
            ComboBox {
                id: boxChooser
                Layout.preferredWidth: 130
                focusPolicy: Qt.NoFocus
                model: [qsTr("Box 1"), qsTr("Box 2"), qsTr("Box 3"), qsTr("Box 4"), qsTr("Box 5"), qsTr("Learned")]
                currentIndex: Math.max(0, Math.min(5, session.box - 1))
                displayText: session.isRetry ? qsTr("Again · Box %1").arg(session.box) : currentText
                onActivated: (index) => {
                    const target = index + 1
                    const name = currentText
                    currentIndex = Qt.binding(() => Math.max(0, Math.min(5, session.box - 1)))
                    if (target === session.box && !session.isRetry)
                        return
                    movedHint.text = qsTr("\u201C%1\u201D moved to %2").arg(session.front).arg(name)
                    movedHint.visible = true
                    movedTimer.restart()
                    session.moveCurrent(target)
                }
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
            Switch {
                text: qsTr("Auto-speak")
                checked: page.autoSpeak
                onToggled: page.autoSpeak = checked
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

            readonly property string meaningName: Translator.targetLanguage === "fa" ? "فارسی" : "English"
            Row {
                id: directionRow
                anchors.centerIn: parent
                spacing: 0
                Repeater {
                    model: [ { code: "german", label: "Deutsch \u2192 " + directionSwitch.meaningName },
                             { code: "meaning", label: "\u200E" + directionSwitch.meaningName + " \u2192 Deutsch" },
                             { code: "mixed", label: "\uD83D\uDD00 " + qsTr("Mix") } ]
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
                        contentItem: Label {
                            text: segment.modelData.label
                            font.pixelSize: 13
                            font.bold: segment.selected
                            color: segment.selected ? "white" : Material.foreground
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
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
            text: page.direction === "meaning" ? qsTr("See the meaning — can you say it in German?")
                : page.direction === "mixed" ? qsTr("Surprise me: every card comes from either side.")
                : qsTr("Read the German word — do you know what it means?")
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
                    Label {
                        Layout.alignment: Qt.AlignHCenter
                        visible: page.direction === "mixed"
                        font.pixelSize: 12
                        leftPadding: 10
                        rightPadding: 10
                        topPadding: 3
                        bottomPadding: 3
                        color: Material.accentColor
                        text: "\uD83D\uDD00 " + (page.germanFirst ? "Deutsch \u2192 " + directionSwitch.meaningName
                                                                  : "\u200E" + directionSwitch.meaningName + " \u2192 Deutsch")
                        background: Rectangle {
                            radius: height / 2
                            color: "transparent"
                            border.width: 1
                            border.color: Material.accentColor
                        }
                    }

                    // Meaning first: the question is the meaning (first line only: the plural line
                    // would give the German word away).
                    MeaningText {
                        Layout.fillWidth: true
                        visible: !page.germanFirst && page.primaryMeaning.length > 0
                        text: page.primaryMeaning.split("\n")[0]
                        center: true
                        pixelSize: 26
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: !page.germanFirst && !page.revealed
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        opacity: 0.6
                        text: /^(der|die|das) /i.test(session.front)
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
                        SpeakButton { speakText: session.front }
                    }

                    MenuSeparator { Layout.fillWidth: true; visible: page.germanFirst && page.revealed }

                    // Language of the meaning (also changes the setting)
                    RowLayout {
                        Layout.alignment: Qt.AlignHCenter
                        visible: page.revealed || !page.germanFirst
                        spacing: 0
                        Repeater {
                            model: [ { code: "fa", label: "فارسی" }, { code: "en", label: "English" } ]
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
                    MeaningText {
                        Layout.fillWidth: true
                        // Meaning first: the full meaning (with the plural line) once the answer is shown
                        visible: page.revealed && page.primaryMeaning.length > 0
                                 && (page.germanFirst || page.primaryMeaning.indexOf("\n") > 0)
                        text: page.germanFirst ? page.primaryMeaning
                                               : page.primaryMeaning.split("\n").slice(1).join("\n")
                        center: true
                        pixelSize: 22
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
            text: page.germanFirst ? qsTr("Show the meaning") : qsTr("Show the German word")
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
