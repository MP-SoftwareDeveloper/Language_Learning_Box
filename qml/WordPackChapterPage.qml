import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Controls.Material
import LearningBox

// Preview of one word-pack chapter (Netzwerk neu A1, or a chapter of a level pack when levelId is set),
// with "add to box" for the whole chapter.
Page {
    id: page
    property string levelId: ""        // "" = the Netzwerk neu A1 pack, else a level pack ("a1-1" ...)
    property int chapterNumber: 0
    property string chapterTitle: ""
    property var words: []

    title: qsTr("K%1 · %2").arg(chapterNumber).arg(chapterTitle)

    function reload() {
        words = levelId !== "" ? LevelPacks.chapterWords(levelId, chapterNumber)
                               : WordPacks.chapterWords(chapterNumber)
    }
    function addChapter() {
        if (levelId !== "") LevelPacks.addChapter(levelId, chapterNumber)
        else WordPacks.addChapter(chapterNumber)
    }
    Component.onCompleted: reload()
    Connections {
        target: WordPacks
        function onChaptersChanged() { page.reload() }
    }
    Connections {
        target: LevelPacks
        function onChanged() { page.reload() }
    }
    Connections {
        target: Translator
        function onSettingsChanged() { page.reload() } // meaning language switched
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
            onClicked: page.addChapter()
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
                            knownTranslation: (Translator.meaningLanguage === "fa" ? row.modelData.exampleFa : row.modelData.exampleEn) ?? ""
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
                    SpeakButton { speakText: row.modelData.front; iconSize: 26 } // compact in long lists
                }
            }
        }
    }
}
