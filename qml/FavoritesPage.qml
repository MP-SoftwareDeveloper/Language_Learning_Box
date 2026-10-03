import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Controls.Material
import LearningBox

// Favorite words: every starred card of all learning boxes, newest star first.
// Tap a word to edit it; the star removes it from the list (the card itself stays).
Page {
    id: page
    title: qsTr("Favorite words")

    property var cards: []
    function reload() {
        // Keep the scroll position when a star is removed (a new list would jump to the top)
        const y = list.contentY
        cards = sortButton.apply(CardStore.favorites())
        list.forceLayout()
        list.contentY = Math.max(list.originY, Math.min(y, list.originY + list.contentHeight - list.height))
    }
    Component.onCompleted: reload()
    // Rebuilt when cards change or the page is shown again, not for a star: a star tapped here
    // keeps its card in the list (empty star, tap again to undo) until you come back to the page.
    Connections {
        target: CardStore
        function onChanged() { page.reload() }
    }
    StackView.onActivating: reload()
    StackView.onDeactivating: Speaker.stop()

    function boxName(b) { return BoxNames.name(b) }

    Component { id: editPage; CardEditPage {} }

    // Display order only (as added / A-Z / Z-A)
    Item {
        id: sortBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: page.cards.length > 0 ? sortButton.implicitHeight : 0
        clip: true
        SortButton {
            id: sortButton
            anchors.left: parent.left
            anchors.leftMargin: 8
            onOrderChanged: page.reload()
        }
    }

    ListView {
        id: list
        anchors.top: sortBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        clip: true
        model: page.cards
        ScrollBar.vertical: ScrollBar {}

        header: Item {
            width: list.width
            height: list.count > 0 ? hint.implicitHeight : 0
            HintLabel {
                id: hint
                width: parent.width
                leftPadding: 16
                rightPadding: 16
                topPadding: 8
                bottomPadding: 4
                visible: list.count > 0
                text: qsTr("Tap ★ on any card — in review, in a box, in the editor or in Lens — to add it here.")
            }
        }

        delegate: ItemDelegate {
            id: row
            required property var modelData
            width: ListView.view.width
            leftPadding: 12
            rightPadding: 4
            topPadding: 8
            bottomPadding: 8
            // The whole card: front, meaning and example, each with 🔊
            contentItem: CardFace {
                card: row.modelData
                note: (row.modelData.collection ? row.modelData.collection + " \u00B7 " : "") + page.boxName(row.modelData.box)
                StarButton {
                    implicitWidth: 36
                    cardId: row.modelData.id
                }
            }
            Rectangle {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: 12
                height: 1
                color: Material.foreground
                opacity: 0.12
            }
            onClicked: page.StackView.view.push(editPage, { cardId: row.modelData.id })
        }

        ColumnLayout {
            anchors.centerIn: parent
            width: Math.min(parent.width - 48, 320)
            visible: list.count === 0
            spacing: 8
            StarIcon {
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: 48
                Layout.preferredHeight: 48
                color: Material.foreground
                opacity: 0.4
            }
            Label {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                opacity: 0.7
                text: qsTr("No favorite words yet.\nTap ★ on any card — in review, in a box, in the editor or in Lens — to add it here.")
            }
        }
    }
}
