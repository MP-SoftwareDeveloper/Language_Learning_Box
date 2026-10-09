#pragma once

#include <QHash>
#include <functional>
#include "translation/wiktionary.h"
#include <QObject>
#include <QSet>
#include <QStringList>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class QNetworkAccessManager;
class QNetworkRequest;
class QNetworkReply;
class QUrl;
class QQmlEngine;
class QJSEngine;

// German -> Persian/English translation for Lens and the card editor.
//
//  * Online (setting on + network reachable): Azure Translator when the user entered their own key in
//    Settings (optional), otherwise - or when Azure refuses - Google's public translate endpoint, then MyMemory.
//    Every result is saved, so the same word/sentence also works offline later.
//  * Offline: saved translations only (QML adds the word pack's Persian meanings on top).
//
// Settings (QSettings, "translation/*"): target language ("fa" or "en") and whether to go online.
class Translator : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QString targetLanguage READ targetLanguage WRITE setTargetLanguage NOTIFY settingsChanged)
    Q_PROPERTY(bool onlineEnabled READ onlineEnabled WRITE setOnlineEnabled NOTIFY settingsChanged)
    Q_PROPERTY(bool networkAvailable READ networkAvailable NOTIFY networkChanged)
    Q_PROPERTY(bool useOnline READ useOnline NOTIFY networkChanged)       // online translation is tried
    Q_PROPERTY(bool rightToLeft READ rightToLeft NOTIFY settingsChanged)  // meaning is Persian
    // Language being learned (from the selected learning box): "de" or "en".
    Q_PROPERTY(QString sourceLanguage READ sourceLanguage NOTIFY settingsChanged)
    // Language of the meanings in the selected learning box: Persian, English or German, never
    // the learning language (CardStore::meaningLanguage). targetLanguage is the same, writable.
    Q_PROPERTY(QString meaningLanguage READ meaningLanguage NOTIFY settingsChanged)
    Q_PROPERTY(int savedCount READ savedCount NOTIFY savedCountChanged)
    // Optional Azure Translator (own free key, 2 million characters a month). Empty key = not used.
    Q_PROPERTY(QString azureKey READ azureKey WRITE setAzureKey NOTIFY azureChanged)
    Q_PROPERTY(QString azureRegion READ azureRegion WRITE setAzureRegion NOTIFY azureChanged)
    Q_PROPERTY(bool azureConfigured READ azureConfigured NOTIFY azureChanged)
    Q_PROPERTY(bool azureTesting READ azureTesting NOTIFY azureChanged)

public:
    static Translator *create(QQmlEngine *, QJSEngine *);
    static Translator *instance();

    // Meaning language of the selected learning box ("fa", "en", "de"); same as meaningLanguage.
    QString targetLanguage() const;
    void setTargetLanguage(const QString &lang);
    bool onlineEnabled() const { return m_onlineEnabled; }
    void setOnlineEnabled(bool on);
    bool networkAvailable() const;
    // The setting alone decides: Android's reachability report is unreliable (it can say
    // "disconnected" while online), and a failed request falls back to saved translations anyway.
    // Online on in Settings and the app in Full mode (Simple mode stays offline).
    bool useOnline() const;
    bool rightToLeft() const { return meaningLanguage() == QLatin1String("fa"); }
    QString sourceLanguage() const;
    QString meaningLanguage() const;
    int savedCount() const;
    QString azureKey() const { return m_azureKey; }
    void setAzureKey(const QString &key);
    QString azureRegion() const { return m_azureRegion; }
    void setAzureRegion(const QString &region);
    bool azureConfigured() const { return !m_azureKey.isEmpty(); }
    bool azureTesting() const { return m_azureTesting; }
    // Translates a test word with the entered key; answers through azureTested(ok, message).
    Q_INVOKABLE void testAzure();

    // Starts a translation of `text` (in the learning language) into the meaning language. Returns a request id;
    // the answer arrives through translated(id, ...). Offline misses answer with empty text.
    Q_INVOKABLE int translate(const QString &text);
    // Same between any two languages ("de", "en", "fa"), e.g. the dictionary in both directions.
    Q_INVOKABLE int translateBetween(const QString &text, const QString &source, const QString &target);
    // Saved translation (any source) or an empty string; synchronous.
    Q_INVOKABLE QString saved(const QString &text) const;
    Q_INVOKABLE QString savedBetween(const QString &text, const QString &source, const QString &target) const;
    Q_INVOKABLE void clearSaved();
    // Saves a known translation (e.g. from Tatoeba) so saved() / offline use find it.
    Q_INVOKABLE void remember(const QString &text, const QString &translation);
    // Same for a given language ("fa" / "en"), e.g. the starter cards save both languages.
    void rememberIn(const QString &language, const QString &text, const QString &translation);
    // Example sentences with `word` from Tatoeba (online only). Answers through
    // examplesSuggested(id, [{text, translation}], error); translations are also saved.
    Q_INVOKABLE int suggestExamples(const QString &word);
    // Gender (der / die / das) and plural of a German noun from the German Wiktionary (online only; every
    // answer is saved, so the word is found offline later). Answers through
    // grammarFound(id, {front, lemma, plural, pluralLine, forms}, error); the map is empty when the
    // word is not a (single-word) noun or nothing is known.
    Q_INVOKABLE int lookupGrammar(const QString &word);

    // Word suggestions while typing, like a search box: Wiktionary's prefix search in `language` ("de", "en",
    // "fa"; online only). Answers through wordsSuggested(id, words, error).
    Q_INVOKABLE int suggestWords(const QString &prefix, const QString &language);

