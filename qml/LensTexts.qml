import QtQuick

// Texts about Lens (words from a photo) in the four help languages: the setup question, the "Lens is turned off"
// message on Home and the Settings switch. t(key, lang) falls back to English.
QtObject {
    readonly property var tx: ({
        "en": {
            "askTitle": "Use Lens?",
            "askBody": "Lens reads printed text from a photo (a book page, a sign, a menu). You tap the words you want and they become cards with their meaning. The camera is only used when you open Lens, and photos stay on your phone (unless you turn on online recognition in Settings).",
            "askSwitch": "Yes, use Lens (recommended)",
            "askHint": "You can change this any time in Settings.",
            "offTitle": "Lens is turned off",
            "offBody": "You chose not to use Lens. Turn it on to read words from a photo.",
            "turnOn": "Turn on Lens",
            "close": "Close",
            "setLens": "Use Lens (words from a photo)",
            "setLensHint": "Shows the Lens button on Home. Turn it off if you do not want to use the camera."
        },
        "fa": {
            "askTitle": "از لنز استفاده می‌کنید؟",
            "askBody": "لنز متن چاپ‌شده را از روی عکس می‌خواند (صفحهٔ کتاب، تابلو، منو). روی واژه‌هایی که می‌خواهید بزنید تا با معنی به کارت تبدیل شوند. دوربین فقط هنگام باز کردن لنز استفاده می‌شود و عکس‌ها روی گوشی می‌مانند (مگر اینکه در تنظیمات تشخیص آنلاین را روشن کنید).",
            "askSwitch": "بله، از لنز استفاده می‌کنم (پیشنهادی)",
            "askHint": "هر زمان می‌توانید در تنظیمات تغییر دهید.",
            "offTitle": "لنز خاموش است",
            "offBody": "استفاده از لنز را انتخاب نکردید. برای خواندن واژه‌ها از روی عکس، آن را روشن کنید.",
            "turnOn": "روشن کردن لنز",
            "close": "بستن",
            "setLens": "استفاده از لنز (واژه از روی عکس)",
            "setLensHint": "دکمهٔ لنز را در صفحهٔ اصلی نشان می‌دهد. اگر نمی‌خواهید از دوربین استفاده شود، خاموشش کنید."
        },
        "de": {
            "askTitle": "Lens verwenden?",
            "askBody": "Lens liest gedruckten Text von einem Foto (Buchseite, Schild, Speisekarte). Du tippst die gewünschten Wörter an, und sie werden zu Karten mit Bedeutung. Die Kamera wird nur beim Öffnen von Lens benutzt, und die Fotos bleiben auf deinem Handy (außer du schaltest in den Einstellungen die Online-Erkennung ein).",
            "askSwitch": "Ja, Lens verwenden (empfohlen)",
            "askHint": "Das kannst du jederzeit in den Einstellungen ändern.",
            "offTitle": "Lens ist ausgeschaltet",
            "offBody": "Du hast Lens abgewählt. Schalte es ein, um Wörter von einem Foto zu lesen.",
            "turnOn": "Lens einschalten",
            "close": "Schließen",
            "setLens": "Lens verwenden (Wörter aus einem Foto)",
            "setLensHint": "Zeigt den Lens-Knopf auf Home. Ausschalten, wenn du die Kamera nicht nutzen möchtest."
        },
        "sv": {
            "askTitle": "Använda Lens?",
            "askBody": "Lens läser tryckt text från ett foto (en boksida, en skylt, en meny). Du trycker på de ord du vill ha och de blir kort med betydelse. Kameran används bara när du öppnar Lens, och fotona stannar på din telefon (om du inte slår på igenkänning online i Inställningar).",
            "askSwitch": "Ja, använd Lens (rekommenderas)",
            "askHint": "Du kan ändra det när som helst i Inställningar.",
            "offTitle": "Lens är avstängt",
            "offBody": "Du valde att inte använda Lens. Slå på det för att läsa ord från ett foto.",
            "turnOn": "Slå på Lens",
            "close": "Stäng",
            "setLens": "Använd Lens (ord från ett foto)",
            "setLensHint": "Visar Lens-knappen på startsidan. Stäng av om du inte vill använda kameran."
        }
    })
    // "en" | "fa" | "de" | "sv" for any language code
    function langOf(code) { return code === "fa" || code === "de" || code === "sv" ? code : "en" }
    function t(key, lang) { return (tx[lang] && tx[lang][key]) || tx.en[key] }
}
