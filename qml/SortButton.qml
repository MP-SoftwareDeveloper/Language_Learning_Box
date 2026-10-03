import QtQuick
import QtQuick.Controls
import QtCore

// Display order of a card list: as added -> A to Z -> Z to A (tap to cycle). Only changes how a list
// is shown; the review ("Start") order is not affected. The choice is remembered for all lists.
Button {
    id: root
    property string order: store.order        // "none" | "az" | "za"
    flat: true
    focusPolicy: Qt.NoFocus
    text: order === "az" ? "A → Z" : order === "za" ? "Z → A" : qsTr("Order: added")
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
