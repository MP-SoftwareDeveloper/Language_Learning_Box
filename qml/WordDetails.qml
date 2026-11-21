import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Controls.Material
import LearningBox

// The same block under the text box in the Dictionary and in Add card (and so for words picked in Lens):
//   - the singular forms of each gender and the plural forms, each with 🔊 and the gender colour
//   - for a verb: the Partizip II ("ist gegangen") with a simple example sentence ("Ich bin gegangen."), both with 🔊
//   - example sentences with a check box: the ticked ones go on the card (one or several, or none)
// It only shows what the WordLookup `info` found. Set Layout margins where it is used.
ColumnLayout {
    id: root
    property var info: null
    property int pixelSize: 16
    spacing: 4

    // Only "Singular" / "Plural": the gender is shown by the colour mark, never as text
    function labelFor(kind) { return kind === "plural" ? qsTr("Plural") : kind === "participle" ? qsTr("Partizip II") : qsTr("Singular") }
    // Plurals are read without "die", several forms with a pause
    function spokenOf(row) {
        if (row.kind === "participle")
            return row.text.replace(/\s*\/\s*/g, ", ").trim() // "ist gefahren, hat gefahren"
        return row.kind === "plural"
                ? row.text.replace(/(^|\/\s*)die\s+/gi, "$1").replace(/\s*\/\s*/g, ", ").trim() : row.text
    }

    TextMetrics {
        id: labelWidth
        font.pixelSize: 13
        text: "Singular:        " // 8 spaces
    }

    // ---- one spelling, two words (noun / verb): the choice is the capital letter ----
    ColumnLayout {
        Layout.fillWidth: true
        visible: root.info !== null && root.info.ambiguous
        spacing: 4
        Label {
            textFormat: Text.PlainText
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            font.pixelSize: 12
            opacity: 0.6
            text: qsTr("This spelling is two different words. Which one do you mean?")
        }
        Flow {
            Layout.fillWidth: true
            spacing: 8
            Button {
                highlighted: root.info !== null && root.info.asNoun
                flat: !highlighted
                text: qsTr("Noun: %1").arg(root.info ? root.info.senseNoun : "")
                onClicked: root.info.chooseSense(true)
            }
            Button {
                highlighted: root.info !== null && !root.info.asNoun
                flat: !highlighted
                text: qsTr("Verb or other: %1").arg(root.info ? root.info.senseKey : "")
                onClicked: root.info.chooseSense(false)
            }
        }
    }

    // ---- forms ----
    ColumnLayout {
        Layout.fillWidth: true
        spacing: 2
        visible: root.info !== null && root.info.rows.length > 0
        Repeater {
            model: root.info ? root.info.rows : []
            delegate: ColumnLayout {
                required property var modelData
                Layout.fillWidth: true
                spacing: 0
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    Label {
                        textFormat: Text.PlainText
                        Layout.preferredWidth: labelWidth.advanceWidth // "Singular:" + 8 spaces
                        text: root.labelFor(modelData.kind) + ":"
                        font.pixelSize: 13
                        opacity: 0.6
                    }
                    Label {
                        textFormat: Text.PlainText
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
                // Separable verb typed as "füge hinzu": the verb it is a form of
                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: labelWidth.advanceWidth + 6
                    visible: (modelData.infinitive ?? "") !== ""
                    spacing: 6
                    Label {
                        textFormat: Text.PlainText
                        Layout.fillWidth: true
                        text: qsTr("Infinitive: %1").arg(modelData.infinitive ?? "")
                        wrapMode: Text.WordWrap
                        font.pixelSize: root.pixelSize
                        opacity: 0.85
                    }
                    SpeakButton {
                        Layout.alignment: Qt.AlignVCenter
                        speakText: modelData.infinitive ?? ""
                        languageTag: "de-DE"
                        iconSize: 17
                    }
                }
                // Verb: the simple example sentence under the participle, with its own 🔊
                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: labelWidth.advanceWidth + 6
                    visible: (modelData.example ?? "") !== ""
                    spacing: 6
                    Label {
                        textFormat: Text.PlainText
                        Layout.fillWidth: true
                        text: modelData.example ?? ""
                        wrapMode: Text.WordWrap
                        font.pixelSize: root.pixelSize
                        opacity: 0.85
                    }
                    SpeakButton {
                        Layout.alignment: Qt.AlignVCenter
                        speakText: modelData.example ?? ""
                        languageTag: "de-DE"
                        iconSize: 17
                    }
                }
            }
        }
    }

    // ---- example sentences ----
    Label {
        textFormat: Text.PlainText
        Layout.topMargin: 4
        visible: root.info !== null && (root.info.examples.length > 0 || root.info.examplesRequest >= 0)
        font.pixelSize: 12
        opacity: 0.6
        text: root.info && root.info.examplesRequest >= 0 && root.info.examples.length === 0
              ? qsTr("Finding example sentences…") : qsTr("Example sentences – tick one or more for the card")
    }
    // The search for sentences finished and found none: say so, with the reason when it failed (no connection,
    // Tatoeba refused or was too slow) - an empty list alone looks the same as "this word has no sentence"
    Label {
        textFormat: Text.PlainText
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        visible: root.info !== null && root.info.examplesAsked && root.info.examples.length === 0
                 && root.info.examplesRequest < 0
        font.pixelSize: 12
        opacity: 0.6
        text: root.info && root.info.examplesError !== ""
              ? qsTr("No example sentences – Tatoeba: %1").arg(root.info.examplesError)
              : qsTr("No example sentences found for this word")
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
                sourceLang: root.info.lang
                knownTranslation: modelData.translation ?? "" // already in the meaning language (see WordLookup)
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
