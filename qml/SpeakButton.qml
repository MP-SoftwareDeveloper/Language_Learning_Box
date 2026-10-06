import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import LearningBox

// Reads `speakText` aloud, by default in the language of the selected learning box
// (German, or American English).
ToolButton {
    id: root
    property string speakText: ""
    property string languageTag: "" // "" = learning language
    // Size of the speaker symbol in pixels (20% smaller than before). Padding keeps the button clear of the edges.
    property int iconSize: 29
    padding: 6
    // Colour of the speaker symbol ("transparent" = the normal emoji colours); used for translations: red
    property color tint: "transparent"
    readonly property bool tinted: tint.a > 0

    // Only the button that started the current speech shows ⏹ (Speaker.utterance is the latest request).
    property int myUtterance: 0
    readonly property bool active: myUtterance !== 0 && Speaker.speaking && Speaker.utterance === myUtterance

    enabled: speakText.trim().length > 0
    contentItem: Item {
        implicitWidth: symbol.implicitWidth
        implicitHeight: symbol.implicitHeight
        opacity: root.enabled ? 1 : 0.3
        Label {
            id: symbol
            anchors.fill: parent
            text: root.active ? "\u23F9" : "\uD83D\uDD0A" // emoji stop / speaker: always in the emoji font
            font.pixelSize: root.iconSize
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            layer.enabled: root.tinted
            visible: !root.tinted
        }
        MultiEffect {
            anchors.fill: symbol
            visible: root.tinted
            source: symbol
            colorization: 1.0
            colorizationColor: root.tint
        }
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
