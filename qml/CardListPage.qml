import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Controls.Material
import LearningBox

Page {
    id: page
    title: qsTr("All cards (%1)").arg(cards.count)

    CardListModel {
        id: cards
        filter: search.text
    }

    Component { id: editPage; CardEditPage {} }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        TextField {
            id: search
            Layout.fillWidth: true
            Layout.margins: 12
            placeholderText: qsTr("Search (German, Persian, …)")
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: cards
            ScrollBar.vertical: ScrollBar {}

            delegate: ItemDelegate {
                id: row
                required property int cardId
                required property string front
                required property string back
                required property int box
                required property url imageUrl
                width: ListView.view.width

                contentItem: RowLayout {
                    spacing: 12
                    Image {
                        Layout.preferredWidth: 44
                        Layout.preferredHeight: 44
                        visible: row.imageUrl.toString() !== ""
                        source: row.imageUrl
                        sourceSize: Qt.size(88, 88) // decode small for the list
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
                        clip: true
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Label {
                            Layout.fillWidth: true
                            text: row.front
                            font.pixelSize: 17
                            font.bold: true
                            elide: Text.ElideRight
                        }
                        Label {
                            Layout.fillWidth: true
                            text: row.back.split("\n")[0] // first line = meaning
                            visible: row.back.length > 0
                            opacity: 0.7
                            elide: Text.ElideRight
                            maximumLineCount: 1
                        }
                    }
                    Label {
                        text: row.box > 5 ? "\u2B50" : row.box
                        font.bold: true
                        opacity: 0.6
                    }
                    SpeakButton { speakText: row.front }
                }
                onClicked: page.StackView.view.push(editPage, { cardId: row.cardId })
                onPressAndHold: {
                    page.menuCard = { id: row.cardId, front: row.front }
                    cardMenu.popup()
                }
            }

            Label {
                anchors.centerIn: parent
                visible: list.count === 0
                text: search.text.length > 0 ? qsTr("No matches") : qsTr("No cards yet")
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
}
