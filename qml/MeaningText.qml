import QtQuick
import QtQuick.Controls

// Multi-line meaning text where each line keeps its own writing direction:
// a Persian line is laid out right-to-left, a German grammar note ("Pl. die Äpfel")
// left-to-right. A plain Label would force one direction on all lines.
Column {
    id: root
    property string text: ""
    property int pixelSize: 16
    property bool center: false   // center every line, otherwise align by its direction

    spacing: 2

    Repeater {
        model: root.text.length > 0 ? root.text.replace(/\s*·\s*Pl\./, "\nPlural").split("\n") : []
        delegate: Label {
            required property string modelData
            required property int index
            width: root.width
            // old cards: "Pl. ...", "Sg. ...", "Mask. ...", "Fem. ..." are shown without the gender word
            text: modelData.replace(/^\s*(?:Mask|Fem)\.\s+(?:Pl\.|Plural)\s*/, "Plural ")
                           .replace(/^\s*(?:Mask|Fem)\.\s+/, "Singular ")
                           .replace(/^\s*Pl\.\s*/, "Plural ").replace(/^\s*Sg\.\s*/, "Singular ")
            wrapMode: Text.WordWrap
            font.pixelSize: index === 0 ? root.pixelSize : Math.round(root.pixelSize * 0.8)
            opacity: index === 0 ? 1.0 : 0.7
            horizontalAlignment: root.center ? Text.AlignHCenter : undefined
        }
    }
}
