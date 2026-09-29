import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Controls.Material
import LearningBox

// First start only: meaning language, Simple or Full mode, 100 starter words, German voice.
// Everything can be changed later in Settings.
Page {
    id: page
    objectName: "setupPage"
    title: qsTr("Welcome")

    signal finished()

    property string chosenMode: "simple"
    readonly property bool fa: Translator.targetLanguage === "fa"
    readonly property var stepTitles: [qsTr("Welcome"), qsTr("Choose your app"), qsTr("Starter words"), qsTr("Ready")]

    function finish() {
        AppMode.mode = chosenMode
        if (starterBox.checked) {
            // A new install has one empty learning box: use it for the starter words.
            const name = qsTr("Starter – 100 words")
            if (CardStore.collections.length === 1 && CardStore.totalCount === 0) {
                CardStore.renameCollection(CardStore.currentCollection, name)
            } else {
                const id = CardStore.createCollection(name)
                if (id > 0)
                    CardStore.selectCollection(id)
            }
            WordPacks.addStarterCards()
        }
        AppMode.setupDone = true
        page.finished()
    }

    header: ColumnLayout {
        spacing: 0
        Label {
            Layout.fillWidth: true
            Layout.topMargin: 16
            horizontalAlignment: Text.AlignHCenter
            text: page.stepTitles[steps.currentIndex]
            font.pixelSize: 20
            font.bold: true
        }
        PageIndicator {
            Layout.alignment: Qt.AlignHCenter
            count: steps.count
            currentIndex: steps.currentIndex
        }
    }

    SwipeView {
        id: steps
        anchors.fill: parent
        interactive: false
        clip: true

        // ---- 1. Welcome + language ----
        Flickable {
            contentHeight: welcome.implicitHeight + 32
            clip: true
            ColumnLayout {
                id: welcome
                x: 16; y: 8
                width: parent.width - 32
                spacing: 12
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    font.pixelSize: 16
                    text: qsTr("Learn German words with a Leitner box: cards you know move up and come back later, cards you don't know come back soon.")
                }
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    font.pixelSize: 16
                    horizontalAlignment: Text.AlignRight
                    text: "واژه‌های آلمانی را با جعبهٔ لایتنر یاد بگیرید."
                }
                Label {
                    Layout.topMargin: 8
                    font.bold: true
                    text: qsTr("Meanings in") + " / " + "معنی به"
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Repeater {
                        model: [ { code: "en", label: "English" }, { code: "fa", label: "فارسی" } ]
                        delegate: Button {
                            required property var modelData
                            Layout.fillWidth: true
                            Layout.preferredHeight: 56
                            highlighted: Translator.targetLanguage === modelData.code
                            text: modelData.label
                            font.pixelSize: 17
                            onClicked: Translator.targetLanguage = modelData.code
                        }
                    }
                }
                HintLabel {
                    text: qsTr("The back of your cards and the help are shown in this language. You can change it any time in Settings.")
                }
            }
        }

        // ---- 2. Simple or Full ----
        Flickable {
            contentHeight: modeColumn.implicitHeight + 32
            clip: true
            ColumnLayout {
                id: modeColumn
                x: 16; y: 8
                width: parent.width - 32
                spacing: 8

                Repeater {
                    model: [
                        { mode: "simple", title: qsTr("Simple"), text: qsTr("Just Leitner flashcards: add cards, review, listen. Offline, no permissions.") },
                        { mode: "full", title: qsTr("Full"), text: qsTr("Everything: Lens (words from photos), meanings and example sentences filled in, word packs, pictures, Anki decks.") }
                    ]
                    delegate: Pane {
                        required property var modelData
                        readonly property bool chosen: page.chosenMode === modelData.mode
                        Layout.fillWidth: true
                        padding: 12
                        background: Rectangle {
                            radius: 8
                            color: chosen ? Qt.rgba(Material.accentColor.r, Material.accentColor.g, Material.accentColor.b, 0.15) : "transparent"
                            border.width: chosen ? 2 : 1
                            border.color: chosen ? Material.accentColor : Material.dividerColor
                        }
                        TapHandler { onTapped: page.chosenMode = modelData.mode }
                        ColumnLayout {
                            anchors.fill: parent
                            RowLayout {
                                RadioButton {
                                    checked: chosen
                                    onClicked: page.chosenMode = modelData.mode
                                }
                                Label { text: modelData.title; font.pixelSize: 18; font.bold: true }
                            }
                            Label {
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                                text: modelData.text
                            }
                        }
                    }
                }
                HintLabel { text: qsTr("You can switch any time in Settings. Your cards and progress are kept.") }
                Label { Layout.topMargin: 8; text: qsTr("What's included"); font.bold: true }
                ModeComparison { Layout.fillWidth: true }
            }
        }

        // ---- 3. Starter words ----
        Flickable {
            contentHeight: starterColumn.implicitHeight + 32
            clip: true
            ColumnLayout {
                id: starterColumn
                x: 16; y: 8
                width: parent.width - 32
                spacing: 8
                CheckBox {
                    id: starterBox
                    checked: true
                    text: qsTr("Add %1 starter words (recommended)").arg(WordPacks.starterTotal)
                    font.pixelSize: 16
                }
                HintLabel {
                    text: qsTr("Common everyday words — greetings, numbers, family, food, home, time, verbs, city — with English and Persian meanings and an example sentence. They go into their own learning box “Starter – 100 words”.")
                }
                Label { Layout.topMargin: 8; text: qsTr("For example"); font.bold: true }
                Repeater {
                    model: [ ["das Brot", "bread", "نان"], ["der Bahnhof", "train station", "ایستگاه قطار"], ["sprechen", "to speak", "صحبت کردن"] ]
                    delegate: Label {
                        required property var modelData
                        Layout.fillWidth: true
                        text: "• " + modelData[0] + "  →  " + (page.fa ? modelData[2] : modelData[1])
                    }
                }
                HintLabel { text: qsTr("You can add them later too: Home → + (new learning box) → Start with: starter words.") }
            }
        }

        // ---- 4. Ready ----
        Flickable {
            contentHeight: readyColumn.implicitHeight + 32
            clip: true
            ColumnLayout {
                id: readyColumn
                x: 16; y: 8
                width: parent.width - 32
                spacing: 8
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    font.pixelSize: 16
                    text: page.chosenMode === "full" ? qsTr("You chose the Full app.") : qsTr("You chose the Simple app.")
                }
                Switch {
                    visible: page.chosenMode === "full"
                    text: qsTr("Translate online when connected")
                    checked: Translator.onlineEnabled
                    onToggled: Translator.onlineEnabled = checked
                }
                HintLabel {
                    visible: page.chosenMode === "full"
                    text: qsTr("Fills in meanings and example sentences for new words. Offline, saved translations are used.")
                }
                Label {
                    Layout.topMargin: 8
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: !Speaker.ready ? qsTr("Checking the German voice …")
                          : Speaker.germanAvailable ? qsTr("German voice: installed ✓")
                          : qsTr("German voice: not installed. Cards are not read aloud until you install one.")
                    color: Speaker.ready && !Speaker.germanAvailable ? Material.color(Material.Orange) : Material.foreground
                }
                Button {
                    visible: Speaker.ready && !Speaker.germanAvailable && Qt.platform.os === "android"
                    flat: true
                    text: qsTr("Get the German voice (Google Play)")
                    onClicked: Qt.openUrlExternally("https://play.google.com/store/apps/details?id=com.google.android.tts")
                }
                HintLabel {
                    Layout.topMargin: 8
                    text: qsTr("Tip: the ? at the top of every page explains how everything works.")
                }
            }
        }
    }

    footer: RowLayout {
        spacing: 8
        Button {
            Layout.margins: 12
            flat: true
            visible: steps.currentIndex > 0
            text: qsTr("Back")
            onClicked: steps.currentIndex -= 1
        }
        Item { Layout.fillWidth: true }
        Button {
            Layout.margins: 12
            highlighted: true
            text: steps.currentIndex < steps.count - 1 ? qsTr("Next") : qsTr("Start learning")
            onClicked: {
                if (steps.currentIndex < steps.count - 1)
                    steps.currentIndex += 1
                else
                    page.finish()
            }
        }
    }
}
