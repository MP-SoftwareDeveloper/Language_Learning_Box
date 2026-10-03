import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Controls.Material
import QtQuick.Dialogs
import LearningBox

// Get cards from a file or a link: LearningBox (.lbox), CSV/text (Excel, Quizlet, Anki text)
// and, in Full mode, Anki decks (.apkg) and links. A preview in plain words, options with hints.
Page {
    id: page

    // Language of the explanations on this page: the switch at the top (shared with the help pages).
    readonly property string lang: pageLang.lang
    readonly property bool rtl: lang === "fa"
    function tl(en, fa, de) { return lang === "fa" ? fa : (lang === "de" ? de : en) }
    title: page.tl("Get cards", "دریافت کارت‌ها", "Karten holen")

    readonly property var preview: DeckExchange.preview
    readonly property bool hasPreview: preview.count !== undefined || (preview.error ?? "") !== ""
    readonly property bool ready: (preview.error ?? "") === "" && (preview.count ?? 0) > 0
    readonly property bool intoNewBox: intoNew.checked
    // New cards = all when they go into a new learning box.
    readonly property int addCount: intoNewBox ? (preview.count ?? 0) : (preview.newCount ?? 0)
    readonly property int updateCount: intoNewBox || skipExisting.checked ? 0 : (preview.existingCount ?? 0)
    property string message: ""
    property bool messageOk: true
    property bool imported: false
    function say(text, ok) { message = text; messageOk = ok }

    // New file: a new learning box named after the file is the default when the file has a title.
    Connections {
        target: DeckExchange
        function onPreviewChanged() {
            const title = DeckExchange.preview.title ?? ""
            newBoxName.text = title
            if (title !== "")
                intoNew.checked = true
            else
                intoCurrent.checked = true
            swapSides.checked = false
            skipExisting.checked = true
        }
    }
    StackView.onRemoved: DeckExchange.clearPreview()

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth

        ColumnLayout {
            width: parent.width
            spacing: 4
            LayoutMirroring.enabled: page.rtl
            LayoutMirroring.childrenInherit: true

            HowToBar { exporting: false }
            HelpLanguageBar { id: pageLang; Layout.alignment: Qt.AlignHCenter }

            // ---- Where from ----
            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.topMargin: 8
                visible: !page.hasPreview
                spacing: 4

                Button {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 56
                    highlighted: true
                    enabled: !DeckExchange.busy
                    text: page.tl("Choose a file", "انتخاب فایل", "Datei wählen")
                    onClicked: { page.message = ""; openDialog.open() }
                }
                HintLabel {
                    text: AppMode.full ? page.tl("Files ending in .lbox (LearningBox), .csv or .txt (Excel, Quizlet) or .apkg (Anki).", "فایل‌هایی با پسوند ‎lbox.‎ (LearningBox)، ‎csv.‎ یا ‎txt.‎ (اکسل، Quizlet) یا ‎apkg.‎ (Anki).", "Dateien mit der Endung .lbox (LearningBox), .csv oder .txt (Excel, Quizlet) oder .apkg (Anki).")
                                       : page.tl("Files ending in .lbox (LearningBox), .csv or .txt (Excel, Quizlet).", "فایل‌هایی با پسوند ‎lbox.‎ (LearningBox)، ‎csv.‎ یا ‎txt.‎ (اکسل، Quizlet).", "Dateien mit der Endung .lbox (LearningBox), .csv oder .txt (Excel, Quizlet).")
                }
                HintLabel {
                    text: page.tl("Your own list as a .txt file: German word, English meaning, German example sentence, each card ending with ; — tap ? above for the format.", "فهرست خودتان در یک فایل ‎txt.‎: واژهٔ آلمانی، معنی انگلیسی، جملهٔ مثال آلمانی، و هر کارت با ‎;‎ تمام شود — برای دیدن قالب، بالا روی ? بزنید.", "Deine eigene Liste als .txt-Datei: deutsches Wort, englische Bedeutung, deutscher Beispielsatz, jede Karte endet mit ; — tippe oben auf ? für das Format.")
                }

                Label {
                    Layout.topMargin: 16
                    visible: AppMode.full
                    text: page.tl("… or paste a link", "… یا یک لینک بچسبانید", "… oder einen Link einfügen")
                    font.bold: true
                }
                RowLayout {
                    Layout.fillWidth: true
                    visible: AppMode.full
                    TextField {
                        id: linkField
                        Layout.fillWidth: true
                        placeholderText: "https://drive.google.com/file/d/…"
                        inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
                        onAccepted: if (text.trim() !== "") { page.message = ""; DeckExchange.openLink(text) }
                    }
                    Button {
                        text: page.tl("Get", "دریافت", "Holen")
                        enabled: !DeckExchange.busy && linkField.text.trim() !== ""
                        onClicked: { page.message = ""; DeckExchange.openLink(linkField.text) }
                    }
                }
                HintLabel {
                    visible: AppMode.full
                    text: page.tl("A Google Drive, Dropbox or GitHub link. In Google Drive choose Share → “Anyone with the link” first.", "یک لینک Google Drive، Dropbox یا GitHub. در Google Drive ابتدا Share ← «Anyone with the link» را انتخاب کنید.", "Ein Google-Drive-, Dropbox- oder GitHub-Link. Wähle in Google Drive zuerst Share → „Anyone with the link“.")
                }
            }

            BusyIndicator {
                Layout.alignment: Qt.AlignHCenter
                visible: DeckExchange.busy
                running: visible
            }

            // ---- Preview + options ----
            Pane {
                Layout.fillWidth: true
                Layout.leftMargin: 12
                Layout.rightMargin: 12
                Layout.topMargin: 8
                visible: page.hasPreview
                Material.elevation: 2

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 4

                    Label {
                        Layout.fillWidth: true
                        visible: (page.preview.error ?? "") !== ""
                        wrapMode: Text.WordWrap
                        color: Material.color(Material.Red)
                        text: page.preview.error ?? ""
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        visible: page.ready
                        spacing: 4

                        Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            font.bold: true
                            font.pixelSize: 16
                            text: (page.preview.title ? "“" + page.preview.title + "” · " : "")
                                  + page.tl("%1 card(s)", "%1 کارت", "%1 Karte(n)").arg(page.preview.count ?? 0)
                                  + ((page.preview.pictures ?? 0) > 0 ? " · " + page.tl("%1 picture(s)", "%1 تصویر", "%1 Bild(er)").arg(page.preview.pictures) : "")
                        }
                        // First cards, so the user can check which side is German
                        Repeater {
                            model: page.preview.sample ?? []
                            delegate: Label {
                                required property var modelData
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                                opacity: 0.8
                                text: "• " + (swapSides.checked ? modelData.back.split("\n")[0] + "  →  " + modelData.front
                                                                    : modelData.front + "  →  " + modelData.back.split("\n")[0])
                            }
                        }
                        CheckBox {
                            id: swapSides
                            visible: page.preview.format !== "lbox"
                            text: page.tl("German is in the second column", "آلمانی در ستون دوم است", "Deutsch steht in der zweiten Spalte")
                        }
                        HintLabel {
                            visible: swapSides.visible
                            leftPadding: 8
                            text: page.tl("Tick this if the lines above show the meaning first.", "اگر خطوط بالا ابتدا معنی را نشان می‌دهند این را علامت بزنید.", "Hake an, wenn die Zeilen oben zuerst die Bedeutung zeigen.")
                        }

                        Label {
                            Layout.topMargin: 8
                            text: page.tl("Where should the cards go?", "کارت‌ها کجا بروند؟", "Wohin sollen die Karten?")
                            font.bold: true
                        }
                        ButtonGroup { id: targetGroup }
                        RadioButton {
                            id: intoNew
                            ButtonGroup.group: targetGroup
                            text: page.tl("A new learning box", "یک جعبهٔ یادگیری جدید", "Eine neue Lernbox")
                        }
                        TextField {
                            id: newBoxName
                            visible: intoNew.checked
                            Layout.fillWidth: true
                            Layout.leftMargin: 8
                            placeholderText: page.tl("Name, e.g. Netzwerk neu A2", "نام، مثلاً Netzwerk neu A2", "Name, z. B. Netzwerk neu A2")
                        }
                        HintLabel {
                            visible: intoNew.checked
                            leftPadding: 8
                            text: page.preview.language === "en" ? page.tl("Learning language: \uD83C\uDDFA\uD83C\uDDF8 English", "زبان یادگیری: \uD83C\uDDFA\uD83C\uDDF8 انگلیسی", "Lernsprache: \uD83C\uDDFA\uD83C\uDDF8 Englisch")
                                                                 : page.tl("Learning language: \uD83C\uDDE9\uD83C\uDDEA German", "زبان یادگیری: \uD83C\uDDE9\uD83C\uDDEA آلمانی", "Lernsprache: \uD83C\uDDE9\uD83C\uDDEA Deutsch")
                        }
                        RadioButton {
                            id: intoCurrent
                            ButtonGroup.group: targetGroup
                            checked: true
                            text: page.tl("This learning box: %1", "این جعبهٔ یادگیری: %1", "Diese Lernbox: %1").arg(CardStore.currentCollectionName)
                        }
                        HintLabel {
                            visible: intoCurrent.checked
                            leftPadding: 8
                            text: page.tl("%1 new · %2 you already have", "%1 جدید · %2 را از قبل دارید", "%1 neu · %2 hast du schon").arg(page.preview.newCount ?? 0).arg(page.preview.existingCount ?? 0)
                        }

                        Label {
                            Layout.topMargin: 8
                            visible: intoCurrent.checked && (page.preview.existingCount ?? 0) > 0
                            text: page.tl("Words you already have", "واژه‌هایی که از قبل دارید", "Wörter, die du schon hast")
                            font.bold: true
                        }
                        ButtonGroup { id: dupGroup }
                        RadioButton {
                            id: skipExisting
                            visible: intoCurrent.checked && (page.preview.existingCount ?? 0) > 0
                            ButtonGroup.group: dupGroup
                            checked: true
                            text: page.tl("Keep mine", "مال من بماند", "Meine behalten")
                        }
                        RadioButton {
                            visible: intoCurrent.checked && (page.preview.existingCount ?? 0) > 0
                            ButtonGroup.group: dupGroup
                            text: page.tl("Use the file's meaning and example", "معنی و مثال فایل استفاده شود", "Bedeutung und Beispiel aus der Datei verwenden")
                        }
                        HintLabel {
                            visible: intoCurrent.checked && (page.preview.existingCount ?? 0) > 0
                            leftPadding: 8
                            text: page.tl("Your progress for these words is kept either way.", "پیشرفت شما برای این واژه‌ها در هر صورت حفظ می‌شود.", "Dein Fortschritt für diese Wörter bleibt in jedem Fall erhalten.")
                        }

                        CheckBox {
                            id: keepProgress
                            Layout.topMargin: 4
                            visible: page.preview.hasProgress === true
                            checked: true
                            text: page.tl("Keep the progress from the file", "پیشرفت فایل حفظ شود", "Fortschritt aus der Datei behalten")
                        }
                        HintLabel {
                            visible: keepProgress.visible
                            leftPadding: 8
                            text: page.tl("On: cards go into the boxes they had. Off: they start in the box below.", "روشن: کارت‌ها به جعبه‌هایی که داشتند می‌روند. خاموش: از جعبهٔ زیر شروع می‌شوند.", "An: Die Karten kommen in die Boxen, in denen sie waren. Aus: Sie beginnen in der Box unten.")
                        }
                        RowLayout {
                            Layout.topMargin: 4
                            visible: !(keepProgress.visible && keepProgress.checked)
                            Label { text: page.tl("Start in", "شروع از", "Beginnen in") }
                            ComboBox {
                                id: importBox
                                Layout.preferredWidth: 160
                                model: BoxNames.all
                            }
                        }
                        HintLabel {
                            visible: !(keepProgress.visible && keepProgress.checked)
                            leftPadding: 8
                            text: page.tl("New cards are asked from this box.", "کارت‌های جدید از این جعبه پرسیده می‌شوند.", "Neue Karten werden ab dieser Box gefragt.")
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.topMargin: 8
                        Button {
                            flat: true
                            text: page.tl("Choose another file", "انتخاب فایل دیگر", "Andere Datei wählen")
                            onClicked: DeckExchange.clearPreview()
                        }
                        Item { Layout.fillWidth: true }
                        Button {
                            highlighted: true
                            visible: page.ready
                            enabled: page.addCount + page.updateCount > 0
                            text: page.updateCount > 0 ? page.tl("Add %1 · update %2", "افزودن %1 · به‌روزرسانی %2", "%1 hinzufügen · %2 aktualisieren").arg(page.addCount).arg(page.updateCount)
                                                       : page.tl("Add %1 card(s)", "افزودن %1 کارت", "%1 Karte(n) hinzufügen").arg(page.addCount)
                            onClicked: {
                                const r = DeckExchange.applyImport(skipExisting.checked ? "skip" : "update",
                                                                   keepProgress.visible && keepProgress.checked,
                                                                   importBox.currentIndex + 1, swapSides.checked,
                                                                   intoNew.checked ? (newBoxName.text.trim() || qsTr("Imported cards")) : "")
                                if (r.error) {
                                    page.say(r.error, false)
                                } else {
                                    page.say(page.tl("Added %1 · updated %2 · skipped %3", "افزوده شد %1 · به‌روز شد %2 · ردشده %3", "Hinzugefügt %1 · aktualisiert %2 · übersprungen %3").arg(r.added).arg(r.updated).arg(r.skipped), true)
                                    page.imported = true
                                    DeckExchange.clearPreview()
                                }
                            }
                        }
                    }
                }
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
            Button {
                Layout.leftMargin: 16
                visible: page.imported
                text: page.tl("Open “%1”", "باز کردن «%1»", "„%1“ öffnen").arg(CardStore.currentCollectionName)
                onClicked: page.StackView.view.pop(null)
            }
            Item { Layout.preferredHeight: 16 }
        }
    }

    FileDialog {
        id: openDialog
        title: page.tl("Choose a card file", "انتخاب فایل کارت", "Kartendatei wählen")
        fileMode: FileDialog.OpenFile
        // Android matches filters by file type; .lbox/.apkg are unknown there, so show all files.
        nameFilters: Qt.platform.os === "android" ? []
                     : [qsTr("Card files (*.lbox *.apkg *.csv *.txt *.tsv)"), qsTr("All files (*)")]
        onAccepted: {
            page.imported = false
            DeckExchange.openFile(selectedFile)
        }
    }
}
