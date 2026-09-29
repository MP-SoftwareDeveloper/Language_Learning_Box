import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Controls.Material
import QtQuick.Dialogs
import LearningBox

// Get cards from a file or a link: LearningBox (.lbox), CSV/text (Excel, Quizlet, Anki text)
// and, in Full mode, Anki decks (.apkg) and links. A preview in plain words, options with hints.
Page {
    id: page
    title: qsTr("Get cards")

    readonly property var preview: DeckExchange.preview
    readonly property bool hasPreview: preview.count !== undefined || (preview.error ?? "") !== ""
    readonly property bool ready: (preview.error ?? "") === "" && (preview.count ?? 0) > 0
    readonly property bool intoNewBox: intoNew.checked
    // New cards = all when they go into a new learning box.
    readonly property int addCount: intoNewBox ? (preview.count ?? 0) : (preview.newCount ?? 0)
    readonly property int updateCount: intoNewBox || skipExisting.checked ? 0 : (preview.existingCount ?? 0)
    property string message: ""
    property bool messageOk: true
    property bool imported: false
    function say(text, ok) { message = text; messageOk = ok }

    // New file: a new learning box named after the file is the default when the file has a title.
    Connections {
        target: DeckExchange
        function onPreviewChanged() {
            const title = DeckExchange.preview.title ?? ""
            newBoxName.text = title
            if (title !== "")
                intoNew.checked = true
            else
                intoCurrent.checked = true
            swapSides.checked = false
            skipExisting.checked = true
        }
    }
    StackView.onRemoved: DeckExchange.clearPreview()

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth

        ColumnLayout {
            width: parent.width
            spacing: 4

            // ---- Where from ----
            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.topMargin: 8
                visible: !page.hasPreview
                spacing: 4

                Button {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 56
                    highlighted: true
                    enabled: !DeckExchange.busy
                    text: qsTr("Choose a file")
                    onClicked: { page.message = ""; openDialog.open() }
                }
                HintLabel {
                    text: AppMode.full ? qsTr("Files ending in .lbox (LearningBox), .csv or .txt (Excel, Quizlet) or .apkg (Anki).")
                                       : qsTr("Files ending in .lbox (LearningBox), .csv or .txt (Excel, Quizlet).")
                }

                Label {
                    Layout.topMargin: 16
                    visible: AppMode.full
                    text: qsTr("… or paste a link")
                    font.bold: true
                }
                RowLayout {
                    Layout.fillWidth: true
                    visible: AppMode.full
                    TextField {
                        id: linkField
                        Layout.fillWidth: true
                        placeholderText: "https://drive.google.com/file/d/…"
                        inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
                        onAccepted: if (text.trim() !== "") { page.message = ""; DeckExchange.openLink(text) }
                    }
                    Button {
                        text: qsTr("Get")
                        enabled: !DeckExchange.busy && linkField.text.trim() !== ""
                        onClicked: { page.message = ""; DeckExchange.openLink(linkField.text) }
                    }
                }
                HintLabel {
                    visible: AppMode.full
                    text: qsTr("A Google Drive, Dropbox or GitHub link. In Google Drive choose Share → “Anyone with the link” first.")
                }
            }

            BusyIndicator {
                Layout.alignment: Qt.AlignHCenter
                visible: DeckExchange.busy
                running: visible
            }

            // ---- Preview + options ----
            Pane {
                Layout.fillWidth: true
                Layout.leftMargin: 12
                Layout.rightMargin: 12
                Layout.topMargin: 8
                visible: page.hasPreview
                Material.elevation: 2

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 4

                    Label {
                        Layout.fillWidth: true
                        visible: (page.preview.error ?? "") !== ""
                        wrapMode: Text.WordWrap
                        color: Material.color(Material.Red)
                        text: page.preview.error ?? ""
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        visible: page.ready
                        spacing: 4

                        Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            font.bold: true
                            font.pixelSize: 16
                            text: (page.preview.title ? "“" + page.preview.title + "” · " : "")
                                  + qsTr("%n card(s)", "", page.preview.count ?? 0)
                                  + ((page.preview.pictures ?? 0) > 0 ? " · " + qsTr("%n picture(s)", "", page.preview.pictures) : "")
                        }
                        // First cards, so the user can check which side is German
                        Repeater {
                            model: page.preview.sample ?? []
                            delegate: Label {
                                required property var modelData
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                                opacity: 0.8
                                text: "• " + (swapSides.checked ? modelData.back.split("\n")[0] + "  →  " + modelData.front
                                                                    : modelData.front + "  →  " + modelData.back.split("\n")[0])
                            }
                        }
                        CheckBox {
                            id: swapSides
                            visible: page.preview.format !== "lbox"
                            text: qsTr("German is in the second column")
                        }
                        HintLabel {
                            visible: swapSides.visible
                            leftPadding: 8
                            text: qsTr("Tick this if the lines above show the meaning first.")
                        }

                        Label {
                            Layout.topMargin: 8
                            text: qsTr("Where should the cards go?")
                            font.bold: true
                        }
                        ButtonGroup { id: targetGroup }
                        RadioButton {
                            id: intoNew
                            ButtonGroup.group: targetGroup
                            text: qsTr("A new learning box")
                        }
                        TextField {
                            id: newBoxName
                            visible: intoNew.checked
                            Layout.fillWidth: true
                            Layout.leftMargin: 8
                            placeholderText: qsTr("Name, e.g. Netzwerk neu A2")
                        }
                        RadioButton {
                            id: intoCurrent
                            ButtonGroup.group: targetGroup
                            checked: true
                            text: qsTr("This learning box: %1").arg(CardStore.currentCollectionName)
                        }
                        HintLabel {
                            visible: intoCurrent.checked
                            leftPadding: 8
                            text: qsTr("%1 new · %2 you already have").arg(page.preview.newCount ?? 0).arg(page.preview.existingCount ?? 0)
                        }

                        Label {
                            Layout.topMargin: 8
                            visible: intoCurrent.checked && (page.preview.existingCount ?? 0) > 0
                            text: qsTr("Words you already have")
                            font.bold: true
                        }
                        ButtonGroup { id: dupGroup }
                        RadioButton {
                            id: skipExisting
                            visible: intoCurrent.checked && (page.preview.existingCount ?? 0) > 0
                            ButtonGroup.group: dupGroup
                            checked: true
                            text: qsTr("Keep mine")
                        }
                        RadioButton {
                            visible: intoCurrent.checked && (page.preview.existingCount ?? 0) > 0
                            ButtonGroup.group: dupGroup
                            text: qsTr("Use the file's meaning and example")
                        }
                        HintLabel {
                            visible: intoCurrent.checked && (page.preview.existingCount ?? 0) > 0
                            leftPadding: 8
                            text: qsTr("Your progress for these words is kept either way.")
                        }

                        CheckBox {
                            id: keepProgress
                            Layout.topMargin: 4
                            visible: page.preview.hasProgress === true
                            checked: true
                            text: qsTr("Keep the progress from the file")
                        }
                        HintLabel {
                            visible: keepProgress.visible
                            leftPadding: 8
                            text: qsTr("On: cards go into the boxes they had. Off: they start in the box below.")
                        }
                        RowLayout {
                            Layout.topMargin: 4
                            visible: !(keepProgress.visible && keepProgress.checked)
                            Label { text: qsTr("Start in") }
                            ComboBox {
                                id: importBox
                                Layout.preferredWidth: 160
                                model: [qsTr("Box 1"), qsTr("Box 2"), qsTr("Box 3"), qsTr("Box 4"), qsTr("Box 5"), qsTr("Learned")]
                            }
                        }
                        HintLabel {
                            visible: !(keepProgress.visible && keepProgress.checked)
                            leftPadding: 8
                            text: qsTr("New cards are asked from this box.")
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.topMargin: 8
                        Button {
                            flat: true
                            text: qsTr("Choose another file")
                            onClicked: DeckExchange.clearPreview()
                        }
                        Item { Layout.fillWidth: true }
                        Button {
                            highlighted: true
                            visible: page.ready
                            enabled: page.addCount + page.updateCount > 0
                            text: page.updateCount > 0 ? qsTr("Add %1 · update %2").arg(page.addCount).arg(page.updateCount)
                                                       : qsTr("Add %n card(s)", "", page.addCount)
                            onClicked: {
                                const r = DeckExchange.applyImport(skipExisting.checked ? "skip" : "update",
                                                                   keepProgress.visible && keepProgress.checked,
                                                                   importBox.currentIndex + 1, swapSides.checked,
                                                                   intoNew.checked ? (newBoxName.text.trim() || qsTr("Imported cards")) : "")
                                if (r.error) {
                                    page.say(r.error, false)
                                } else {
                                    page.say(qsTr("Added %1 · updated %2 · skipped %3").arg(r.added).arg(r.updated).arg(r.skipped), true)
                                    page.imported = true
                                    DeckExchange.clearPreview()
                                }
                            }
                        }
                    }
                }
            }

            Label {
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.topMargin: 8
                Layout.fillWidth: true
                visible: page.message !== ""
                wrapMode: Text.WordWrap
                color: page.messageOk ? Material.color(Material.Green) : Material.color(Material.Red)
                text: page.message
            }
            Button {
                Layout.leftMargin: 16
                visible: page.imported
                text: qsTr("Open “%1”").arg(CardStore.currentCollectionName)
                onClicked: page.StackView.view.pop(null)
            }
            Item { Layout.preferredHeight: 16 }
        }
    }

    FileDialog {
        id: openDialog
        title: qsTr("Choose a card file")
        fileMode: FileDialog.OpenFile
        // Android matches filters by file type; .lbox/.apkg are unknown there, so show all files.
        nameFilters: Qt.platform.os === "android" ? []
                     : [qsTr("Card files (*.lbox *.apkg *.csv *.txt *.tsv)"), qsTr("All files (*)")]
        onAccepted: {
            page.imported = false
            DeckExchange.openFile(selectedFile)
        }
    }
}
