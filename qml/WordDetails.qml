import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Controls.Material
import LearningBox

// The same block under the text box in the Dictionary and in Add card (and so for words picked in Lens):
//   - the singular forms of each gender and the plural forms, each with 🔊 and the gender colour
//   - example sentences with a radio button: the chosen one goes on the card (tap it again for none)
// It only shows what the WordLookup `info` found. Set Layout margins where it is used.
ColumnLayout {
    id: root
    property var info: null
    property int pixelSize: 16
    spacing: 4

    // Only "Singular" / "Plural": the gender is shown by the colour mark, never as text
    function labelFor(kind) { return kind === "plural" ? qsTr("Plural") : qsTr("Singular") }
    // Plurals are read without "die", several forms with a pause
    function spokenOf(row) {
        return row.kind === "plural"
                ? row.text.replace(/(^|\/\s*)die\s+/gi, "$1").replace(/\s*\/\s*/g, ", ").trim() : row.text
    }

    // ---- forms ----
    ColumnLayout {
        Layout.fillWidth: true
        spacing: 2
        visible: root.info !== null && root.info.rows.length > 0
        Repeater {
            model: root.info ? root.info.rows : []
            delegate: RowLayout {
                required property var modelData
                Layout.fillWidth: true
                spacing: 6
                Label {
                    Layout.preferredWidth: 128
                    text: root.labelFor(modelData.kind)
                    wrapMode: Text.WordWrap
                    font.pixelSize: 13
                    opacity: 0.6
                }
                Label {
                    Layout.fillWidth: true
                    text: modelData.text
                    wrapMode: Text.WordWrap
                    font.pixelSize: root.pixelSize + 1
                    font.bold: true
                }
                GenderMark { word: modelData.mark }
                SpeakButton {
                    Layout.alignment: Qt.AlignVCenter
                    speakText: root.spokenOf(modelData)
                    languageTag: "de-DE"
                    iconSize: 19
                }
            }
        }
    }

    // ---- example sentences ----
    Label {
        Layout.topMargin: 4
        visible: root.info !== null && (root.info.examples.length > 0 || root.info.examplesRequest >= 0)
        font.pixelSize: 12
        opacity: 0.6
        text: root.info && root.info.examplesRequest >= 0 && root.info.examples.length === 0
              ? qsTr("Finding example sentences…") : qsTr("Example sentences – choose one for the card")
    }
    Repeater {
        model: root.info ? root.info.examples : []
        delegate: RowLayout {
            id: exRow
            required property var modelData
            required property int index
            readonly property bool picked: root.info !== null && root.info.picked === index
            function toggle() { root.info.toggle(index) }
            Layout.fillWidth: true
            // radio button: filled dot = this sentence goes on the card
            Rectangle {
                Layout.alignment: Qt.AlignTop
                Layout.topMargin: 3
                implicitWidth: 22; implicitHeight: 22
                radius: 11
                color: "transparent"
                border.width: 2
                border.color: exRow.picked ? Material.accent : Qt.alpha(Material.foreground, 0.5)
                Rectangle {
                    anchors.centerIn: parent
                    width: 10; height: 10; radius: 5
                    color: Material.accent
                    visible: exRow.picked
                }
                TapHandler { onTapped: exRow.toggle() }
            }
            ExampleText {
                Layout.fillWidth: true
                example: modelData.text
                target: root.info.meaning
                knownTranslation: root.info.meaning === Translator.meaningLanguage ? (modelData.translation ?? "") : ""
                pixelSize: 15
                TapHandler { onTapped: exRow.toggle() }
            }
            SpeakButton {
                Layout.alignment: Qt.AlignTop
                speakText: modelData.text
            }
        }
    }
}
