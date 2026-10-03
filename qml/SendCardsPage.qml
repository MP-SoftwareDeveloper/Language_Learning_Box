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

    // Language of the explanations on this page: the switch at the top (shared with the help pages).
    readonly property string lang: pageLang.lang
    readonly property bool rtl: lang === "fa"
    function tl(en, fa, de) { return lang === "fa" ? fa : (lang === "de" ? de : en) }
    title: page.tl("Send or back up cards", "ارسال یا پشتیبان‌گیری از کارت‌ها", "Karten senden oder sichern")

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
            LayoutMirroring.enabled: page.rtl
            LayoutMirroring.childrenInherit: true

            HowToBar { exporting: true }
            HelpLanguageBar { id: pageLang; Layout.alignment: Qt.AlignHCenter }

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
                        text: page.tl("Tip: to give your cards to a friend, tap “Share” and choose WhatsApp, Telegram or e-mail. Your friend opens the file in LearningBox → Get cards.", "نکته: برای دادن کارت‌ها به دوستتان روی «اشتراک‌گذاری» بزنید و واتس‌اپ، تلگرام یا ایمیل را انتخاب کنید. دوستتان فایل را در LearningBox ← دریافت کارت‌ها باز می‌کند.", "Tipp: Um deine Karten einem Freund zu geben, tippe auf „Teilen“ und wähle WhatsApp, Telegram oder E-Mail. Dein Freund öffnet die Datei in LearningBox → Karten holen.")
                    }
                    Button {
                        Layout.alignment: Qt.AlignRight
                        flat: true
                        text: page.tl("Got it", "متوجه شدم", "Verstanden")
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
                text: page.tl("1. Who is it for?", "۱. برای چه کسی است؟", "1. Für wen ist es?")
                font.pixelSize: 16
                font.bold: true
            }
            ButtonGroup { id: formatGroup }
            RadioButton {
                id: lboxFormat
                Layout.leftMargin: 8
                ButtonGroup.group: formatGroup
                checked: true
                text: page.tl("Another LearningBox app", "برنامهٔ LearningBox دیگر", "Eine andere LearningBox-App")
            }
            HintLabel {
                Layout.leftMargin: 48
                Layout.rightMargin: 16
                text: page.tl("For a friend, a new phone or a backup. A .lbox file that keeps pictures; it opens in LearningBox → Get cards.", "برای دوست، گوشی جدید یا پشتیبان. یک فایل ‎lbox.‎ که تصاویر را نگه می‌دارد؛ در LearningBox ← دریافت کارت‌ها باز می‌شود.", "Für einen Freund, ein neues Handy oder als Sicherung. Eine .lbox-Datei, die Bilder behält; sie öffnet sich in LearningBox → Karten holen.")
            }
            RadioButton {
                id: csvFormat
                Layout.leftMargin: 8
                ButtonGroup.group: formatGroup
                text: page.tl("Excel, Anki or Quizlet", "اکسل، Anki یا Quizlet", "Excel, Anki oder Quizlet")
            }
            HintLabel {
                Layout.leftMargin: 48
                Layout.rightMargin: 16
                text: page.tl("A .csv table: German | meaning | example. No pictures.", "یک جدول ‎csv.‎: آلمانی | معنی | مثال. بدون تصویر.", "Eine .csv-Tabelle: Deutsch | Bedeutung | Beispiel. Keine Bilder.")
            }

            // ---- 2. Which cards? ----
            Label {
                Layout.leftMargin: 16
                Layout.topMargin: 12
                text: page.tl("2. Which cards?", "۲. کدام کارت‌ها؟", "2. Welche Karten?")
                font.pixelSize: 16
                font.bold: true
            }
            ComboBox {
                id: scopeBox
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.fillWidth: true
                model: [page.tl("All cards (all learning boxes)", "همهٔ کارت‌ها (همهٔ جعبه‌های یادگیری)", "Alle Karten (alle Lernboxen)")]
                       .concat(CardStore.collections.map(col => col.name))
                       .concat([page.tl("Favorite cards (all learning boxes)", "کارت‌های مورد علاقه (همهٔ جعبه‌های یادگیری)", "Favoritenkarten (alle Lernboxen)")])
            }
            HintLabel {
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                text: page.tl("%1 card(s)", "%1 کارت", "%1 Karte(n)").arg(page.cardCount)
            }

            // ---- 3. Keep my progress? ----
            Label {
                Layout.leftMargin: 16
                Layout.topMargin: 12
                visible: page.forLearningBox
                text: page.tl("3. Keep my progress?", "۳. پیشرفت من حفظ شود؟", "3. Fortschritt behalten?")
                font.pixelSize: 16
                font.bold: true
            }
            Switch {
                id: keepProgress
                Layout.leftMargin: 8
                visible: page.forLearningBox
                text: checked ? page.tl("Yes, keep the boxes", "بله، جعبه‌ها حفظ شوند", "Ja, Boxen behalten") : page.tl("No, start in %1", "نه، از %1 شروع شود", "Nein, in %1 beginnen").arg(BoxNames.name(1))
            }
            HintLabel {
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                visible: page.forLearningBox
                text: page.tl("On: the cards stay in their boxes — right for a backup or a new phone. Off: your friend starts every card in %1.", "روشن: کارت‌ها در جعبه‌های خود می‌مانند — مناسب پشتیبان یا گوشی جدید. خاموش: دوستتان هر کارت را از %1 شروع می‌کند.", "An: Die Karten bleiben in ihren Boxen — richtig für eine Sicherung oder ein neues Handy. Aus: Dein Freund beginnt jede Karte in %1.").arg(BoxNames.name(1))
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
                    text: page.tl("Share…", "اشتراک‌گذاری…", "Teilen…")
                    onClicked: {
                        const r = DeckExchange.shareCards(page.format, page.withProgress, page.scope)
                        if (!r.ok)
                            page.say(r.error, false)
                        else
                            page.say(page.tl("Ready to send: %1 (%2 card(s))", "آمادهٔ ارسال: %1 (%2 کارت)", "Bereit zum Senden: %1 (%2 Karte(n))").arg(r.file).arg(r.count), true)
                    }
                }
                Button {
                    Layout.fillWidth: true
                    enabled: page.cardCount > 0
                    text: page.tl("Save file…", "ذخیرهٔ فایل…", "Datei speichern…")
                    onClicked: saveDialog.open()
                }
            }
            HintLabel {
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                text: page.tl("Share: send it now with WhatsApp, Telegram, e-mail or Drive. Save file: keep it in a folder on this phone.", "اشتراک‌گذاری: همین حالا با واتس‌اپ، تلگرام، ایمیل یا Drive بفرستید. ذخیرهٔ فایل: آن را در پوشه‌ای روی این گوشی نگه دارید.", "Teilen: jetzt per WhatsApp, Telegram, E-Mail oder Drive senden. Datei speichern: in einem Ordner auf diesem Handy aufbewahren.")
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
        title: page.tl("Save cards as", "ذخیرهٔ کارت‌ها به‌عنوان", "Karten speichern als")
        fileMode: FileDialog.SaveFile
        defaultSuffix: page.format
        nameFilters: page.forLearningBox ? [qsTr("LearningBox (*.lbox)")] : [qsTr("CSV (*.csv)")]
        currentFolder: StandardPaths.writableLocation(StandardPaths.DocumentsLocation)
        selectedFile: currentFolder + "/" + DeckExchange.suggestedFileName(page.format, page.scope)
        onAccepted: {
            const r = DeckExchange.exportCards(selectedFile, page.format, page.withProgress, page.scope)
            if (r.ok) {
                const name = decodeURIComponent(selectedFile.toString().split("/").pop())
                page.say(page.tl("Saved: %1 (%2 card(s))", "ذخیره شد: %1 (%2 کارت)", "Gespeichert: %1 (%2 Karte(n))").arg(name).arg(r.count), true)
            } else {
                page.say(r.error, false)
            }
        }
    }
}
