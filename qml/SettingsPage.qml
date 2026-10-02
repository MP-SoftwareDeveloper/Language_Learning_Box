import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Controls.Material
import LearningBox

Page {
    id: page
    objectName: "settingsPage" // Main hides the gear here
    title: qsTr("Settings")

    // Language of the explanations on this page (shared with the help pages).
    HelpLanguageBar { id: langBar; visible: false }
    readonly property string lang: langBar.lang
    readonly property bool rtl: lang === "fa"
    readonly property int hAlign: rtl ? Text.AlignRight : Text.AlignLeft
    readonly property var tx: ({
        "en": {
            "lang": "Language of the explanations",
            "app": "App",
            "simple": "Simple",
            "simpleHint": "Leitner flashcards only. Offline, no permissions.",
            "full": "Full",
            "fullHint": "Adds Lens, meanings and example sentences filled in, word packs, pictures and Anki decks.",
            "switchHint": "Switching only shows or hides features. Your cards and progress are kept.",
            "compare": "Compare",
            "translation": "Translation",
            "meaningsIn": "Meanings in “%1” (learning %2) are shown in:",
            "usedFull": "Used for the back of new cards, the review and Lens translations; English and German meanings can be listened to (🔊). ",
            "usedSimple": "Used for the back of new cards and the review; English and German meanings can be listened to (🔊). ",
            "eachBox": "Each learning box keeps its own choice. The help follows it (Persian, else English).",
            "online": "Translate online when connected",
            "simpleOffline": "The Simple app works offline: it uses the meanings on your cards and translations saved earlier.",
            "nowOff": "Now: offline (online is switched off).",
            "nowOn": "Now: online (Google Translate).",
            "nowMaybe": "Now: online when a connection is available (Google Translate).",
            "offlineNote": "Offline, the app uses translations it saved earlier and the word pack's Persian meanings.",
            "saved": "Saved translations: %1",
            "clear": "Clear",
            "lens": "Text recognition (Lens)",
            "onlineOcr": "Online recognition (handwriting)",
            "ocrHint": "Photos are sent to Azure AI Vision for reading. With the free tier (F0) this never costs anything: 5000 photos a month, after that Azure refuses and the offline reader is used. Also used offline or when the service fails.",
            "howKey": "How to get a free key",
            "test": "Test",
            "speech": "Speech",
            "speed": "Speed"
        },
        "fa": {
            "lang": "زبان توضیحات",
            "app": "برنامه",
            "simple": "ساده",
            "simpleHint": "فقط کارت‌های لایتنر. آفلاین و بدون نیاز به مجوز.",
            "full": "کامل",
            "fullHint": "لنز، معنی‌ها و جمله‌های مثال خودکار، بسته‌های واژگان، تصویر و دسته‌های Anki را اضافه می‌کند.",
            "switchHint": "تغییر حالت فقط امکانات را نشان می‌دهد یا پنهان می‌کند. کارت‌ها و پیشرفت شما حفظ می‌شود.",
            "compare": "مقایسه",
            "translation": "ترجمه",
            "meaningsIn": "معنی‌ها در «%1» (یادگیری %2) نمایش داده می‌شوند به:",
            "usedFull": "برای پشت کارت‌های جدید، مرور و ترجمه‌های لنز استفاده می‌شود؛ معنی‌های انگلیسی و آلمانی را می‌توان شنید (🔊). ",
            "usedSimple": "برای پشت کارت‌های جدید و مرور استفاده می‌شود؛ معنی‌های انگلیسی و آلمانی را می‌توان شنید (🔊). ",
            "eachBox": "هر جعبهٔ یادگیری انتخاب خودش را نگه می‌دارد. راهنما از آن پیروی می‌کند (فارسی، وگرنه انگلیسی).",
            "online": "ترجمهٔ آنلاین در صورت اتصال",
            "simpleOffline": "برنامهٔ ساده آفلاین کار می‌کند: از معنی‌های روی کارت‌ها و ترجمه‌های ذخیره‌شدهٔ قبلی استفاده می‌کند.",
            "nowOff": "اکنون: آفلاین (ترجمهٔ آنلاین خاموش است).",
            "nowOn": "اکنون: آنلاین (Google Translate).",
            "nowMaybe": "اکنون: آنلاین در صورت وجود اتصال (Google Translate).",
            "offlineNote": "در حالت آفلاین، برنامه از ترجمه‌های ذخیره‌شدهٔ قبلی و معنی‌های فارسی بستهٔ واژگان استفاده می‌کند.",
            "saved": "ترجمه‌های ذخیره‌شده: %1",
            "clear": "پاک کردن",
            "lens": "تشخیص متن (لنز)",
            "onlineOcr": "تشخیص آنلاین (دست‌نویس)",
            "ocrHint": "عکس‌ها برای خواندن به Azure AI Vision فرستاده می‌شوند. با طرح رایگان (F0) هیچ هزینه‌ای ندارد: ۵۰۰۰ عکس در ماه؛ پس از آن Azure نمی‌پذیرد و خواندن آفلاین استفاده می‌شود. در حالت آفلاین یا هنگام خطای سرویس هم همین‌طور.",
            "howKey": "راهنمای گرفتن کلید رایگان",
            "test": "آزمایش",
            "speech": "گفتار",
            "speed": "سرعت"
        },
        "de": {
            "lang": "Sprache der Erklärungen",
            "app": "App",
            "simple": "Einfach",
            "simpleHint": "Nur Leitner-Karteikarten. Offline, keine Berechtigungen.",
            "full": "Voll",
            "fullHint": "Fügt Lens, ausgefüllte Bedeutungen und Beispielsätze, Wortpakete, Bilder und Anki-Decks hinzu.",
            "switchHint": "Das Umschalten zeigt oder verbirgt nur Funktionen. Deine Karten und dein Fortschritt bleiben erhalten.",
            "compare": "Vergleichen",
            "translation": "Übersetzung",
            "meaningsIn": "Bedeutungen in „%1“ (Lernsprache: %2) werden angezeigt auf:",
            "usedFull": "Wird für die Rückseite neuer Karten, die Wiederholung und Lens-Übersetzungen verwendet; englische und deutsche Bedeutungen kann man anhören (🔊). ",
            "usedSimple": "Wird für die Rückseite neuer Karten und die Wiederholung verwendet; englische und deutsche Bedeutungen kann man anhören (🔊). ",
            "eachBox": "Jede Lernbox merkt sich ihre eigene Wahl. Die Hilfe folgt ihr (Persisch, sonst Englisch).",
            "online": "Online übersetzen, wenn verbunden",
            "simpleOffline": "Die einfache App arbeitet offline: Sie nutzt die Bedeutungen auf deinen Karten und früher gespeicherte Übersetzungen.",
            "nowOff": "Jetzt: offline (Online ist ausgeschaltet).",
            "nowOn": "Jetzt: online (Google Translate).",
            "nowMaybe": "Jetzt: online, sobald eine Verbindung besteht (Google Translate).",
            "offlineNote": "Offline nutzt die App früher gespeicherte Übersetzungen und die persischen Bedeutungen des Wortpakets.",
            "saved": "Gespeicherte Übersetzungen: %1",
            "clear": "Löschen",
            "lens": "Texterkennung (Lens)",
            "onlineOcr": "Online-Erkennung (Handschrift)",
            "ocrHint": "Fotos werden zum Lesen an Azure AI Vision gesendet. Mit dem kostenlosen Tarif (F0) entstehen nie Kosten: 5000 Fotos pro Monat, danach lehnt Azure ab und der Offline-Leser wird verwendet. Er wird auch offline oder bei Dienstfehlern verwendet.",
            "howKey": "So bekommst du einen kostenlosen Schlüssel",
            "test": "Testen",
            "speech": "Sprache",
            "speed": "Tempo"
        }
    })
    function tr2(key) { return (tx[lang] && tx[lang][key]) || tx.en[key] }

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth

        ColumnLayout {
            width: parent.width
            spacing: 8

            Item { Layout.preferredHeight: 4 }

            Label {
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.fillWidth: true
                horizontalAlignment: page.hAlign
                text: page.tr2("lang")
                font.bold: true
            }
            HelpLanguageBar {
                Layout.leftMargin: 8
                Layout.bottomMargin: 8
            }

            // ---------- app mode ----------
            Label {
                Layout.leftMargin: 16
                text: page.tr2("app")
                font.pixelSize: 16
                font.bold: true
            }
            // checkable: false - the dot always shows the stored mode (see the language choice below)
            RadioButton {
                Layout.leftMargin: 8
                checkable: false
                text: page.tr2("simple")
                checked: !AppMode.full
                onClicked: AppMode.mode = "simple"
            }
            HintLabel {
                Layout.leftMargin: 56
                Layout.rightMargin: 16
                Layout.topMargin: -12
                horizontalAlignment: page.hAlign
                text: page.tr2("simpleHint")
            }
            RadioButton {
                Layout.leftMargin: 8
                checkable: false
                text: page.tr2("full")
                checked: AppMode.full
                onClicked: AppMode.mode = "full"
            }
            HintLabel {
                Layout.leftMargin: 56
                Layout.rightMargin: 16
                Layout.topMargin: -12
                horizontalAlignment: page.hAlign
                text: page.tr2("fullHint")
            }
            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                HintLabel { horizontalAlignment: page.hAlign; text: page.tr2("switchHint") }
                Button {
                    flat: true
                    text: page.tr2("compare")
                    onClicked: compareDialog.open()
                }
            }

            // ---------- translation ----------
            Label {
                Layout.leftMargin: 16
                text: page.tr2("translation")
                font.pixelSize: 16
                font.bold: true
            }
            HintLabel {
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                horizontalAlignment: page.hAlign
                text: page.tr2("meaningsIn").arg(CardStore.currentCollectionName)
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
                horizontalAlignment: page.hAlign
                text: (AppMode.full ? page.tr2("usedFull") : page.tr2("usedSimple")) + page.tr2("eachBox")
            }

            SwitchDelegate {
                Layout.fillWidth: true
                visible: AppMode.full
                text: page.tr2("online")
                checked: Translator.onlineEnabled
                onToggled: Translator.onlineEnabled = checked
            }
            Label {
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                opacity: 0.7
                horizontalAlignment: page.hAlign
                text: {
                    if (!AppMode.full)
                        return page.tr2("simpleOffline")
                    const now = !Translator.onlineEnabled
                              ? page.tr2("nowOff")
                              : Translator.networkAvailable
                                ? page.tr2("nowOn")
                                : page.tr2("nowMaybe")
                    return now + " " + page.tr2("offlineNote")
                }
            }
            RowLayout {
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Label {
                    Layout.fillWidth: true
                    text: page.tr2("saved").arg(Translator.savedCount)
                }
                Button {
                    flat: true
                    enabled: Translator.savedCount > 0
                    text: page.tr2("clear")
                    onClicked: Translator.clearSaved()
                }
            }

            // ---------- text recognition (Lens) ----------
            Label {
                Layout.leftMargin: 16
                Layout.topMargin: 12
                visible: AppMode.full
                text: page.tr2("lens")
                font.pixelSize: 16
                font.bold: true
            }
            SwitchDelegate {
                Layout.fillWidth: true
                visible: AppMode.full
                text: page.tr2("onlineOcr")
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
                horizontalAlignment: page.hAlign
                text: page.tr2("ocrHint")
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
                    text: CloudOcr.testing ? qsTr("Testing\u2026") : page.tr2("test")
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
                    text: page.tr2("howKey")
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
                text: page.tr2("speech")
                font.pixelSize: 16
                font.bold: true
            }
            RowLayout {
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Label { text: page.tr2("speed") }
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
