import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtCore

// Display order of a card list: as added -> A to Z -> Z to A (tap to cycle). Only changes how a list
// is shown; the review ("Start") order is not affected. The choice is remembered for all lists.
Button {
    id: root
    property string order: store.order        // "none" | "az" | "za"
    flat: true
    focusPolicy: Qt.NoFocus
    // Compact, so the line under it is close
    implicitHeight: 36
    topInset: 0
    bottomInset: 0
    topPadding: 4
    bottomPadding: 4
    text: qsTr("Sort") + ": " + (order === "az" ? "A → Z" : order === "za" ? "Z → A" : qsTr("Default"))
    // Bold and in the accent colour so it is easy to see
    contentItem: Label {
        text: root.text
        font.bold: true
        font.pixelSize: 15
        color: Material.accent
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
    onClicked: {
        order = order === "none" ? "az" : order === "az" ? "za" : "none"
        store.order = order
    }
    Settings {
        id: store
        category: "ui"
        property string order: "none"
    }

    // Leading article / punctuation are ignored: "der Hund" sorts under H.
    function key(card) {
        return String(card.front ?? "").trim().replace(/^[^A-Za-z0-9À-ɏ؀-ۿ]+/, "")
                .replace(/^(der|die|das|ein|eine|the|a|an)\s+/i, "").toLowerCase()
    }
    function apply(list) {
        if (order === "none") return list
        const sorted = list.slice().sort((a, b) => key(a).localeCompare(key(b)))
        if (order === "za") sorted.reverse()
        return sorted
    }
}
