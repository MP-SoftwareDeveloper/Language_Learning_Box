import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Controls.Material
import LearningBox

// Built-in vocabulary: one row per chapter, add a chapter to the box or preview its words.
Page {
    id: page
    title: qsTr("Word packs")

    Component { id: chapterPage; WordPackChapterPage {} }
    Component { id: levelsPage; LevelPacksPage {} }

    function addedMessage(n) {
        return n > 0 ? qsTr("%n word(s) added to Box 1", "", n)
                     : qsTr("These words are already in your box")
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Starter words: 100 common words with English and Persian meanings
        Pane {
            Layout.fillWidth: true
            Layout.margins: 12
            Layout.bottomMargin: 0
            Material.elevation: 1
            RowLayout {
                anchors.fill: parent
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        font.bold: true
                        text: qsTr("%1 starter words").arg(WordPacks.starterTotal)
                    }
                    HintLabel {
                        text: qsTr("Everyday words with English and Persian meanings. %1 of %2 are in this learning box.")
                              .arg(WordPacks.starterInBox).arg(WordPacks.starterTotal)
                    }
                }
                Button {
                    enabled: WordPacks.starterInBox < WordPacks.starterTotal
                    text: WordPacks.starterInBox < WordPacks.starterTotal ? qsTr("Add") : qsTr("Added")
                    onClicked: WordPacks.addStarterCards()
                }
            }
        }

        // A1 and A2 levels (3 levels each), themes of the course "Starten wir!"
        Pane {
            Layout.fillWidth: true
            Layout.margins: 12
            Layout.bottomMargin: 0
            Material.elevation: 1
            RowLayout {
                anchors.fill: parent
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        font.bold: true
                        text: qsTr("A1 and A2 levels")
                    }
                    HintLabel {
                        text: qsTr("%1 words and phrases in 6 levels (3 for A1, 3 for A2). Each level is its own learning box, with English and Persian meanings and example sentences.")
                              .arg(LevelPacks.totalWords)
                    }
                }
                Button {
                    highlighted: true
                    text: qsTr("Open")
                    onClicked: page.StackView.view.push(levelsPage)
                }
            }
        }

        Label {
            Layout.fillWidth: true
            Layout.margins: 16
            Layout.bottomMargin: 4
            text: WordPacks.title
            font.pixelSize: 20
            font.bold: true
        }
        Label {
            Layout.fillWidth: true
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            Layout.bottomMargin: 8
            wrapMode: Text.WordWrap
            opacity: 0.75
            text: qsTr("A1 words grouped by the course's chapter themes, with Persian meanings and example sentences. "
                       + "%1 of %2 words are in your box. Tip: add one chapter at a time, "
                       + "new words start in Box 1.").arg(WordPacks.wordsInBox).arg(WordPacks.totalWords)
        }
        Label {
            Layout.leftMargin: 16
            visible: WordPacks.error.length > 0
            color: Material.color(Material.Red)
            text: WordPacks.error
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: WordPacks.chapters
            ScrollBar.vertical: ScrollBar {}

            delegate: ItemDelegate {
                id: row
                required property var modelData
                readonly property bool complete: modelData.inBox >= modelData.total
                width: ListView.view.width

                contentItem: RowLayout {
                    spacing: 12
                    Label {
                        text: row.modelData.number
                        font.pixelSize: 18
                        font.bold: true
                        color: Material.accent
                        Layout.preferredWidth: 28
                        horizontalAlignment: Text.AlignHCenter
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Label {
                            Layout.fillWidth: true
                            text: row.modelData.title
                            font.pixelSize: 16
                            font.bold: true
                            elide: Text.ElideRight
                        }
                        Label {
                            Layout.fillWidth: true
                            opacity: 0.7
                            text: qsTr("%1 words · %2 in your box").arg(row.modelData.total).arg(row.modelData.inBox)
                        }
                        ProgressBar {
                            Layout.fillWidth: true
                            from: 0; to: row.modelData.total
                            value: row.modelData.inBox
                        }
                    }
                    Button {
                        flat: row.complete
                        highlighted: !row.complete
                        enabled: !row.complete
                        text: row.complete ? qsTr("Added") : qsTr("Add")
                        onClicked: toast.show(page.addedMessage(WordPacks.addChapter(row.modelData.number)))
                    }
                }
                onClicked: page.StackView.view.push(chapterPage, {
                    chapterNumber: row.modelData.number,
                    chapterTitle: row.modelData.title
                })
            }

            footer: Button {
                width: ListView.view.width - 32
                x: 16
                flat: true
                enabled: WordPacks.wordsInBox < WordPacks.totalWords
                text: qsTr("Add all chapters")
                onClicked: toast.show(page.addedMessage(WordPacks.addAll()))
            }
        }
    }

    ToolTip {
        id: toast
        function show(msg) { text = msg; open() }
        timeout: 2000
        x: (parent.width - width) / 2
        y: parent.height - height - 24
    }
}
