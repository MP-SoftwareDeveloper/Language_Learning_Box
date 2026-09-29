import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Controls.Material
import LearningBox

Page {
    id: page
    title: qsTr("Learning Box")

    signal reviewRequested()
    signal addRequested()
    signal browseRequested()
    signal sendRequested()
    signal getRequested()
    signal packsRequested()
    signal lensRequested()
    signal boxRequested(int box) // 1..5, 6 = Learned

    // Learning-box chooser: light-blue background.
    readonly property color comboColor: "#E3F2FD"
    // Box tiles fade from blue (Box 1) to green (Learned).
    readonly property color tileBlue: "#1E88E5"
    readonly property color tileGreen: "#43A047"
    function tileColor(i) { return Qt.tint(tileBlue, Qt.rgba(tileGreen.r, tileGreen.g, tileGreen.b, i / 5)) }
    readonly property color selectedText: "#0D2A45"
    // Flag of the language learned in a learning box
    function flag(lang) { return lang === "en" ? "\uD83C\uDDFA\uD83C\uDDF8" : "\uD83C\uDDE9\uD83C\uDDEA" }
    readonly property bool english: CardStore.learningLanguage === "en"
    property int menuId: 0
    property string menuName: ""

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth

        ColumnLayout {
            width: parent.width
            spacing: 16

            Item { Layout.preferredHeight: 4 }

            // ---- Learning box chooser ----
            // A ComboBox whose closed state already shows the first two learning boxes
            // (the selected one, highlighted, and the one used before it — tap to switch).
            // The arrow opens the full list. The list is ordered by last use, so the
            // selected box is always first and stays first after a restart.
            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 4
                spacing: 0

                ComboBox {
                    id: boxCombo
                    Layout.fillWidth: true
                    model: CardStore.collections
                    textRole: "name"
                    currentIndex: 0
                    Material.foreground: page.selectedText // arrow, readable on the light background
                    background: Rectangle {
                        implicitHeight: 48
                        radius: 6
                        color: boxCombo.pressed ? Qt.darker(page.comboColor, 1.05) : page.comboColor
                    }
                    onActivated: (index) => {
                        const id = CardStore.collections[index].id
                        CardStore.selectCollection(id)
                        currentIndex = 0
                    }

                    contentItem: Column {
                        leftPadding: 4
                        topPadding: 6
                        bottomPadding: 6
                        spacing: 2
                        // Selected learning box
                        Label {
                            width: Math.min(implicitWidth, boxCombo.availableWidth - 8)
                            text: page.flag(CardStore.learningLanguage) + "  " + CardStore.currentCollectionName
                            elide: Text.ElideRight
                            font.pixelSize: 17
                            font.bold: true
                            color: page.selectedText
                            leftPadding: 6
                            rightPadding: 6
                            topPadding: 2
                            bottomPadding: 2
                        }
                        // Second option, shown without opening the list.
                        Label {
                            width: boxCombo.availableWidth - 8
                            visible: CardStore.collections.length > 1
                            text: CardStore.collections.length > 1
                                  ? page.flag(CardStore.collections[1].language) + "  " + CardStore.collections[1].name
                                    + "  ·  " + qsTr("%n due", "", CardStore.collections[1].due)
                                  : ""
                            elide: Text.ElideRight
                            leftPadding: 6
                            color: page.selectedText
                            opacity: secondTap.pressed ? 0.4 : 0.75
                            MouseArea {
                                id: secondTap
                                anchors.fill: parent
                                onClicked: {
                                    CardStore.selectCollection(CardStore.collections[1].id)
                                    boxCombo.currentIndex = 0
                                }
                            }
                        }
                    }

                    delegate: ItemDelegate {
                        required property var modelData
                        required property int index
                        width: ListView.view ? ListView.view.width : implicitWidth
                        contentItem: RowLayout {
                            spacing: 8
                            Label {
                                Layout.fillWidth: true
                                Layout.maximumWidth: implicitWidth
                                text: page.flag(modelData.language) + "  " + modelData.name
                                elide: Text.ElideRight
                                font.bold: modelData.current
                                leftPadding: 6
                                rightPadding: 6
                            }
                            Item { Layout.fillWidth: true }
                            Label {
                                text: qsTr("%1 cards").arg(modelData.total)
                                opacity: 0.6
                            }
                        }
                    }
                }
                ToolButton {
                    text: "+"
                    font.pixelSize: 22
                    onClicked: newDialog.open()
                }
                ToolButton {
                    text: "⋮"
                    font.pixelSize: 20
                    onClicked: {
                        page.menuId = CardStore.currentCollection
                        page.menuName = CardStore.currentCollectionName
                        boxMenu.popup()
                    }
                }
            }

            GridLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                columns: 3
                rowSpacing: 8
                columnSpacing: 8

                Repeater {
                    model: 6
                    // Tap a box to see its cards and move them between boxes.
                    delegate: Pane {
                        required property int index
                        Layout.fillWidth: true
                        id: tile
                        readonly property color base: page.tileColor(index)
                        padding: 10
                        // Fading blue-green: lighter at the top, the tile's colour at the bottom.
                        background: Rectangle {
                            radius: 8
                            opacity: boxTap.pressed ? 0.8 : 1
                            gradient: Gradient {
                                GradientStop { position: 0; color: Qt.lighter(tile.base, 1.3) }
                                GradientStop { position: 1; color: tile.base }
                            }
                        }
                        TapHandler {
                            id: boxTap
                            onTapped: page.boxRequested(index + 1)
                        }
                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 2
                            Label {
                                text: index < 5 ? qsTr("Box %1").arg(index + 1) : qsTr("Learned")
                                color: "white"
                                opacity: 0.85
                            }
                            Label {
                                text: index < 5 ? (CardStore.boxCounts[index] ?? 0) : CardStore.learnedCount
                                color: "white"
                                font.pixelSize: 26
                                font.bold: true
                            }
                        }
                    }
                }
            }

            Label {
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: CardStore.totalCount === 0
                      ? (page.english ? qsTr("This learning box is empty. Add your first English word.")
                                      : qsTr("This learning box is empty. Add your first German word."))
                      : qsTr("%n card(s) due today", "", CardStore.dueCount)
                font.pixelSize: 16
            }

            Button {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                highlighted: true
                enabled: CardStore.dueCount > 0
                text: qsTr("Start review")
                onClicked: page.reviewRequested()
            }
            Button {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                visible: AppMode.full
                text: qsTr("\uD83D\uDCF7  Lens \u00B7 words from a photo")
                onClicked: page.lensRequested()
            }
            Button {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                text: qsTr("Add card")
                onClicked: page.addRequested()
            }
            Button {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                visible: AppMode.full && !page.english // German words
                text: qsTr("Word packs · %1").arg(WordPacks.title)
                onClicked: page.packsRequested()
            }
            Button {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                flat: true
                enabled: CardStore.totalCount > 0
                text: qsTr("All cards (%1)").arg(CardStore.totalCount)
                onClicked: page.browseRequested()
            }
            // Send / get cards, each with a hint in plain words
            Button {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.topMargin: 4
                flat: true
                enabled: CardStore.totalCount > 0
                text: qsTr("\u2B06  Send or back up cards")
                onClicked: page.sendRequested()
            }
            HintLabel {
                Layout.leftMargin: 24
                Layout.rightMargin: 24
                Layout.topMargin: -8
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("Save this learning box as a file — for a friend, a new phone or a backup.")
            }
            Button {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                flat: true
                text: qsTr("\u2B07  Get cards from a file or link")
                onClicked: page.getRequested()
            }
            HintLabel {
                Layout.leftMargin: 24
                Layout.rightMargin: 24
                Layout.topMargin: -8
                horizontalAlignment: Text.AlignHCenter
                text: AppMode.full ? qsTr("Add cards a friend sent you, or from Anki, Quizlet or Excel.")
                                   : qsTr("Add cards a friend sent you, or from Quizlet or Excel.")
            }

            // Simple mode: what Full mode would add
            Pane {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.topMargin: 8
                visible: !AppMode.full
                padding: 12
                background: Rectangle {
                    radius: 8
                    color: "transparent"
                    border.width: 1
                    border.color: Material.accentColor
                }
                ColumnLayout {
                    anchors.fill: parent
                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        font.bold: true
                        text: qsTr("More features")
                    }
                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: qsTr("Lens (words from photos), meanings and example sentences filled in for you, word packs, pictures on cards …")
                    }
                    Button {
                        flat: true
                        text: qsTr("See what the Full app adds")
                        onClicked: compareDialog.open()
                    }
                }
            }

            // Speech settings
            Label {
                Layout.leftMargin: 16
                Layout.topMargin: 8
                text: qsTr("Speech")
                font.pixelSize: 16
                font.bold: true
            }
            Label {
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                visible: Speaker.ready && !Speaker.voiceAvailable
                color: Material.color(Material.Orange)
                text: qsTr("No %1 voice is installed. On Android: Settings → Text-to-speech → install the %1 voice data.").arg(Speaker.voiceName)
            }
            Button {
                Layout.leftMargin: 16
                flat: true
                visible: Speaker.ready && !Speaker.voiceAvailable && Qt.platform.os === "android"
                text: qsTr("Get the %1 voice (Speech Recognition and Synthesis from Google)").arg(Speaker.voiceName)
                onClicked: Qt.openUrlExternally("https://play.google.com/store/apps/details?id=com.google.android.tts")
            }
            RowLayout {
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Label { text: qsTr("Speed") }
                Slider {
                    Layout.fillWidth: true
                    from: -1; to: 1; stepSize: 0.1
                    value: Speaker.rate
                    onMoved: Speaker.rate = value
                }
                SpeakButton { speakText: page.english ? "Good morning! How are you?" : "Guten Morgen! Wie geht es dir?" }
            }

            Label {
                Layout.leftMargin: 16
                visible: CardStore.lastError.length > 0
                color: Material.color(Material.Red)
                text: CardStore.lastError
            }
            Item { Layout.preferredHeight: 16 }
        }
    }
    Menu {
        id: boxMenu
        MenuItem {
            text: qsTr("Rename")
            onTriggered: {
                renameField.text = page.menuName
                renameDialog.open()
            }
        }
        MenuItem {
            text: qsTr("Start over (all cards to Box 1)")
            enabled: CardStore.totalCount > 0
            onTriggered: resetDialog.open()
        }
        MenuItem {
            text: qsTr("Delete")
            enabled: CardStore.collections.length > 1
            onTriggered: deleteDialog.open()
        }
    }

    ResetDialog { id: resetDialog }

    Dialog {
        id: compareDialog
        title: qsTr("Simple and Full app")
        modal: true
        width: Math.min(360, page.width - 16)
        height: Math.min(implicitHeight, page.height - 16)
        x: (page.width - width) / 2
        y: Math.max(8, (page.height - height) / 2)
        contentItem: Flickable {
            implicitHeight: compareColumn.implicitHeight
            contentHeight: compareColumn.implicitHeight
            clip: true
            ColumnLayout {
                id: compareColumn
                width: parent.width
                ModeComparison { Layout.fillWidth: true }
                HintLabel {
                    Layout.topMargin: 8
                    text: qsTr("Your cards and progress are kept when you switch. You can switch back in Settings.")
                }
            }
        }
        footer: DialogButtonBox {
            Button {
                flat: true
                text: qsTr("Not now")
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            }
            Button {
                flat: true
                text: qsTr("Switch to Full")
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            }
        }
        onAccepted: AppMode.mode = "full"
    }

    Dialog {
        id: newDialog
        title: qsTr("New learning box")
        modal: true
        width: 320
        x: (page.width - width) / 2
        y: Math.max(8, (page.height - height) / 3)
        standardButtons: Dialog.Ok | Dialog.Cancel
        property string meaning: "fa"
        onAboutToShow: {
            newName.text = ""
            meaning = "fa"
            learnGerman.checked = true
            startWith.currentIndex = 0
            newName.forceActiveFocus()
        }
        ColumnLayout {
            anchors.fill: parent
            spacing: 8
            TextField {
                id: newName
                Layout.fillWidth: true
                placeholderText: learnEnglish.checked ? qsTr("e.g. English A2") : qsTr("e.g. Netzwerk neu A2")
            }
            Label { text: qsTr("I want to learn") }
            RowLayout {
                ButtonGroup { id: learnGroup }
                RadioButton {
                    id: learnGerman
                    ButtonGroup.group: learnGroup
                    checked: true
                    text: "\uD83C\uDDE9\uD83C\uDDEA Deutsch"
                }
                RadioButton {
                    id: learnEnglish
                    ButtonGroup.group: learnGroup
                    text: "\uD83C\uDDFA\uD83C\uDDF8 English"
                    onCheckedChanged: {
                        startWith.currentIndex = 0
                        if (newDialog.meaning === (checked ? "en" : "de"))
                            newDialog.meaning = "fa" // never the language being learned
                    }
                }
            }
            HintLabel {
                visible: learnEnglish.checked
                text: qsTr("Read aloud with an American voice.")
            }
            Label { text: qsTr("Meanings in") }
            RowLayout {
                Repeater {
                    // Every language except the one being learned
                    model: [ { code: "fa", label: "فارسی" }, { code: "en", label: "English" }, { code: "de", label: "Deutsch" } ]
                        .filter(o => o.code !== (learnEnglish.checked ? "en" : "de"))
                    delegate: RadioButton {
                        required property var modelData
                        checkable: false // shows newDialog.meaning, never its own state
                        text: modelData.label
                        checked: newDialog.meaning === modelData.code
                        onClicked: newDialog.meaning = modelData.code
                    }
                }
            }
            Label { text: qsTr("Start with"); visible: !learnEnglish.checked }
            ComboBox {
                id: startWith
                Layout.fillWidth: true
                visible: !learnEnglish.checked // starter words and word packs are German
                // Word pack only in the Full app
                model: AppMode.full ? [qsTr("Empty box"), qsTr("%1 starter words (German, English, Persian)").arg(WordPacks.starterTotal), qsTr("Word pack: %1").arg(WordPacks.title)]
                                    : [qsTr("Empty box"), qsTr("%1 starter words (German, English, Persian)").arg(WordPacks.starterTotal)]
            }
        }
        onAccepted: {
            const name = newName.text.trim()
            const id = CardStore.createCollection(name.length > 0 ? name : qsTr("Learning box %1").arg(CardStore.collections.length + 1),
                                                  learnEnglish.checked ? "en" : "de", newDialog.meaning)
            if (id > 0) {
                CardStore.selectCollection(id)
                if (startWith.currentIndex === 1)
                    WordPacks.addStarterCards()
                else if (startWith.currentIndex === 2)
                    WordPacks.addAll()
            }
        }
    }

    Dialog {
        id: renameDialog
        title: qsTr("Rename learning box")
        modal: true
        width: 320
        x: (page.width - width) / 2
        y: Math.max(8, (page.height - height) / 3)
        standardButtons: Dialog.Ok | Dialog.Cancel
        TextField {
            id: renameField
            anchors.left: parent.left
            anchors.right: parent.right
        }
        onAccepted: {
            if (renameField.text.trim().length > 0)
                CardStore.renameCollection(page.menuId, renameField.text.trim())
        }
    }

    Dialog {
        id: deleteDialog
        title: qsTr("Delete learning box?")
        modal: true
        width: 320
        x: (page.width - width) / 2
        y: Math.max(8, (page.height - height) / 3)
        standardButtons: Dialog.Yes | Dialog.No
        Label {
            anchors.left: parent.left
            anchors.right: parent.right
            wrapMode: Text.WordWrap
            text: qsTr("\"%1\" and all its cards will be deleted. This cannot be undone.").arg(page.menuName)
        }
        onAccepted: CardStore.deleteCollection(page.menuId)
    }
}
