import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Controls.Material
import LearningBox

// One Leitner box: every card in it, with buttons to move a card to the previous or next box.
// Tabs switch between boxes 1-5 and Learned (6).
Page {
    id: page
    property int box: 1
    title: box > 5 ? qsTr("Learned") : qsTr("Box %1").arg(box)

    property var cards: []
    property var lastMove: null // {id, front, from, to} for Undo

    function reload() { cards = CardStore.cardsInBox(box) }
    onBoxChanged: reload()
    Component.onCompleted: reload()
    Connections {
        target: CardStore
        function onChanged() { page.reload() }
    }

    function boxName(b) { return b > 5 ? qsTr("Learned") : qsTr("Box %1").arg(b) }
    function move(card, to) {
        const from = card.box, id = card.id, front = card.front // before the list reloads
        if (CardStore.moveCard(id, to)) {
            lastMove = { id: id, front: front, from: from, to: to }
            undoBar.open()
            undoTimer.restart() // also when a second card is moved while the bar is showing
        }
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
                    text: (index < 5 ? (index + 1) : "★") + " (" + count + ")"
                    font.pixelSize: 13
                    leftPadding: 2
                    rightPadding: 2
                }
            }
        }

        Label {
            Layout.fillWidth: true
            Layout.margins: 12
            Layout.bottomMargin: 4
            wrapMode: Text.WordWrap
            opacity: 0.7
            text: page.box > 5
                  ? qsTr("Learned cards are no longer reviewed. Move one back to box 5 to practise it again.")
                  : qsTr("Reviewed every %n day(s). \u2039 moves a card back, \u203A moves it forward; it is then due after that box's interval. Press and hold a card to edit or delete it.",
                         "", Math.pow(2, page.box - 1))
        }

        Button {
            Layout.leftMargin: 8
            visible: page.box > 1 && page.cards.length > 0
            flat: true
            text: qsTr("Move all %n card(s) to Box 1", "", page.cards.length)
            onClicked: resetDialog.open()
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

                contentItem: RowLayout {
                    spacing: 6
                    Image {
                        Layout.preferredWidth: 40
                        Layout.preferredHeight: 40
                        visible: row.modelData.imageUrl.toString() !== ""
                        source: row.modelData.imageUrl
                        sourceSize: Qt.size(80, 80)
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
                        clip: true
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 1
                        Label {
                            Layout.fillWidth: true
                            text: row.modelData.front
                            font.pixelSize: 16
                            font.bold: true
                            elide: Text.ElideRight
                        }
                        Label {
                            Layout.fillWidth: true
                            visible: text !== ""
                            text: row.modelData.back.split("\n")[0]
                            opacity: 0.75
                            elide: Text.ElideRight
                        }
                        Label {
                            visible: text !== ""
                            text: page.dueText(row.modelData)
                            font.pixelSize: 12
                            opacity: 0.55
                        }
                    }
                    SpeakButton { speakText: row.modelData.front }
                    ToolButton {
                        enabled: row.modelData.box > 1
                        opacity: enabled ? 1 : 0.25
                        text: "\u2039" // ‹ (◀ is drawn as an emoji on Android)
                        font.pixelSize: 30
                        font.bold: true
                        ToolTip.visible: hovered || pressed
                        ToolTip.text: qsTr("Move to %1").arg(page.boxName(row.modelData.box - 1))
                        onClicked: page.move(row.modelData, row.modelData.box - 1)
                    }
                    ToolButton {
                        enabled: row.modelData.box < 6
                        opacity: enabled ? 1 : 0.25
                        text: "\u203A" // ›
                        font.pixelSize: 30
                        font.bold: true
                        ToolTip.visible: hovered || pressed
                        ToolTip.text: qsTr("Move to %1").arg(page.boxName(row.modelData.box + 1))
                        onClicked: page.move(row.modelData, row.modelData.box + 1)
                    }
                }
                onClicked: page.StackView.view.push(editPage, { cardId: row.modelData.id })
                onPressAndHold: {
                    page.menuCard = { id: row.modelData.id, front: row.modelData.front }
                    cardMenu.popup()
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

    // Press and hold a card: Edit / Delete (with confirmation).
    property var menuCard: null // {id, front}
    Menu {
        id: cardMenu
        MenuItem {
            text: qsTr("Edit")
            onTriggered: page.StackView.view.push(editPage, { cardId: page.menuCard.id })
        }
        MenuItem {
            text: qsTr("Delete")
            Material.foreground: Material.color(Material.Red)
            onTriggered: confirmDelete.open()
        }
    }
    Dialog {
        id: confirmDelete
        // Fixed width + x/y: a size bound to the page (anchors/width) made a layout loop with the
        // wrapping label, because the page's own size also looks at this dialog. 320 fits any phone.
        x: (page.width - width) / 2
        y: (page.height - height) / 2
        width: 320
        modal: true
        title: qsTr("Delete this card?")
        standardButtons: Dialog.Yes | Dialog.No
        Label {
            width: confirmDelete.availableWidth
            wrapMode: Text.WordWrap
            text: (page.menuCard ? page.menuCard.front : "") + "\n\n"
                  + qsTr("The card, its progress and its picture are removed. This cannot be undone.")
        }
        onAccepted: if (page.menuCard) CardStore.removeCard(page.menuCard.id)
    }

    ResetDialog { id: resetDialog; box: page.box }

    // Add a card straight into this box
    RoundButton {
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
                text: page.lastMove ? qsTr("%1 → %2").arg(page.lastMove.front).arg(page.boxName(page.lastMove.to)) : ""
            }
            Button {
                flat: true
                text: qsTr("Undo")
                Material.foreground: Material.accent
                onClicked: {
                    if (page.lastMove)
                        CardStore.moveCard(page.lastMove.id, page.lastMove.from)
                    page.lastMove = null
                    undoBar.close()
                }
            }
        }
    }
}
