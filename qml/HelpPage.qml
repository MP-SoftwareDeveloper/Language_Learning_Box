import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Controls.Material
import LearningBox

// How to use the app, in English or Persian (switch at the top).
// Starts in Persian when Persian is the translation language in Settings.
Page {
    id: page
    objectName: "helpPage"
    title: lang === "fa" ? "راهنما" : qsTr("Help")

    property string lang: Translator.targetLanguage === "fa" ? "fa" : "en"
    readonly property bool rtl: lang === "fa"
    readonly property var sections: lang === "fa" ? faSections : enSections

    readonly property var enSections: [
        { title: "The idea: a Leitner box",
          body: "Every word is a card. New cards start in Box 1. When you know a card in a review it moves up one box; when you don't, it goes back to Box 1.\n\nEach box waits longer before the card is asked again: Box 1 = 1 day, Box 2 = 2 days, Box 3 = 4 days, Box 4 = 8 days, Box 5 = 16 days. A card you know in Box 5 is Learned and is not asked any more." },
        { title: "Simple and Full app",
          body: "The Simple app is just Leitner flashcards: add cards, review, listen, send and get cards. It works offline and asks for no permissions.\n\nThe Full app adds Lens (words from photos), handwriting recognition, meanings and example sentences filled in while you type, online translation, the Netzwerk neu A1 word pack, pictures on cards and Anki decks.\n\nSwitch any time in Settings \u2192 App. Your cards and progress are kept." },
        { title: "Learning boxes",
          body: "You can keep separate learning boxes, e.g. one per book (“Netzwerk neu A2”). The box at the top of Home is the selected one; the one you used before is shown under it – tap it to switch. The arrow opens the full list.\n\n+ creates a new learning box (empty, or started with a word pack). ⋮ renames or deletes the selected one. Review, Add card, Lens, All cards, word packs and export always work on the selected learning box. The app remembers your choice." },
        { title: "Learning English",
          body: "A learning box can also be for English: + \u2192 I want to learn: \uD83C\uDDFA\uD83C\uDDF8 English. In an English box the words are read with an American voice and Lens reads English text. For every learning box you choose the language of the meanings (Settings \u2192 Translation, or the switch on the answer side): German boxes Persian or English, English boxes Persian or German. English and German meanings and translated example sentences have a \uD83D\uDD0A button too; Persian is not read aloud. The German starter words and word packs are not used there. The flag in front of each learning box shows its language." },
        { title: "Review",
          body: "Home → Start review shows the cards that are due today. Try to remember the meaning, then tap Show the meaning (or Show the German word) and choose Got it! or Not yet. Cards you didn't know are asked again at the end of the session.\n\nOn the answer side you can switch the meaning between فارسی and English. The speaker buttons read the word and the example aloud; Auto-speak reads each new card.\n\nThe switch at the top chooses the way round: Deutsch \u2192 meaning (read the German word, do you know what it means?), meaning \u2192 Deutsch (see the meaning, can you say it in German \u2013 with der / die / das and the plural?) or \uD83D\uDD00 Mix (every card from either side). It is remembered for each learning box; the boxes and progress are the same either way. Then tap Got it! or Not yet.\n\nEdit opens the card to correct it. The box menu (top left) puts the card into any box by hand; the next card follows." },
        { title: "Boxes 1–5 and Learned",
          body: "Tap a coloured box on Home to see its cards. ‹ and › move a card one box back or forward (Undo is shown for a few seconds). Press and hold a card for Edit, Move to… and Delete. + adds a new card straight into that box." },
        { title: "Start over",
          body: "Home \u2192 \u22EE \u2192 Start over puts all cards of the selected learning box back into Box 1 \u2013 useful after a long break or to go through a book again. The cards themselves are kept. You can leave learned cards as they are, spread the cards over several days (e.g. 10 days \u2248 30 cards a day) and clear the statistics. A box page (Box 2 \u2026 Learned) has the same for one box: Move all to Box 1. Undo is offered for 10 seconds." },
        { title: "Adding a card",
          body: "Home → Add card. Type the German word: the app suggests words, fills in the meaning (from the word pack, from saved translations, or online) and offers example sentences – tap one to use it.\n\nPictures: 📷 Take photo uses the camera, Add picture chooses one from your gallery. Choose the box the card starts in. If the word is already in your learning box, the app asks whether to update the existing card." },
        { title: "Lens: words from a photo",
          body: "Home → Lens. Photograph German text (or pick a photo from the gallery). Hold the text flat and well lit; landscape photos are turned upright automatically.\n\nTap words to select them, hold a word to select the whole sentence. The translation appears under the selection. Create card saves the selected words (with meaning) into the selected learning box – or just read the translation without saving.\n\nFor handwriting, set up online recognition in Settings (free Azure key)." },
        { title: "Word packs",
          body: "Home → Word packs: ready-made vocabulary of Netzwerk neu A1 by chapter, with Persian meanings and example sentences. Add single chapters or all of them (Full app).\n\n100 starter words: everyday words with English and Persian meanings and example sentences \u2013 add them to any learning box from Word packs, or choose them when you create a new learning box (+)." },
        { title: "All cards",
          body: "Search all cards of the selected learning box (German, Persian or English). Tap a card to edit it, hold it to delete it." },
        { title: "Dictionary",
          body: "Home \u2192 \uD83D\uDCD6 Dictionary (the last button) looks up a word or sentence between the language of the selected learning box and its meaning language, in both directions \u2013 the arrows button swaps them. You get the translation, other meanings, example sentences and \uD83D\uDD0A for German and English; + Add puts the word straight into the learning box. German words from the word pack are found offline; everything else needs online translation (Full app) and is then saved for offline use." },
        { title: "Send and get cards",
          body: "Home \u2192 \u2B06 Send or back up cards: choose who it is for (another LearningBox app \u2192 .lbox file with pictures; Excel, Anki or Quizlet \u2192 .csv table), which cards, and whether to keep your progress. Share sends the file straight away with WhatsApp, Telegram, e-mail or Drive; Save file keeps it on the phone.\n\nHome \u2192 \u2B07 Get cards from a file or link: choose a .lbox, .csv/.txt or (Full app) Anki .apkg file, or paste a Google Drive, Dropbox or GitHub link. The preview shows how many cards are new and the first cards, so you can check which side is German. Choose a new learning box or the selected one, what to do with words you already have, and press Add." },
        { title: "Settings (gear icon)",
          body: "Translation: Persian or English as the meaning language, and whether to translate online when connected. Offline, saved translations and the word pack are used.\n\nText recognition: optional Azure AI Vision key for handwriting (free tier). Speech: reading speed. If no German voice is installed, install it in Android Settings → Text-to-speech." },
        { title: "Tips",
          body: "• Review a little every day – the boxes do the planning.\n• Learn nouns with their article (der / die / das) and plural.\n• Say the example sentence aloud after the speaker.\n• Keep one learning box per book or course." }
    ]

    readonly property var faSections: [
        { title: "ایده: جعبهٔ لایتنر",
          body: "هر واژه یک کارت است. کارت‌های تازه در خانهٔ ۱ قرار می‌گیرند. اگر در مرور معنی کارت را بدانید، یک خانه جلو می‌رود و اگر ندانید، به خانهٔ ۱ برمی‌گردد.\n\nهر خانه مدت بیشتری صبر می‌کند تا کارت دوباره پرسیده شود: خانهٔ ۱ = ۱ روز، خانهٔ ۲ = ۲ روز، خانهٔ ۳ = ۴ روز، خانهٔ ۴ = ۸ روز، خانهٔ ۵ = ۱۶ روز. کارتی که در خانهٔ ۵ درست جواب داده شود «یادگرفته‌شده» است و دیگر پرسیده نمی‌شود." },
        { title: "نسخهٔ ساده و نسخهٔ کامل",
          body: "نسخهٔ ساده فقط کارت‌های لایتنر است: افزودن کارت، مرور، شنیدن تلفظ، فرستادن و گرفتن کارت‌ها. بدون اینترنت کار می‌کند و هیچ مجوزی نمی‌خواهد.\n\nنسخهٔ کامل این‌ها را اضافه می‌کند: لنز (واژه از روی عکس)، تشخیص دست‌خط، پر شدن خودکار معنی و جملهٔ نمونه هنگام نوشتن، ترجمهٔ آنلاین، بستهٔ واژهٔ Netzwerk neu A1، عکس روی کارت‌ها و دسته‌کارت‌های Anki.\n\nهر وقت خواستید در تنظیمات \u2190 App عوض کنید. کارت‌ها و پیشرفت شما حفظ می‌شوند." },
        { title: "جعبه‌های یادگیری",
          body: "می‌توانید چند جعبهٔ یادگیری جدا داشته باشید، مثلاً یکی برای هر کتاب (Netzwerk neu A2). جعبهٔ بالای صفحهٔ اصلی جعبهٔ انتخاب‌شده است و جعبه‌ای که قبلاً استفاده کرده‌اید زیر آن نمایش داده می‌شود؛ برای رفتن به آن رویش بزنید. فلش، فهرست کامل را باز می‌کند.\n\nدکمهٔ + یک جعبهٔ یادگیری تازه می‌سازد (خالی یا با یک بستهٔ واژه). دکمهٔ ⋮ نام جعبهٔ انتخاب‌شده را تغییر می‌دهد یا آن را حذف می‌کند. مرور، افزودن کارت، لنز، همهٔ کارت‌ها، بسته‌های واژه و خروجی همیشه روی جعبهٔ انتخاب‌شده کار می‌کنند. برنامه انتخاب شما را به خاطر می‌سپارد." },
        { title: "یادگیری انگلیسی",
          body: "جعبهٔ یادگیری می‌تواند برای انگلیسی هم باشد: + \u2190 I want to learn: \uD83C\uDDFA\uD83C\uDDF8 English. در جعبهٔ انگلیسی واژه‌ها با لهجهٔ آمریکایی خوانده می‌شوند و لنز متن انگلیسی را می‌خواند. برای هر جعبهٔ یادگیری زبان معنی‌ها را انتخاب می‌کنید (تنظیمات \u2190 Translation یا کلید سمت پاسخ): جعبه‌های آلمانی فارسی یا انگلیسی، جعبه‌های انگلیسی فارسی یا آلمانی. معنی‌ها و ترجمهٔ جمله‌های نمونه به انگلیسی یا آلمانی هم دکمهٔ \uD83D\uDD0A دارند؛ فارسی خوانده نمی‌شود. واژه‌های آغازین و بسته‌های واژهٔ آلمانی در آن استفاده نمی‌شوند. پرچم کنار نام هر جعبهٔ یادگیری زبان آن را نشان می‌دهد." },
        { title: "مرور",
          body: "در صفحهٔ اصلی Start review کارت‌هایی را که امروز نوبتشان است نشان می‌دهد. سعی کنید معنی را به یاد بیاورید، سپس Show the meaning (یا Show the German word) را بزنید و Got it! (بلد بودم) یا Not yet (هنوز نه) را انتخاب کنید. کارت‌هایی که بلد نبودید در پایان همان جلسه دوباره پرسیده می‌شوند.\n\nدر سمت پاسخ می‌توانید زبان معنی را بین فارسی و English عوض کنید. دکمه‌های بلندگو واژه و جملهٔ نمونه را می‌خوانند؛ Auto-speak هر کارت تازه را خودکار می‌خواند.\n\nکلید بالای کارت جهت مرور را تعیین می‌کند: Deutsch \u2190 معنی (واژهٔ آلمانی را می‌بینید؛ معنی‌اش را می‌دانید؟)، معنی \u2190 Deutsch (معنی را می‌بینید؛ می‌توانید آن را به آلمانی بگویید؟ با der / die / das و جمع) یا \uD83D\uDD00 Mix (هر کارت از یکی از دو طرف). این انتخاب برای هر جعبهٔ یادگیری جدا ذخیره می‌شود و خانه‌ها و پیشرفت در هر حالت یکی است. سپس Got it! (بلد بودم) یا Not yet (هنوز نه) را بزنید.\n\nبا Edit کارت را برای اصلاح باز می‌کنید. با منوی خانه (بالا سمت چپ) می‌توانید کارت را دستی به هر خانه‌ای ببرید؛ سپس کارت بعدی می‌آید." },
        { title: "خانه‌های ۱ تا ۵ و یادگرفته‌شده",
          body: "روی یکی از خانه‌های رنگی صفحهٔ اصلی بزنید تا کارت‌هایش را ببینید. با ‹ و › کارت یک خانه عقب یا جلو می‌رود (دکمهٔ Undo چند ثانیه نمایش داده می‌شود). انگشت را روی کارت نگه دارید تا گزینه‌های ویرایش، انتقال و حذف ظاهر شوند. دکمهٔ + کارت تازه را مستقیم در همان خانه می‌گذارد." },
        { title: "از نو شروع کردن",
          body: "صفحهٔ اصلی \u2190 \u22EE \u2190 Start over همهٔ کارت‌های جعبهٔ یادگیری انتخاب‌شده را به خانهٔ ۱ برمی‌گرداند؛ برای بعد از یک وقفهٔ طولانی یا دوباره خواندن یک کتاب مناسب است. خود کارت‌ها حفظ می‌شوند. می‌توانید کارت‌های یادگرفته‌شده را دست نزنید، کارت‌ها را در چند روز پخش کنید (مثلاً ۱۰ روز \u2248 روزی ۳۰ کارت) و آمار را پاک کنید. در صفحهٔ هر خانه (خانهٔ ۲ تا یادگرفته‌شده) همین کار برای یک خانه ممکن است: Move all to Box 1. تا ۱۰ ثانیه می‌توانید Undo را بزنید." },
        { title: "افزودن کارت",
          body: "صفحهٔ اصلی ← Add card. واژهٔ آلمانی را بنویسید: برنامه واژه پیشنهاد می‌دهد، معنی را خودش پر می‌کند (از بستهٔ واژه، ترجمه‌های ذخیره‌شده یا به‌صورت آنلاین) و جمله‌های نمونه پیشنهاد می‌کند؛ برای استفاده روی یکی بزنید.\n\nعکس: با 📷 Take photo با دوربین عکس بگیرید یا با Add picture از گالری انتخاب کنید. خانه‌ای را که کارت از آن شروع می‌کند انتخاب کنید. اگر واژه از قبل در جعبه باشد، برنامه می‌پرسد که آیا کارت موجود به‌روز شود." },
        { title: "لنز: واژه از روی عکس",
          body: "صفحهٔ اصلی ← Lens. از متن آلمانی عکس بگیرید (یا از گالری انتخاب کنید). متن را صاف و در نور کافی نگه دارید؛ عکس‌های افقی خودکار صاف می‌شوند.\n\nروی واژه‌ها بزنید تا انتخاب شوند و انگشت را روی یک واژه نگه دارید تا کل جمله انتخاب شود. ترجمه زیر متن انتخاب‌شده نمایش داده می‌شود. Create card واژه‌های انتخاب‌شده را با معنی در جعبهٔ یادگیری ذخیره می‌کند؛ می‌توانید فقط ترجمه را بخوانید و چیزی ذخیره نکنید.\n\nبرای دست‌خط، تشخیص آنلاین را در تنظیمات فعال کنید (کلید رایگان Azure)." },
        { title: "بسته‌های واژه",
          body: "صفحهٔ اصلی ← Word packs: واژه‌های آمادهٔ کتاب Netzwerk neu A1 به تفکیک فصل، با معنی فارسی و جملهٔ نمونه. می‌توانید یک فصل یا همهٔ فصل‌ها را اضافه کنید (نسخهٔ کامل).\n\n۱۰۰ واژهٔ آغازین: واژه‌های روزمره با معنی انگلیسی و فارسی و جملهٔ نمونه؛ از صفحهٔ Word packs به هر جعبهٔ یادگیری اضافه کنید یا هنگام ساختن جعبهٔ یادگیری تازه (+) انتخاب کنید." },
        { title: "همهٔ کارت‌ها",
          body: "در همهٔ کارت‌های جعبهٔ یادگیری انتخاب‌شده جست‌وجو کنید (آلمانی، فارسی یا انگلیسی). برای ویرایش روی کارت بزنید و برای حذف انگشت را روی آن نگه دارید." },
        { title: "فرهنگ لغت",
          body: "صفحهٔ اصلی \u2190 \uD83D\uDCD6 Dictionary (آخرین دکمه) یک واژه یا جمله را بین زبان جعبهٔ یادگیری انتخاب‌شده و زبان معنی آن، در هر دو جهت جست‌وجو می‌کند؛ دکمهٔ فلش‌ها جهت را عوض می‌کند. ترجمه، معنی‌های دیگر، جمله‌های نمونه و \uD83D\uDD0A برای آلمانی و انگلیسی نمایش داده می‌شود و با + Add واژه مستقیم به جعبهٔ یادگیری اضافه می‌شود. واژه‌های آلمانی بستهٔ واژه بدون اینترنت پیدا می‌شوند؛ بقیه به ترجمهٔ آنلاین (نسخهٔ کامل) نیاز دارند و بعد برای استفادهٔ آفلاین ذخیره می‌شوند." },
        { title: "فرستادن و گرفتن کارت‌ها",
          body: "صفحهٔ اصلی \u2190 \u2B06 فرستادن یا پشتیبان‌گیری: مشخص کنید برای چه کسی است (برنامهٔ LearningBox دیگر \u2190 فایل \u200E.lbox همراه عکس‌ها؛ Excel، Anki یا Quizlet \u2190 جدول \u200E.csv)، کدام کارت‌ها و اینکه پیشرفت شما حفظ شود یا نه. Share فایل را همان لحظه با واتس‌اپ، تلگرام، ایمیل یا Drive می‌فرستد؛ Save file آن را روی گوشی نگه می‌دارد.\n\nصفحهٔ اصلی \u2190 \u2B07 گرفتن کارت از فایل یا لینک: یک فایل \u200E.lbox، \u200E.csv/.txt یا (در نسخهٔ کامل) Anki \u200E.apkg را انتخاب کنید یا لینک Google Drive، Dropbox یا GitHub را وارد کنید. پیش‌نمایش نشان می‌دهد چند کارت تازه است و چند کارت اول را نمایش می‌دهد تا ببینید کدام طرف آلمانی است. جعبهٔ یادگیری تازه یا جعبهٔ انتخاب‌شده را برگزینید، مشخص کنید با واژه‌هایی که از قبل دارید چه شود و Add را بزنید." },
        { title: "تنظیمات (آیکون چرخ‌دنده)",
          body: "ترجمه: فارسی یا انگلیسی به‌عنوان زبان معنی، و اینکه هنگام اتصال به اینترنت ترجمهٔ آنلاین استفاده شود یا نه. بدون اینترنت، ترجمه‌های ذخیره‌شده و بستهٔ واژه استفاده می‌شوند.\n\nتشخیص متن: کلید اختیاری Azure AI Vision برای دست‌خط (نسخهٔ رایگان). گفتار: سرعت خواندن. اگر صدای آلمانی نصب نیست، آن را در تنظیمات اندروید ← Text-to-speech نصب کنید." },
        { title: "نکته‌ها",
          body: "• هر روز کمی مرور کنید؛ برنامه‌ریزی را خانه‌ها انجام می‌دهند.\n• اسم‌ها را همراه حرف تعریف (der / die / das) و جمع یاد بگیرید.\n• جملهٔ نمونه را بعد از بلندگو با صدای بلند تکرار کنید.\n• برای هر کتاب یا دوره یک جعبهٔ یادگیری جدا داشته باشید." }
    ]

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Language switch
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 8
            spacing: 0
            Repeater {
                model: [ { code: "en", label: "English" }, { code: "fa", label: "فارسی" } ]
                delegate: Button {
                    required property var modelData
                    readonly property bool selected: page.lang === modelData.code
                    flat: !selected
                    highlighted: selected
                    focusPolicy: Qt.NoFocus
                    text: modelData.label
                    onClicked: {
                        page.lang = modelData.code
                        flick.contentY = 0
                    }
                }
            }
        }

        Flickable {
            id: flick
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentHeight: content.implicitHeight + 32
            clip: true
            ScrollBar.vertical: ScrollBar {}

            ColumnLayout {
                id: content
                x: 16
                y: 8
                width: flick.width - 32
                spacing: 6

                Repeater {
                    model: page.sections
                    delegate: ColumnLayout {
                        required property var modelData
                        required property int index
                        Layout.fillWidth: true
                        spacing: 4
                        Label {
                            Layout.fillWidth: true
                            Layout.topMargin: index === 0 ? 4 : 14
                            text: modelData.title
                            font.pixelSize: 18
                            font.bold: true
                            color: Material.accent
                            wrapMode: Text.WordWrap
                            horizontalAlignment: page.rtl ? Text.AlignRight : Text.AlignLeft
                        }
                        Label {
                            Layout.fillWidth: true
                            text: modelData.body
                            font.pixelSize: 15
                            lineHeight: 1.2
                            wrapMode: Text.WordWrap
                            horizontalAlignment: page.rtl ? Text.AlignRight : Text.AlignLeft
                        }
                    }
                }
            }
        }
    }
}
