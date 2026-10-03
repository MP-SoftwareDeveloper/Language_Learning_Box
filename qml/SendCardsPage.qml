import QtCore
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Controls.Material
import QtQuick.Dialogs
import LearningBox

// Send or back up cards: save the selected learning box (or one box) as a file, or share it
// straight away (WhatsApp, Telegram, e-mail, Drive ...). Three plain questions with hints.
Page {
    id: page
    title: qsTr("Send or back up cards")

    readonly property bool forLearningBox: lboxFormat.checked
    // 0 = all cards of all learning boxes, a learning box id, or -1 = favorite cards (all boxes)
    readonly property int scope: scopeBox.currentIndex === 0 ? 0
                                 : scopeBox.currentIndex <= CardStore.collections.length
                                   ? CardStore.collections[scopeBox.currentIndex - 1].id : -1
    function allBoxesTotal() {
        let n = 0
        for (const col of CardStore.collections) n += col.total
        return n
    }
    readonly property int cardCount: scopeBox.currentIndex === 0 ? allBoxesTotal()
                                     : scope > 0 ? CardStore.collections[scopeBox.currentIndex - 1].total
                                     : CardStore.favoriteCount
    readonly property string format: forLearningBox ? "lbox" : "csv"
    readonly property bool withProgress: forLearningBox && keepProgress.checked
    property string message: ""
    property bool messageOk: true
    function say(text, ok) { message = text; messageOk = ok }

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth

        ColumnLayout {
            width: parent.width
            spacing: 4

            HowToBar { exporting: true }

            Pane {
                Layout.fillWidth: true
                Layout.margins: 12
                visible: firstVisitTip.show
                Material.elevation: 1
                ColumnLayout {
                    anchors.fill: parent
                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: qsTr("Tip: to give your cards to a friend, tap “Share” and choose WhatsApp, Telegram or e-mail. Your friend opens the file in LearningBox → Get cards.")
                    }
                    Button {
                        Layout.alignment: Qt.AlignRight
                        flat: true
                        text: qsTr("Got it")
                        onClicked: firstVisitTip.show = false
                    }
                }
            }
            Settings {
                id: firstVisitTip
                category: "tips"
                property bool show: true
            }

            // ---- 1. Who is it for? ----
            Label {
                Layout.leftMargin: 16
                Layout.topMargin: 8
                text: qsTr("1. Who is it for?")
                font.pixelSize: 16
                font.bold: true
            }
            ButtonGroup { id: formatGroup }
            RadioButton {
                id: lboxFormat
                Layout.leftMargin: 8
                ButtonGroup.group: formatGroup
                checked: true
                text: qsTr("Another LearningBox app")
            }
            HintLabel {
                Layout.leftMargin: 48
                Layout.rightMargin: 16
                text: qsTr("For a friend, a new phone or a backup. A .lbox file that keeps pictures; it opens in LearningBox → Get cards.")
            }
            RadioButton {
                id: csvFormat
                Layout.leftMargin: 8
                ButtonGroup.group: formatGroup
                text: qsTr("Excel, Anki or Quizlet")
            }
            HintLabel {
                Layout.leftMargin: 48
                Layout.rightMargin: 16
                text: qsTr("A .csv table: German | meaning | example. No pictures.")
            }

            // ---- 2. Which cards? ----
            Label {
                Layout.leftMargin: 16
                Layout.topMargin: 12
                text: qsTr("2. Which cards?")
                font.pixelSize: 16
                font.bold: true
            }
            ComboBox {
                id: scopeBox
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.fillWidth: true
                model: [qsTr("All cards (all learning boxes)")]
                       .concat(CardStore.collections.map(col => col.name))
                       .concat([qsTr("Favorite cards (all learning boxes)")])
            }
            HintLabel {
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                text: qsTr("%n card(s)", "", page.cardCount)
            }

            // ---- 3. Keep my progress? ----
            Label {
                Layout.leftMargin: 16
                Layout.topMargin: 12
                visible: page.forLearningBox
                text: qsTr("3. Keep my progress?")
                font.pixelSize: 16
                font.bold: true
            }
            Switch {
                id: keepProgress
                Layout.leftMargin: 8
                visible: page.forLearningBox
                text: checked ? qsTr("Yes, keep the boxes") : qsTr("No, start in %1").arg(BoxNames.name(1))
            }
            HintLabel {
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                visible: page.forLearningBox
                text: qsTr("On: the cards stay in their boxes — right for a backup or a new phone. Off: your friend starts every card in %1.").arg(BoxNames.name(1))
            }

            // ---- Actions ----
            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.topMargin: 16
                spacing: 12
                Button {
                    Layout.fillWidth: true
                    highlighted: true
                    enabled: page.cardCount > 0
                    text: qsTr("Share…")
                    onClicked: {
                        const r = DeckExchange.shareCards(page.format, page.withProgress, page.scope)
                        if (!r.ok)
                            page.say(r.error, false)
                        else
                            page.say(qsTr("Ready to send: %1 (%n card(s))", "", r.count).arg(r.file), true)
                    }
                }
                Button {
                    Layout.fillWidth: true
                    enabled: page.cardCount > 0
                    text: qsTr("Save file…")
                    onClicked: saveDialog.open()
                }
            }
            HintLabel {
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                text: qsTr("Share: send it now with WhatsApp, Telegram, e-mail or Drive. Save file: keep it in a folder on this phone.")
            }

            Label {
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.topMargin: 8
                Layout.fillWidth: true
                visible: page.message !== ""
                wrapMode: Text.WordWrap
                color: page.messageOk ? Material.color(Material.Green) : Material.color(Material.Red)
                text: page.message
            }
            Item { Layout.preferredHeight: 16 }
        }
    }

    FileDialog {
        id: saveDialog
        title: qsTr("Save cards as")
        fileMode: FileDialog.SaveFile
        defaultSuffix: page.format
        nameFilters: page.forLearningBox ? [qsTr("LearningBox (*.lbox)")] : [qsTr("CSV (*.csv)")]
        currentFolder: StandardPaths.writableLocation(StandardPaths.DocumentsLocation)
        selectedFile: currentFolder + "/" + DeckExchange.suggestedFileName(page.format, page.scope)
        onAccepted: {
            const r = DeckExchange.exportCards(selectedFile, page.format, page.withProgress, page.scope)
            if (r.ok) {
                const name = decodeURIComponent(selectedFile.toString().split("/").pop())
                page.say(qsTr("Saved: %1 (%n card(s))", "", r.count).arg(name), true)
            } else {
                page.say(r.error, false)
            }
        }
    }
}
