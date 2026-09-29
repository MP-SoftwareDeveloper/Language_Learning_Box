#pragma once

#include <QHash>
#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class QNetworkAccessManager;
class QNetworkReply;
class QQmlEngine;
class QJSEngine;

// German -> Persian/English translation for Lens and the card editor.
//
//  * Online (setting on + network reachable): Google's public translate endpoint.
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

    // Starts a translation of `text` (in the learning language) into the meaning language. Returns a request id;
    // the answer arrives through translated(id, ...). Offline misses answer with empty text.
    Q_INVOKABLE int translate(const QString &text);
    // Saved translation (any source) or an empty string; synchronous.
    Q_INVOKABLE QString saved(const QString &text) const;
    Q_INVOKABLE void clearSaved();
    // Saves a known translation (e.g. from Tatoeba) so saved() / offline use find it.
    Q_INVOKABLE void remember(const QString &text, const QString &translation);
    // Same for a given language ("fa" / "en"), e.g. the starter cards save both languages.
    void rememberIn(const QString &language, const QString &text, const QString &translation);
    // Example sentences with `word` from Tatoeba (online only). Answers through
    // examplesSuggested(id, [{text, translation}], error); translations are also saved.
    Q_INVOKABLE int suggestExamples(const QString &word);

signals:
    void examplesSuggested(int requestId, const QVariantList &examples, const QString &error);
    void settingsChanged();
    void networkChanged();
    void savedCountChanged();
    // source: "online", "saved" or "" (nothing found). error: why online failed, if it did.
    void translated(int requestId, const QString &text, const QStringList &alternatives,
                    const QString &source, const QString &error);

private:
    explicit Translator(QObject *parent = nullptr);
    void onReply(QNetworkReply *reply, int id, const QString &text, const QString &source, const QString &target);
    void answerOffline(int id, const QString &text, const QString &error);
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
    int m_nextId = 1;
};
