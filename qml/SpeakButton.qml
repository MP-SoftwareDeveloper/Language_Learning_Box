import QtQuick
import QtQuick.Controls
import LearningBox

// Reads `speakText` aloud, by default in the language of the selected learning box
// (German, or American English).
ToolButton {
    id: root
    property string speakText: ""
    property string languageTag: "" // "" = learning language

    // Only the button that started the current speech shows ⏹ (Speaker.utterance is the latest request).
    property int myUtterance: 0
    readonly property bool active: myUtterance !== 0 && Speaker.speaking && Speaker.utterance === myUtterance

    enabled: speakText.trim().length > 0
    contentItem: Label {
        text: root.active ? "\u23F9" : "\uD83D\uDD0A" // emoji stop / speaker: always in the emoji font
        font.pixelSize: 20
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        opacity: root.enabled ? 1 : 0.3
    }
    ToolTip.visible: hovered
    ToolTip.text: qsTr("Read aloud")

    onClicked: {
        if (root.active)
            Speaker.stop()
        else
            root.myUtterance = Speaker.speak(root.speakText, root.languageTag)
    }
}
