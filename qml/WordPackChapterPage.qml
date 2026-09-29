import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Controls.Material
import LearningBox

// Preview of one word-pack chapter, with "add to box" for the whole chapter.
Page {
    id: page
    property int chapterNumber: 0
    property string chapterTitle: ""
    property var words: []

    title: qsTr("K%1 · %2").arg(chapterNumber).arg(chapterTitle)

    function reload() { words = WordPacks.chapterWords(chapterNumber) }
    Component.onCompleted: reload()
    Connections {
        target: WordPacks
        function onChaptersChanged() { page.reload() }
    }

    readonly property int missing: {
        let n = 0
        for (const w of words) if (!w.inBox) ++n
        return n
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Button {
            Layout.fillWidth: true
            Layout.margins: 12
            highlighted: true
            enabled: page.missing > 0
            text: page.missing > 0 ? qsTr("Add %n word(s) to my box", "", page.missing)
                                   : qsTr("All words of this chapter are in your box")
            onClicked: WordPacks.addChapter(page.chapterNumber)
        }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: page.words
            ScrollBar.vertical: ScrollBar {}

            delegate: ItemDelegate {
                id: row
                required property var modelData
                width: ListView.view.width

                contentItem: RowLayout {
                    spacing: 8
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Label {
                            Layout.fillWidth: true
                            text: row.modelData.front
                            font.pixelSize: 17
                            font.bold: true
                            wrapMode: Text.WordWrap
                        }
                        // Persian meaning (RTL) + German grammar note (LTR), one line each.
                        MeaningText {
                            Layout.fillWidth: true
                            text: row.modelData.back
                            pixelSize: 15
                        }
                        ExampleText {
                            Layout.fillWidth: true
                            example: row.modelData.example
                            knownTranslation: row.modelData.exampleFa ?? ""
                            onlineLookup: false
                            pixelSize: 14
                            germanOpacity: 0.6
                        }
                    }
                    Label {
                        visible: row.modelData.inBox
                        text: "✅"
                        ToolTip.text: qsTr("In your box")
                    }
                    SpeakButton { speakText: row.modelData.front }
                }
            }
        }
    }
}
