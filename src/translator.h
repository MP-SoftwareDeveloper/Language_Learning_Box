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
    Q_PROPERTY(bool rightToLeft READ rightToLeft NOTIFY settingsChanged)  // target is Persian
    Q_PROPERTY(int savedCount READ savedCount NOTIFY savedCountChanged)

public:
    static Translator *create(QQmlEngine *, QJSEngine *);
    static Translator *instance();

    QString targetLanguage() const { return m_target; }
    void setTargetLanguage(const QString &lang);
    bool onlineEnabled() const { return m_onlineEnabled; }
    void setOnlineEnabled(bool on);
    bool networkAvailable() const;
    // The setting alone decides: Android's reachability report is unreliable (it can say
    // "disconnected" while online), and a failed request falls back to saved translations anyway.
    // Online on in Settings and the app in Full mode (Simple mode stays offline).
    bool useOnline() const;
    bool rightToLeft() const { return m_target == QLatin1String("fa"); }
    int savedCount() const;

    // Starts a translation of German `text` into the target language. Returns a request id;
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
    void onReply(QNetworkReply *reply, int id, const QString &text, const QString &target);
    void answerOffline(int id, const QString &text, const QString &error);
    bool openCache();
    void store(const QString &target, const QString &text, const QString &translation,
               const QStringList &alternatives);
    bool lookup(const QString &target, const QString &text, QString *translation,
                QStringList *alternatives) const;

    QNetworkAccessManager *m_nam = nullptr;
    QString m_target;
    bool m_onlineEnabled = true;
    bool m_cacheOk = false;
    int m_nextId = 1;
};
