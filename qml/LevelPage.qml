import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Controls.Material
import LearningBox

// One level (e.g. "A1 \u00B7 Level 1") = one learning box. Shows its four chapters; tap one to see its words.
Page {
    id: page
    property string levelId: ""

    readonly property var level: {
        for (const l of LevelPacks.levels)
            if (l.id === levelId) return l
        return ({ title: "", subtitle: "", total: 0, inBox: 0, learned: 0, due: 0, boxId: -1, chapters: [] })
    }
    title: level.title

    Component { id: chapterPage; WordPackChapterPage {} }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Label {
            Layout.fillWidth: true
            Layout.margins: 16
            Layout.bottomMargin: 8
            wrapMode: Text.WordWrap
            opacity: 0.75
            text: page.level.subtitle + ". " + qsTr("%1 words \u00B7 %2 learned \u00B7 %3 due").arg(page.level.total).arg(page.level.learned).arg(page.level.due)
        }
        Button {
            Layout.fillWidth: true
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            Layout.bottomMargin: 8
            highlighted: true
            text: qsTr("Open this learning box")
            onClicked: if (LevelPacks.openLevel(page.levelId)) page.StackView.view.pop(null)
        }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: page.level.chapters
            ScrollBar.vertical: ScrollBar {}

            delegate: ItemDelegate {
                id: row
                required property var modelData
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
                            wrapMode: Text.WordWrap
                        }
                        Label {
                            Layout.fillWidth: true
                            opacity: 0.7
                            text: qsTr("%1 words").arg(row.modelData.total)
                        }
                    }
                }
                onClicked: page.StackView.view.push(chapterPage, {
                    levelId: page.levelId,
                    chapterNumber: row.modelData.number,
                    chapterTitle: row.modelData.title
                })
            }
        }
    }
}
