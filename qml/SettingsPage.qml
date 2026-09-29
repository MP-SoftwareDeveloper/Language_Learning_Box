import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Controls.Material
import LearningBox

Page {
    id: page
    objectName: "settingsPage" // Main hides the gear here
    title: qsTr("Settings")

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth

        ColumnLayout {
            width: parent.width
            spacing: 8

            Item { Layout.preferredHeight: 4 }

            // ---------- app mode ----------
            Label {
                Layout.leftMargin: 16
                text: qsTr("App")
                font.pixelSize: 16
                font.bold: true
            }
            // checkable: false - the dot always shows the stored mode (see the language choice below)
            RadioButton {
                Layout.leftMargin: 8
                checkable: false
                text: qsTr("Simple")
                checked: !AppMode.full
                onClicked: AppMode.mode = "simple"
            }
            HintLabel {
                Layout.leftMargin: 56
                Layout.rightMargin: 16
                Layout.topMargin: -12
                text: qsTr("Leitner flashcards only. Offline, no permissions.")
            }
            RadioButton {
                Layout.leftMargin: 8
                checkable: false
                text: qsTr("Full")
                checked: AppMode.full
                onClicked: AppMode.mode = "full"
            }
            HintLabel {
                Layout.leftMargin: 56
                Layout.rightMargin: 16
                Layout.topMargin: -12
                text: qsTr("Adds Lens, meanings and example sentences filled in, word packs, pictures and Anki decks.")
            }
            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                HintLabel { text: qsTr("Switching only shows or hides features. Your cards and progress are kept.") }
                Button {
                    flat: true
                    text: qsTr("Compare")
                    onClicked: compareDialog.open()
                }
            }

            // ---------- translation ----------
            Label {
                Layout.leftMargin: 16
                text: qsTr("Translation")
                font.pixelSize: 16
                font.bold: true
            }
            HintLabel {
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                text: qsTr("Meanings in \u201C%1\u201D (learning %2) are shown in:").arg(CardStore.currentCollectionName)
                      .arg(CardStore.learningLanguage === "en" ? "English" : "Deutsch")
            }
            // Every language except the one being learned. Not checkable by itself: the dot always
            // shows the stored choice (a clicked, checkable radio keeps its own state and drifts).
            Repeater {
                model: [ { code: "fa", label: "فارسی  (Persian)" }, { code: "en", label: "English" }, { code: "de", label: "Deutsch  (German)" } ]
                    .filter(o => o.code !== CardStore.learningLanguage)
                delegate: RadioButton {
                    required property var modelData
                    Layout.leftMargin: 8
                    checkable: false
                    text: modelData.label
                    checked: Translator.meaningLanguage === modelData.code
                    onClicked: Translator.targetLanguage = modelData.code
                }
            }
            HintLabel {
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                text: (AppMode.full ? qsTr("Used for the back of new cards, the review and Lens translations; English and German meanings can be listened to (\uD83D\uDD0A). ")
                                    : qsTr("Used for the back of new cards and the review; English and German meanings can be listened to (\uD83D\uDD0A). "))
                      + qsTr("Each learning box keeps its own choice. The help follows it (Persian, else English).")
            }

            SwitchDelegate {
                Layout.fillWidth: true
                visible: AppMode.full
                text: qsTr("Translate online when connected")
                checked: Translator.onlineEnabled
                onToggled: Translator.onlineEnabled = checked
            }
            Label {
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                opacity: 0.7
                text: {
                    if (!AppMode.full)
                        return qsTr("The Simple app works offline: it uses the meanings on your cards and translations saved earlier.")
                    const now = !Translator.onlineEnabled
                              ? qsTr("Now: offline (online is switched off).")
                              : Translator.networkAvailable
                                ? qsTr("Now: online (Google Translate).")
                                : qsTr("Now: online when a connection is available (Google Translate).")
                    return now + " " + qsTr("Offline, the app uses translations it saved earlier and the word pack's Persian meanings.")
                }
            }
            RowLayout {
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Label {
                    Layout.fillWidth: true
                    text: qsTr("Saved translations: %1").arg(Translator.savedCount)
                }
                Button {
                    flat: true
                    enabled: Translator.savedCount > 0
                    text: qsTr("Clear")
                    onClicked: Translator.clearSaved()
                }
            }

            // ---------- text recognition (Lens) ----------
            Label {
                Layout.leftMargin: 16
                Layout.topMargin: 12
                visible: AppMode.full
                text: qsTr("Text recognition (Lens)")
                font.pixelSize: 16
                font.bold: true
            }
            SwitchDelegate {
                Layout.fillWidth: true
                visible: AppMode.full
                text: qsTr("Online recognition (handwriting)")
                checked: CloudOcr.enabled
                onToggled: CloudOcr.enabled = checked
            }
            Label {
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                opacity: 0.7
                visible: AppMode.full
                text: qsTr("Photos are sent to Azure AI Vision for reading. With the free tier (F0) this never costs "
                           + "anything: 5000 photos a month, after that Azure refuses and the offline reader is used. "
                           + "Also used offline or when the service fails.")
            }
            TextField {
                id: endpointField
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                visible: AppMode.full && CloudOcr.enabled
                placeholderText: qsTr("Endpoint, e.g. https://my-vision.cognitiveservices.azure.com/")
                text: CloudOcr.endpoint
                inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
                onEditingFinished: CloudOcr.endpoint = text
            }
            RowLayout {
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.fillWidth: true
                visible: AppMode.full && CloudOcr.enabled
                TextField {
                    id: keyField
                    Layout.fillWidth: true
                    placeholderText: qsTr("Key (KEY 1)")
                    text: CloudOcr.apiKey
                    echoMode: showKey.checked ? TextInput.Normal : TextInput.Password
                    inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase | Qt.ImhSensitiveData
                    onEditingFinished: CloudOcr.apiKey = text
                }
                ToolButton {
                    id: showKey
                    checkable: true
                    text: checked ? "\u25CE" : "\u25C9"
                    ToolTip.visible: pressed
                    ToolTip.text: checked ? qsTr("Hide key") : qsTr("Show key")
                }
            }
            RowLayout {
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.fillWidth: true
                visible: AppMode.full && CloudOcr.enabled
                Button {
                    text: CloudOcr.testing ? qsTr("Testing\u2026") : qsTr("Test")
                    enabled: !CloudOcr.testing && keyField.text.trim() !== "" && endpointField.text.trim() !== ""
                    onClicked: {
                        CloudOcr.endpoint = endpointField.text
                        CloudOcr.apiKey = keyField.text
                        keyResult.text = ""
                        CloudOcr.testKey()
                    }
                }
                Button {
                    flat: true
                    text: qsTr("How to get a free key")
                    onClicked: Qt.openUrlExternally("https://portal.azure.com/#create/Microsoft.CognitiveServicesComputerVision")
                }
            }
            Label {
                id: keyResult
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.fillWidth: true
                visible: AppMode.full && CloudOcr.enabled && text !== ""
                wrapMode: Text.WordWrap
                property bool ok: false
                color: ok ? Material.color(Material.Green) : Material.color(Material.Red)
                Connections {
                    target: CloudOcr
                    function onKeyTested(ok, message) {
                        keyResult.ok = ok
                        keyResult.text = (ok ? "\u2713 " : "\u2717 ") + message
                    }
                }
            }

            // ---------- speech ----------
            Label {
                Layout.leftMargin: 16
                Layout.topMargin: 12
                text: qsTr("Speech")
                font.pixelSize: 16
                font.bold: true
            }
            RowLayout {
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Label { text: qsTr("Speed") }
                Slider {
                    Layout.fillWidth: true
                    from: -1; to: 1; stepSize: 0.1
                    value: Speaker.rate
                    onMoved: Speaker.rate = value
                }
                SpeakButton { speakText: "Guten Morgen! Wie geht es dir?" }
            }
            Item { Layout.preferredHeight: 16 }
        }
    }

    Dialog {
        id: compareDialog
        title: qsTr("Simple and Full app")
        modal: true
        width: Math.min(360, page.width - 16)
        height: Math.min(implicitHeight, page.height - 16)
        x: (page.width - width) / 2
        y: Math.max(8, (page.height - height) / 2)
        standardButtons: Dialog.Close
        contentItem: Flickable {
            implicitHeight: compareGrid.implicitHeight
            contentHeight: compareGrid.implicitHeight
            clip: true
            ModeComparison { id: compareGrid; width: parent.width }
        }
    }
}