signals:
    void wordsSuggested(int requestId, const QStringList &words, const QString &error);
    void grammarFound(int requestId, const QVariantMap &grammar, const QString &error);
    void examplesSuggested(int requestId, const QVariantList &examples, const QString &error);
    void settingsChanged();
    void networkChanged();
    void savedCountChanged();
    void azureChanged();
    void azureTested(bool ok, const QString &message);
    // source: "online", "saved" or "" (nothing found). error: why online failed, if it did.
    void translated(int requestId, const QString &text, const QStringList &alternatives,
                    const QString &source, const QString &error);

private:
    void fetchGrammar(const QString &word, bool followSingular, const QString &article,
                      std::function<void(const wiktionary::Grammar &, const QString &)> finished);
    // Wiktionary has no usable entry for a plural noun ("Nudeln"): try the likely singulars one after the other
    // (wiktionary::singularCandidates) and take the first noun whose plural is the word. `original` is the answer
    // when none fits.
    void guessSingular(const QString &plural, const wiktionary::Grammar &original, QStringList candidates,
                       std::function<void(const wiktionary::Grammar &, const QString &)> finished);
    explicit Translator(QObject *parent = nullptr);
    // Google, then MyMemory (the way it always was); Azure comes first when a key is set.
    void requestFree(int id, const QString &text, const QString &source, const QString &target);
    void requestAzure(int id, const QString &text, const QString &source, const QString &target);
    // Other translations of a single word (Azure dictionary) before the answer goes out.
    void azureAlternatives(int id, const QString &text, const QString &source, const QString &target,
                           const QString &main);
    QNetworkRequest azureRequest(const QUrl &url) const;
    void requestGoogle(int id, const QString &text, const QString &source, const QString &target);
    void onReply(QNetworkReply *reply, int id, const QString &text, const QString &source, const QString &target);
    // Fallback when Google refuses (429 etc.): MyMemory, free and without a key.
    void requestMyMemory(int id, const QString &text, const QString &source, const QString &target,
                         const QString &googleError);
    static QString errorText(QNetworkReply *reply); // short, readable ("Google limit reached (429)")
    void answerOffline(int id, const QString &text, const QString &source, const QString &target,
                       const QString &error);
    bool openCache();
    // Cache rows are per meaning language and source text; English source texts are stored with an
    // "en|" prefix so they never mix with German ones (German rows keep their old keys).
    void store(const QString &source, const QString &target, const QString &text, const QString &translation,
               const QStringList &alternatives);
    bool lookup(const QString &source, const QString &target, const QString &text, QString *translation,
                QStringList *alternatives) const;

    QNetworkAccessManager *m_nam = nullptr;
    QString m_lastSource;
    bool m_onlineEnabled = true;
    bool m_cacheOk = false;
    qint64 m_googleBlockedUntil = 0; // ms since epoch; after a 429 Google is skipped for a while
    qint64 m_azureBlockedUntil = 0;  // same for Azure (wrong key, monthly quota used up)
    QString m_azureKey;
    QString m_azureRegion;
    bool m_azureTesting = false;
    QSet<QString> m_azureNoDictionary; // "de>fa" pairs the Azure dictionary does not know
    int m_nextId = 1;
};
