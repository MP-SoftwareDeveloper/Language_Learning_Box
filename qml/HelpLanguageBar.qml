import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LearningBox

// English / فارسی / Deutsch switch used by every help and how-to text.
// The choice is shared and remembered (AppMode.helpLanguage); until the person picks one,
// Persian is used when Persian is the translation language in Settings, otherwise English.
RowLayout {
    id: bar
    readonly property string lang: AppMode.helpLanguage !== "" ? AppMode.helpLanguage
                                   : (Translator.targetLanguage === "fa" ? "fa" : "en")
    signal picked(string code)
    spacing: 0

    Repeater {
        model: [ { code: "en", label: "English" },
                 { code: "fa", label: "فارسی" },
                 { code: "de", label: "Deutsch" } ]
        delegate: Button {
            required property var modelData
            readonly property bool selected: bar.lang === modelData.code
            flat: !selected
            highlighted: selected
            focusPolicy: Qt.NoFocus
            text: modelData.label
            onClicked: {
                AppMode.helpLanguage = modelData.code
                bar.picked(modelData.code)
            }
        }
    }
}
