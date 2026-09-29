import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Controls.Material
import LearningBox

// "Start over": cards back into Box 1 — the whole selected learning box (box = 0) or one box
// (2..5, 6 = Learned). Options: learned cards, spread over days, clear statistics. Undo for 10 s.
// Put it inside a page with anchors.fill: parent and call open().
Item {
    id: root
    anchors.fill: parent
    property int box: 0

    function open() { dialog.open() }
    function boxName(b) { return b > 5 ? qsTr("Learned") : qsTr("Box %1").arg(b) }

    // CardStore.totalCount / boxCounts make it update when cards change.
    readonly property int count: dialog.visible
        ? (CardStore.totalCount, CardStore.learnedCount, CardStore.resetCount(box > 0 || includeLearned.checked, box))
        : 0
    readonly property int days: spread.checked ? daysBox.value : 1

    Dialog {
        id: dialog
        width: 320
        x: (root.width - width) / 2
        y: Math.max(8, (root.height - height) / 3)
        modal: true
        title: root.box > 0 ? qsTr("Move all cards of %1 to Box 1?").arg(root.boxName(root.box))
                            : qsTr("Start “%1” over?").arg(CardStore.currentCollectionName)
        footer: DialogButtonBox {
            Button {
                flat: true
                text: qsTr("Cancel")
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            }
            Button {
                flat: true
                enabled: root.count > 0
                text: qsTr("Start over")
                Material.foreground: Material.color(Material.Red)
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            }
        }
        onAboutToShow: {
            includeLearned.checked = true
            allToday.checked = true
            daysBox.value = 10
            clearStats.checked = false
        }

        ColumnLayout {
            width: dialog.availableWidth
            spacing: 4

            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: root.count === 0
                      ? qsTr("There are no cards to move.")
                      : qsTr("%n card(s) go back to Box 1. The cards themselves (words, meanings, pictures) are kept.", "", root.count)
            }
            CheckBox {
                id: includeLearned
                visible: root.box === 0
                text: qsTr("Include learned cards")
            }
            HintLabel {
                visible: includeLearned.visible
                leftPadding: 8
                text: includeLearned.checked
                      ? qsTr("Learned cards are practised again too.")
                      : qsTr("Off: the %n learned card(s) stay learned.", "", CardStore.learnedCount)
            }

            Label {
                Layout.topMargin: 8
                text: qsTr("When should they be asked?")
                font.bold: true
            }
            ButtonGroup { id: whenGroup }
            RadioButton {
                id: allToday
                ButtonGroup.group: whenGroup
                text: qsTr("All today")
            }
            HintLabel {
                leftPadding: 8
                visible: allToday.checked
                text: qsTr("%n card(s) due today.", "", root.count)
            }
            RowLayout {
                RadioButton {
                    id: spread
                    ButtonGroup.group: whenGroup
                    text: qsTr("Spread over")
                }
                SpinBox {
                    id: daysBox
                    from: 2
                    to: 60
                    value: 10
                    editable: true
                    Layout.preferredWidth: 120
                    onValueModified: spread.checked = true
                }
                Label { text: qsTr("days") }
            }
            HintLabel {
                leftPadding: 8
                visible: spread.checked
                text: qsTr("About %1 cards a day — easier after a long break.").arg(Math.ceil(root.count / Math.max(1, daysBox.value)))
            }

            CheckBox {
                id: clearStats
                Layout.topMargin: 4
                text: qsTr("Also clear the statistics")
            }
            HintLabel {
                leftPadding: 8
                text: qsTr("Reviews and mistakes per card are set to 0.")
            }
        }

        onAccepted: {
            const n = root.box > 0 ? CardStore.resetBox(root.box, root.days, clearStats.checked)
                                   : CardStore.resetCollection(includeLearned.checked, root.days, clearStats.checked)
            if (n > 0) {
                undoBar.text = qsTr("%n card(s) are back in Box 1", "", n)
                undoBar.open()
                undoTimer.restart()
            }
        }
    }

    Popup {
        id: undoBar
        property string text: ""
        x: 12
        y: root.height - height - 12
        width: root.width - 24
        padding: 8
        closePolicy: Popup.NoAutoClose
        Timer { id: undoTimer; interval: 10000; onTriggered: undoBar.close() }
        background: Rectangle { color: "#323232"; radius: 6 }
        contentItem: RowLayout {
            Label {
                Layout.fillWidth: true
                color: "white"
                elide: Text.ElideRight
                text: undoBar.text
            }
            Button {
                flat: true
                text: qsTr("Undo")
                Material.foreground: Material.accent
                onClicked: {
                    CardStore.undoReset()
                    undoBar.close()
                }
            }
        }
    }
}
