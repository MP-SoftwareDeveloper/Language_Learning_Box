import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// A SwitchDelegate whose text wraps instead of making the whole page wider than the screen
// (long labels, e.g. in German, otherwise push the switch and the texts below off the right edge).
SwitchDelegate {
    id: control
    Layout.minimumWidth: 0 // the label wraps, so it never needs more than the page width
    Layout.preferredWidth: 100 // not the unwrapped text width: it must not make the page wider
    contentItem: Label {
        textFormat: Text.PlainText
        text: control.text
        font: control.font
        wrapMode: Text.WordWrap
        verticalAlignment: Text.AlignVCenter
        opacity: control.enabled ? 1.0 : 0.38
        leftPadding: control.mirrored && control.indicator ? control.indicator.width + control.spacing : 0
        rightPadding: !control.mirrored && control.indicator ? control.indicator.width + control.spacing : 0
    }
}
