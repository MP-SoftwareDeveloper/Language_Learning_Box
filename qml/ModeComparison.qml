import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Controls.Material

// What Simple and Full mode offer (first-run setup and Settings).
GridLayout {
    id: grid
    columns: 3
    columnSpacing: 8
    rowSpacing: 4

    readonly property var features: [
        { text: qsTr("Cards: add, edit, delete, search"), simple: true },
        { text: qsTr("Leitner review, boxes 1–5 + Learned"), simple: true },
        { text: qsTr("Several learning boxes, start over"), simple: true },
        { text: qsTr("German speech"), simple: true },
        { text: qsTr("100 starter words (German, English, Persian)"), simple: true },
        { text: qsTr("Send and receive cards (.lbox, CSV)"), simple: true },
        { text: qsTr("Works offline, asks for no permissions"), simple: true, full: false },
        { text: qsTr("Lens: words from a photo"), simple: false },
        { text: qsTr("Handwriting recognition (free Azure key)"), simple: false },
        { text: qsTr("Meaning filled in, word suggestions while typing"), simple: false },
        { text: qsTr("Example sentences (word pack, Tatoeba)"), simple: false },
        { text: qsTr("Online translation"), simple: false },
        { text: qsTr("Word pack Netzwerk neu A1 (597 words)"), simple: false },
        { text: qsTr("Pictures on cards (gallery, camera)"), simple: false },
        { text: qsTr("Anki decks, cards from a link"), simple: false }
    ]

    Label { text: ""; Layout.fillWidth: true }
    Label { text: qsTr("Simple"); font.bold: true; Layout.alignment: Qt.AlignHCenter }
    Label { text: qsTr("Full"); font.bold: true; Layout.alignment: Qt.AlignHCenter }

    Repeater {
        model: grid.features.length * 3
        delegate: Label {
            required property int index
            readonly property var f: grid.features[Math.floor(index / 3)]
            readonly property int column: index % 3
            Layout.fillWidth: column === 0
            Layout.alignment: column === 0 ? Qt.AlignLeft : Qt.AlignHCenter
            wrapMode: column === 0 ? Text.WordWrap : Text.NoWrap
            font.pixelSize: column === 0 ? 14 : 16
            readonly property bool yes: column === 1 ? f.simple : (f.full ?? true)
            text: column === 0 ? f.text : (yes ? "✓" : "–")
            color: column === 0 ? Material.foreground : (yes ? Material.color(Material.Green) : Material.hintTextColor)
        }
    }
}
