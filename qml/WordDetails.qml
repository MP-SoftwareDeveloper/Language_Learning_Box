import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Controls.Material
import LearningBox

// The same block under the text box in the Dictionary and in Add card (and so for words picked in Lens):
//   - the singular forms of each gender and the plural forms, each with 🔊 and the gender colour
//   - example sentences with a check box: the ticked ones go on the card (one or several, or none)
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

    TextMetrics {
        id: labelWidth
        font.pixelSize: 13
        text: "Singular:        " // 8 spaces
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
                    Layout.preferredWidth: labelWidth.advanceWidth // "Singular:" + 8 spaces
                    text: root.labelFor(modelData.kind) + ":"
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
              ? qsTr("Finding example sentences…") : qsTr("Example sentences – tick one or more for the card")
    }
    Repeater {
        model: root.info ? root.info.examples : []
        delegate: RowLayout {
            id: exRow
            required property var modelData
            required property int index
            readonly property bool picked: root.info !== null && root.info.isPicked(index)
            function toggle() { root.info.toggle(index) }
            Layout.fillWidth: true
            // check box: ticked = this sentence goes on the card (any number of them)
            CheckBox {
                Layout.alignment: Qt.AlignTop
                padding: 0
                topPadding: 2
                checked: exRow.picked
                onToggled: exRow.toggle()
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
