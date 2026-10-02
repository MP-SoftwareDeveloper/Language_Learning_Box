import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Controls.Material
import LearningBox

// A "?" row at the top of the Export and Import pages. Tapping it opens a short how-to
// in English, Persian or German (the same language choice as the Help page).
RowLayout {
    id: root
    property bool exporting: true

    Layout.fillWidth: true
    Layout.leftMargin: 16
    Layout.rightMargin: 8
    Layout.topMargin: 4
    spacing: 4

    readonly property string lang: langBar.lang
    readonly property bool rtl: lang === "fa"

    readonly property var texts: ({
        en: {
            exp: {
                title: "How to export your cards",
                intro: "Save your learning box as a file — for a friend, a new phone or a backup.",
                steps: "1. Choose who it is for: another LearningBox app (.lbox, keeps pictures) or Excel / Anki / Quizlet (.csv table).\n2. Choose which cards: all of them, one box, or only learned cards.\n3. Choose whether to keep your progress (the boxes).\n4. Tap Share to send the file with WhatsApp, Telegram, e-mail or Drive — or Save file to keep it on this phone.",
                hint: "Your friend opens the file in LearningBox → Import."
            },
            imp: {
                title: "How to import cards",
                intro: "Add cards a friend sent you, or from Quizlet, Excel or (Full app) Anki.",
                steps: "1. Choose a file (.lbox, .csv, .txt or, in the Full app, .apkg) — or paste a Google Drive, Dropbox or GitHub link and tap Get.\n2. Check the preview: how many cards are new and which ones you already have.\n3. Choose where the cards go: a new learning box or this one, and which box they start in.\n4. Tap Add to put the cards in your box.",
                hint: "Your progress for words you already have is kept.",
                fileTitle: "Make your own text file (.txt)",
                fileIntro: "Write each card as: German word, English meaning, German example sentence — and end the card with ;\nPut each part on its own line, or the whole card on one line separated by commas. Don't use ; inside a sentence. Save the file as .txt (UTF-8), then pick it with Choose a file.",
                fileExample: "Haus\nhouse\nDas Haus ist groß.;\n\nBaum, tree, Der Baum ist alt.;"
            }
        },
        fa: {
            exp: {
                title: "راهنمای خروجی گرفتن از کارت‌ها (Export)",
                intro: "جعبهٔ یادگیری خود را به‌صورت یک فایل ذخیره کنید – برای دوستی، گوشی تازه یا پشتیبان‌گیری.",
                steps: "۱. مشخص کنید برای چه کسی است: برنامهٔ LearningBox دیگر (فایل ‎.lbox همراه عکس‌ها) یا Excel / Anki / Quizlet (جدول ‎.csv).\n۲. کدام کارت‌ها: همه، فقط یک خانه یا فقط کارت‌های یادگرفته‌شده.\n۳. مشخص کنید پیشرفت (خانه‌ها) حفظ شود یا نه.\n۴. با Share فایل را با واتس‌اپ، تلگرام، ایمیل یا Drive بفرستید – یا با Save file آن را روی همین گوشی نگه دارید.",
                hint: "دوست شما فایل را در LearningBox ← Import باز می‌کند."
            },
            imp: {
                title: "راهنمای ورودی گرفتن کارت‌ها (Import)",
                intro: "کارت‌هایی را که دوستی فرستاده یا از Quizlet، Excel (و در نسخهٔ کامل Anki) آمده‌اند به جعبهٔ خود اضافه کنید.",
                steps: "۱. یک فایل (‎.lbox، ‎.csv، ‎.txt یا در نسخهٔ کامل ‎.apkg) انتخاب کنید – یا لینک Google Drive، Dropbox یا GitHub را وارد کنید و Get را بزنید.\n۲. پیش‌نمایش را ببینید: چند کارت تازه است و کدام‌ها را از قبل دارید.\n۳. مشخص کنید کارت‌ها کجا بروند: جعبهٔ یادگیری تازه یا همین جعبه، و از کدام خانه شروع شوند.\n۴. با Add کارت‌ها را به جعبه اضافه کنید.",
                hint: "پیشرفت شما برای واژه‌هایی که از قبل دارید حفظ می‌شود.",
                fileTitle: "ساختن فایل متنی خودتان (\u200E.txt)",
                fileIntro: "هر کارت را این‌طور بنویسید: واژهٔ آلمانی، معنی انگلیسی، جملهٔ نمونهٔ آلمانی – و در پایان کارت ; بگذارید.\nهر بخش را در یک خط جدا بنویسید، یا کل کارت را در یک خط و با ویرگول (,) جدا کنید. داخل جمله ; نگذارید. فایل را با قالب \u200E.txt (UTF-8) ذخیره کنید و با Choose a file انتخاب کنید.",
                fileExample: "Haus\nhouse\nDas Haus ist groß.;\n\nBaum, tree, Der Baum ist alt.;"
            }
        },
        de: {
            exp: {
                title: "So exportierst du deine Karten",
                intro: "Speichere deine Lernbox als Datei – für einen Freund, ein neues Handy oder als Sicherung.",
                steps: "1. Wähle, für wen sie ist: eine andere LearningBox-App (.lbox, mit Bildern) oder Excel / Anki / Quizlet (.csv-Tabelle).\n2. Wähle die Karten: alle, nur ein Fach oder nur gelernte Karten.\n3. Wähle, ob dein Fortschritt (die Fächer) erhalten bleiben soll.\n4. Tippe auf Share, um die Datei per WhatsApp, Telegram, E-Mail oder Drive zu senden – oder auf Save file, um sie auf diesem Handy zu speichern.",
                hint: "Dein Freund öffnet die Datei in LearningBox → Import."
            },
            imp: {
                title: "So importierst du Karten",
                intro: "Füge Karten hinzu, die ein Freund dir geschickt hat oder die aus Quizlet, Excel oder (Vollversion) Anki stammen.",
                steps: "1. Wähle eine Datei (.lbox, .csv, .txt oder in der Vollversion .apkg) – oder füge einen Google-Drive-, Dropbox- oder GitHub-Link ein und tippe auf Get.\n2. Prüfe die Vorschau: wie viele Karten neu sind und welche du schon hast.\n3. Wähle, wohin die Karten kommen: in eine neue Lernbox oder in diese, und in welchem Fach sie starten.\n4. Tippe auf Add, um die Karten hinzuzufügen.",
                hint: "Dein Fortschritt bei Wörtern, die du schon hast, bleibt erhalten.",
                fileTitle: "Eigene Textdatei erstellen (.txt)",
                fileIntro: "Schreibe jede Karte so: deutsches Wort, englische Bedeutung, deutscher Beispielsatz – und beende die Karte mit ;\nJeder Teil steht in einer eigenen Zeile, oder die ganze Karte steht in einer Zeile, durch Kommas getrennt. Verwende kein ; innerhalb eines Satzes. Speichere die Datei als .txt (UTF-8) und wähle sie mit Choose a file aus.",
                fileExample: "Haus\nhouse\nDas Haus ist groß.;\n\nBaum, tree, Der Baum ist alt.;"
            }
        }
    })
    readonly property var t: texts[lang][exporting ? "exp" : "imp"]

    Label {
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        opacity: 0.7
        font.pixelSize: 14
        text: root.exporting ? qsTr("Not sure how exporting works? Tap ?")
                             : qsTr("Not sure how importing works? Tap ?")
    }
    ToolButton {
        Layout.preferredWidth: 48
        Layout.preferredHeight: 48
        focusPolicy: Qt.NoFocus
        contentItem: Item {
            HelpIcon {
                anchors.centerIn: parent
                width: 32
                height: 32
                color: Material.accent
            }
        }
        onClicked: howDialog.open()
    }

    Dialog {
        id: howDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        width: Math.min(400, (parent ? parent.width : 400) - 16)
        height: Math.min(implicitHeight, (parent ? parent.height : 640) - 16)
        contentItem: Flickable {
            implicitHeight: howColumn.implicitHeight
            contentHeight: howColumn.implicitHeight
            clip: true
            ScrollBar.vertical: ScrollBar {}
            ColumnLayout {
                id: howColumn
                width: parent.width
                spacing: 10
                HelpLanguageBar {
                    id: langBar
                    Layout.alignment: Qt.AlignHCenter
                }
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    font.pixelSize: 18
                    font.bold: true
                    color: Material.accent
                    horizontalAlignment: root.rtl ? Text.AlignRight : Text.AlignLeft
                    text: root.t.title
                }
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    horizontalAlignment: root.rtl ? Text.AlignRight : Text.AlignLeft
                    text: root.t.intro
                }
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    lineHeight: 1.2
                    horizontalAlignment: root.rtl ? Text.AlignRight : Text.AlignLeft
                    text: root.t.steps
                }
                HintLabel {
                    horizontalAlignment: root.rtl ? Text.AlignRight : Text.AlignLeft
                    text: root.t.hint
                }

                // Import only: how to write your own card list
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: 6
                    spacing: 8
                    visible: (root.t.fileTitle ?? "") !== ""
                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        font.pixelSize: 16
                        font.bold: true
                        color: Material.accent
                        horizontalAlignment: root.rtl ? Text.AlignRight : Text.AlignLeft
                        text: root.t.fileTitle ?? ""
                    }
                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        horizontalAlignment: root.rtl ? Text.AlignRight : Text.AlignLeft
                        text: root.t.fileIntro ?? ""
                    }
                    Pane {
                        Layout.fillWidth: true
                        padding: 10
                        background: Rectangle {
                            radius: 6
                            color: Qt.rgba(0.5, 0.5, 0.5, 0.15)
                        }
                        Label {
                            width: parent.width
                            wrapMode: Text.WordWrap
                            font.family: "monospace"
                            font.pixelSize: 14
                            horizontalAlignment: Text.AlignLeft
                            text: root.t.fileExample ?? ""
                        }
                    }
                }
            }
        }
        footer: DialogButtonBox {
            Button {
                flat: true
                text: qsTr("Close")
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            }
        }
    }
}
