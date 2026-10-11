import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Controls.Material
import LearningBox

// Letters many phone keyboards do not offer (ä ö ü ß): tap one to type it into `target` (a TextField or
// TextArea) at the cursor. Shown only while `target` has the focus, the Settings switch is on and
// `forLanguage` is true (by default: the learning language is German). The buttons never take the
// focus, so the keyboard stays open.
Flow {
    id: root
    property Item target: null
    property bool forLanguage: CardStore.learningLanguage === "de"
    // Typed into the target (the Dictionary starts its lookup after this)
    signal inserted()

    spacing: 4
    visible: target !== null && target.activeFocus && KeyboardPrefs.germanKeys && forLanguage

    Repeater {
        model: ["ä", "ö", "ü", "ß", "Ä", "Ö", "Ü"]
        delegate: Button {
            required property string modelData
            focusPolicy: Qt.NoFocus
            flat: true
            implicitWidth: 44
            implicitHeight: 40
            padding: 0
            text: modelData
            font.pixelSize: 20
            onClicked: {
                if (!root.target)
                    return
                root.target.insert(root.target.cursorPosition, modelData)
                root.inserted()
            }
        }
    }
}
