import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Controls.Material
import LearningBox

// One Leitner box: every card in it, with buttons to move a card to the previous or next box.
// Tabs switch between boxes 1-5 and Learned (6). ★ adds a card to Favorite words.
// Every card has a check box: tick several cards (or "Select all"), then move them one box back or
// forward, star or delete them.
Page {
    id: page
    property int box: 1
    title: BoxNames.name(box)

    property var cards: []
    property var lastMove: null // {label, to, items: [{id, from}]} for Undo

    // Selection mode: {id: true} of the checked cards
    property bool selecting: false
    property var checked: ({})
    readonly property int checkedCount: Object.keys(checked).length
    readonly property var checkedIds: Object.keys(checked).map(k => Number(k))
    function isChecked(id) { return checked[id] === true }
    function toggle(id) {
        const c = Object.assign({}, checked)
        if (c[id]) delete c[id]; else c[id] = true
        checked = c
        selecting = Object.keys(c).length > 0 // the selection bar shows while something is ticked
    }
    function selectAll(on) {
        const c = {}
        if (on) for (const card of cards) c[card.id] = true
        checked = c
        selecting = on && cards.length > 0
    }
    function stopSelecting() { selecting = false; checked = {} }

    function reload() {
        // A new list resets the ListView to the top: keep the scroll position (star, delete, move)
        const y = list.contentY
        cards = sortButton.apply(CardStore.cardsInBox(box))
        list.forceLayout()
        list.contentY = Math.max(list.originY, Math.min(y, list.originY + list.contentHeight - list.height))
        // Drop checks of cards that left this box
        const c = {}
        for (const card of cards) if (checked[card.id]) c[card.id] = true
        if (Object.keys(c).length !== checkedCount) checked = c
        if (Object.keys(c).length === 0) selecting = false
    }
    onBoxChanged: { stopSelecting(); reload(); list.positionViewAtBeginning() } // another box: from the top
    Component.onCompleted: reload()
    Connections {
        target: CardStore
        function onChanged() { page.reload() }
    }

    StackView.onDeactivating: Speaker.stop()

    function boxName(b) { return BoxNames.name(b) }
    function move(card, to) {
        const from = card.box, id = card.id, front = card.front // before the list reloads
        if (CardStore.moveCard(id, to))
            showUndo(front, to, [{ id: id, from: from }])
    }
    function showUndo(label, to, items) {
        lastMove = { label: label, to: to, items: items }
        undoBar.open()
        undoTimer.restart() // also when a second card is moved while the bar is showing
    }
    // The ticked cards, one box back (delta -1) or forward (+1)
    function moveSelected(delta) {
        const items = []
        let to = 0
        for (const card of cards) {
            if (!checked[card.id]) continue
            const target = Math.max(1, Math.min(6, card.box + delta))
            if (target === card.box) continue
            if (CardStore.moveCard(card.id, target)) {
                items.push({ id: card.id, from: card.box })
                to = target
            }
        }
        stopSelecting()
        if (items.length > 0)
            showUndo(qsTr("%n card(s)", "", items.length), to, items)
    }
    function dueText(card) {
        if (card.box > 5 || !card.dueAt) return ""
        const days = Math.round((new Date(card.dueAt).setHours(0, 0, 0, 0) - new Date().setHours(0, 0, 0, 0)) / 86400000)
        if (days <= 0) return qsTr("due today")
        if (days === 1) return qsTr("due tomorrow")
        return qsTr("due in %n day(s)", "", days)
    }

    Component { id: editPage; CardEditPage {} }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        TabBar {
            id: tabs
            Layout.fillWidth: true
            currentIndex: page.box - 1
            onCurrentIndexChanged: page.box = currentIndex + 1
            Repeater {
                model: 6
                TabButton {
                    required property int index
                    readonly property int count: index < 5 ? (CardStore.boxCounts[index] ?? 0) : CardStore.learnedCount
                    text: BoxNames.code(index + 1) + " (" + count + ")"
                    font.pixelSize: 13
                    leftPadding: 2
                    rightPadding: 2
                }
            }
        }

        Label {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.maximumWidth: page.width - 24 // never wider than the page (wraps instead)
            Layout.margins: 12
            Layout.bottomMargin: 4
            wrapMode: Text.WordWrap
            opacity: 0.7
            text: page.box > 5
                  ? qsTr("Learned cards are no longer reviewed. Move one back to %1 to practise it again.").arg(BoxNames.name(5))
                  : qsTr("Reviewed every %n day(s). \u2039 moves a card back, \u203A moves it forward; it is then due after that box's interval. Tap a card to edit it; tick the boxes to select several cards.",
                         "", Math.pow(2, page.box - 1))
        }

        // A Flow, not a RowLayout: buttons in a RowLayout cannot shrink, so a long label ("Move all 101
        // card(s) to New (reg1)", a big system font) made this row wider than the page - and a
        // ColumnLayout lays everything out at its minimum width when that is wider than the page,
        // which pushed the whole page (hint, cards, ‹ ›) past the right edge. A Flow wraps instead.
        Flow {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.leftMargin: 8
            Layout.rightMargin: 8
            spacing: 0
            visible: !page.selecting && page.cards.length > 0
            SortButton { id: sortButton; objectName: "sortButton"; onOrderChanged: page.reload() }
            Button {
                visible: page.box > 1
                flat: true
                text: qsTr("Move all %n card(s) to %1", "", page.cards.length).arg(BoxNames.name(1))
                onClicked: resetDialog.open()
            }
            Button {
                objectName: "selectButton"
                flat: true
                text: qsTr("Select all")
                onClicked: page.selectAll(true)
            }
        }
        // Light-blue line under the Sort row, edge to edge
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 12
            color: "#4FC3F7"
            visible: !page.selecting && page.cards.length > 0
        }

        // Selection bar: all / count / move back / move forward / ★ / delete / done
        Pane {
            objectName: "selectionBar"
            Layout.fillWidth: true
            visible: page.selecting
            padding: 4
            Material.elevation: 2
            RowLayout {
                anchors.fill: parent
                spacing: 0
                CheckBox {
                    objectName: "selectAll"
                    checkable: false // follows the selection, not its own state
                    checkState: page.checkedCount === 0 ? Qt.Unchecked
                              : page.checkedCount === page.cards.length ? Qt.Checked : Qt.PartiallyChecked
                    onClicked: page.selectAll(page.checkedCount < page.cards.length)
                }
                Label {
                    Layout.fillWidth: true
                    text: qsTr("%n selected", "", page.checkedCount)
                    elide: Text.ElideRight
                }
                ToolButton {
                    objectName: "moveSelectedBack"
                    enabled: page.checkedCount > 0 && page.box > 1
                    opacity: enabled ? 1 : 0.25
                    text: "\u2039" // ‹
                    font.pixelSize: 48
                    font.bold: true
                    implicitWidth: 44
                    implicitHeight: 56
                    padding: 0
                    ToolTip.visible: hovered || pressed
                    ToolTip.text: qsTr("Move selected to %1").arg(page.boxName(page.box - 1))
                    onClicked: page.moveSelected(-1)
                }
                ToolButton {
                    objectName: "moveSelectedForward"
                    enabled: page.checkedCount > 0 && page.box < 6
                    opacity: enabled ? 1 : 0.25
                    text: "\u203A" // ›
                    font.pixelSize: 48
                    font.bold: true
                    implicitWidth: 44
                    implicitHeight: 56
                    padding: 0
                    ToolTip.visible: hovered || pressed
                    ToolTip.text: qsTr("Move selected to %1").arg(page.boxName(page.box + 1))
                    onClicked: page.moveSelected(1)
                }
                ToolButton {
                    objectName: "starSelected"
                    enabled: page.checkedCount > 0
                    contentItem: Item {
                        implicitWidth: 22
                        implicitHeight: 22
                        StarIcon {
                            anchors.centerIn: parent
                            width: 22
                            height: 22
                            filled: true
                            opacity: parent.parent.enabled ? 1 : 0.35
                        }
                    }
                    ToolTip.visible: hovered || pressed
                    ToolTip.text: qsTr("Add to Favorite words")
                    onClicked: {
                        for (const id of page.checkedIds)
                            CardStore.setFavorite(id, true)
                        page.stopSelecting()
                    }
                }
                Button {
                    objectName: "deleteSelected"
                    flat: true
                    enabled: page.checkedCount > 0
                    text: qsTr("Delete")
                    Material.foreground: Material.color(Material.Red)
                    onClicked: confirmDelete.open()
                }
                Button {
                    flat: true
                    text: qsTr("Done")
                    onClicked: page.stopSelecting()
                }
            }
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: page.cards
            bottomMargin: 88 // keep the last card clear of the + button
            ScrollBar.vertical: ScrollBar {}

            delegate: ItemDelegate {
                id: row
                required property var modelData
                width: ListView.view.width
                leftPadding: 12
                rightPadding: 4
                topPadding: 8
                bottomPadding: 8
                // Thin line between cards
                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: 12
                    height: 1
                    color: Material.foreground
                    opacity: 0.12
                }

                // The whole card: front, full meaning and example, each with 🔊
                contentItem: RowLayout {
                    spacing: 6
                    CheckBox {
                        Layout.alignment: Qt.AlignTop
                        checkable: false
                        checked: page.isChecked(row.modelData.id)
                        onClicked: page.toggle(row.modelData.id)
                        padding: 0
                    }
                    CardFace {
                        Layout.fillWidth: true
                        card: row.modelData
                        note: page.dueText(row.modelData)
                        StarButton {
                            implicitWidth: 36
                            cardId: row.modelData.id
                        }
                        ToolButton {
                            visible: !page.selecting
                            enabled: row.modelData.box > 1
                            opacity: enabled ? 1 : 0.25
                            text: "\u2039" // ‹ (◀ is drawn as an emoji on Android)
                            font.pixelSize: 60
                            font.bold: true
                            implicitWidth: 50
                            implicitHeight: 72
                            padding: 0
                            ToolTip.visible: hovered || pressed
                            ToolTip.text: qsTr("Move to %1").arg(page.boxName(row.modelData.box - 1))
                            onClicked: page.move(row.modelData, row.modelData.box - 1)
                        }
                        ToolButton {
                            visible: !page.selecting
                            enabled: row.modelData.box < 6
                            opacity: enabled ? 1 : 0.25
                            text: "\u203A" // ›
                            font.pixelSize: 60
                            font.bold: true
                            implicitWidth: 50
                            implicitHeight: 72
                            padding: 0
                            ToolTip.visible: hovered || pressed
                            ToolTip.text: qsTr("Move to %1").arg(page.boxName(row.modelData.box + 1))
                            onClicked: page.move(row.modelData, row.modelData.box + 1)
                        }
                    }
                }
                highlighted: page.selecting && page.isChecked(row.modelData.id)
                onClicked: {
                    if (page.selecting)
                        page.toggle(row.modelData.id)
                    else
                        page.StackView.view.push(editPage, { cardId: row.modelData.id })
                }
                onPressAndHold: {
                    if (!page.selecting) {
                        page.selecting = true
                        page.checked = {}
                    }
                    page.toggle(row.modelData.id)
                }
            }

            Label {
                anchors.centerIn: parent
                visible: list.count === 0
                text: qsTr("This box is empty")
                opacity: 0.6
            }
        }
    }

    // Delete the selected cards (with confirmation)
    Dialog {
        id: confirmDelete
        // Fixed width + x/y: a size bound to the page (anchors/width) made a layout loop with the
        // wrapping label, because the page's own size also looks at this dialog. 320 fits any phone.
        x: (page.width - width) / 2
        y: (page.height - height) / 2
        width: 320
        modal: true
        title: qsTr("Delete %n card(s)?", "", page.checkedCount)
        standardButtons: Dialog.Yes | Dialog.No
        Label {
            width: confirmDelete.availableWidth
            wrapMode: Text.WordWrap
            text: page.cards.filter(c => page.isChecked(c.id)).slice(0, 5).map(c => c.front).join(", ")
                  + (page.checkedCount > 5 ? " …" : "") + "\n\n"
                  + qsTr("The cards, their progress and their pictures are removed. This cannot be undone.")
        }
        onAccepted: {
            CardStore.removeCards(page.checkedIds)
            page.stopSelecting()
        }
    }

    ResetDialog { id: resetDialog; box: page.box }

    // Add a card straight into this box
    RoundButton {
        visible: !page.selecting
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 20
        width: 60
        height: 60
        highlighted: true
        text: "+"
        font.pixelSize: 28
        onClicked: page.StackView.view.push(editPage, { initialBox: page.box })
    }

    // "Moved … · Undo" bar
    Popup {
        id: undoBar
        x: 12
        y: page.height - height - 12
        width: page.width - 24
        padding: 8
        closePolicy: Popup.NoAutoClose
        Timer { id: undoTimer; interval: 4000; onTriggered: undoBar.close() }
        background: Rectangle { color: "#323232"; radius: 6 }
        contentItem: RowLayout {
            Label {
                Layout.fillWidth: true
                color: "white"
                elide: Text.ElideRight
                text: page.lastMove ? qsTr("%1 → %2").arg(page.lastMove.label).arg(page.boxName(page.lastMove.to)) : ""
            }
            Button {
                flat: true
                text: qsTr("Undo")
                Material.foreground: Material.accent
                onClicked: {
                    if (page.lastMove)
                        for (const item of page.lastMove.items)
                            CardStore.moveCard(item.id, item.from)
                    page.lastMove = null
                    undoBar.close()
                }
            }
        }
    }
}
