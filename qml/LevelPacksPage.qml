import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Controls.Material
import LearningBox

// The A1 and A2 levels. Each level is its own learning box (also in the box list on the home page),
// with its own boxes 1-5 and progress. Tap a level to see its chapters; "Open box" starts learning it.
Page {
    id: page
    title: qsTr("A1 and A2 levels")

    Component { id: levelPage; LevelPage {} }

    function openBox(levelId) {
        if (LevelPacks.openLevel(levelId))
            page.StackView.view.pop(null) // back to the home page, now showing that box
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Label {
            Layout.fillWidth: true
            Layout.margins: 16
            Layout.bottomMargin: 8
            wrapMode: Text.WordWrap
            opacity: 0.75
            text: qsTr("Six learning boxes, by the lesson themes of “Starten wir!”. Each one has its own Boxes 1–5, so you can track every level separately. Choose one in the list on the home page, or tap “Open box”.")
        }
        Label {
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            visible: LevelPacks.error.length > 0
            wrapMode: Text.WordWrap
            color: Material.color(Material.Red)
            text: LevelPacks.error
        }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: LevelPacks.levels
            ScrollBar.vertical: ScrollBar {}

            delegate: ColumnLayout {
                id: item
                required property var modelData
                required property int index
                width: ListView.view.width
                spacing: 0

                Label {
                    Layout.fillWidth: true
                    Layout.leftMargin: 16
                    Layout.topMargin: 12
                    Layout.bottomMargin: 4
                    visible: item.index === 0 || LevelPacks.levels[item.index - 1].cefr !== item.modelData.cefr
                    text: item.modelData.cefr === "A1" ? qsTr("A1 \u2013 Beginner") : qsTr("A2 \u2013 Elementary")
                    font.pixelSize: 20
                    font.bold: true
                }

                ItemDelegate {
                    Layout.fillWidth: true
                    contentItem: RowLayout {
                        spacing: 12
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            Label {
                                Layout.fillWidth: true
                                text: item.modelData.title
                                font.pixelSize: 17
                                font.bold: true
                                color: Material.accent
                                elide: Text.ElideRight
                            }
                            Label {
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                                opacity: 0.8
                                text: item.modelData.subtitle
                            }
                            Label {
                                Layout.fillWidth: true
                                opacity: 0.7
                                text: item.modelData.boxId < 0
                                      ? qsTr("%1 words \u00B7 box not created yet").arg(item.modelData.total)
                                      : qsTr("%1 words \u00B7 %2 learned \u00B7 %3 due")
                                            .arg(item.modelData.total).arg(item.modelData.learned).arg(item.modelData.due)
                            }
                            ProgressBar {
                                Layout.fillWidth: true
                                from: 0; to: Math.max(1, item.modelData.total)
                                value: item.modelData.learned
                            }
                        }
                        Button {
                            highlighted: true
                            text: qsTr("Open box")
                            onClicked: page.openBox(item.modelData.id)
                        }
                    }
                    onClicked: page.StackView.view.push(levelPage, { levelId: item.modelData.id })
                }
            }
        }
    }
}
