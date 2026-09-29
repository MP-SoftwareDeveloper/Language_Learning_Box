#pragma once

#include "wordpack.h"
#include "starterdeck.h"

#include <QObject>
#include <QHash>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

// Built-in vocabulary packs (currently: Netzwerk neu A1 chapter themes).
// Exposed to QML as a singleton; chapters are added to the box on request.
class WordPacks : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QString title READ title CONSTANT)
    Q_PROPERTY(QVariantList chapters READ chapters NOTIFY chaptersChanged)
    Q_PROPERTY(int totalWords READ totalWords CONSTANT)
    Q_PROPERTY(int wordsInBox READ wordsInBox NOTIFY chaptersChanged)
    Q_PROPERTY(QString error READ error CONSTANT)
    // Starter cards (100 common words, de / en / fa)
    Q_PROPERTY(int starterTotal READ starterTotal CONSTANT)
    Q_PROPERTY(int starterInBox READ starterInBox NOTIFY chaptersChanged)

public:
    static WordPacks *create(QQmlEngine *, QJSEngine *);

    QString title() const { return m_pack.title; }
    QVariantList chapters() const { return m_chapters; }
    int totalWords() const;
    int wordsInBox() const { return m_wordsInBox; }
    QString error() const { return m_error; }
    int starterTotal() const { return int(m_starter.size()); }
    int starterInBox() const { return m_starterInBox; }

    // Adds the starter cards that are not already in the current learning box, with the meaning
    // in the chosen language (Settings), and saves the English and Persian meanings and example
    // translations so the fa / en switch works offline. Returns the count added.
    Q_INVOKABLE int addStarterCards();

    // Adds the chapter's words that are not already in the box. Returns the count added.
    Q_INVOKABLE int addChapter(int number);
    Q_INVOKABLE int addAll();
    // Cards of one chapter for preview: [{front, back, example, inBox}]
    Q_INVOKABLE QVariantList chapterWords(int number) const;

    static QString deckName(const WordPack &pack, int chapter);

    // Finds a pack entry for a word seen in a text (e.g. from the camera):
    // "Apfel" / "apfel" / "der Apfel" -> {front: "der Apfel", back, example}. Empty map if unknown.
    Q_INVOKABLE QVariantMap lookup(const QString &word) const;
    // Adds several words at once (e.g. selected in a photo). Known words get the pack's
    // article, meaning and example; unknown words get an empty meaning. Skips duplicates.
    Q_INVOKABLE int addWords(const QStringList &words, const QString &deck);
    // Same, with a meaning per word (e.g. from the translator): items = [{word, back}].
    // Known words keep the pack's article and example; a non-empty `back` replaces the pack meaning.
    Q_INVOKABLE int addTranslatedWords(const QVariantList &items, const QString &deck);
    // Persian translation of a word-pack example sentence ("" if the sentence is not from a pack).
    // Works for cards already in the box too, since their example is the pack's text.
    Q_INVOKABLE QString exampleTranslation(const QString &example) const;
    // Pack words starting with `prefix` (article ignored; "bro" -> "das Brot"), best first:
    // [{front, back, example}]. For suggestions while typing a new card.
    Q_INVOKABLE QVariantList suggest(const QString &prefix, int max = 5) const;
    // Pack example sentences that contain `word` (offline suggestions): [{text, translation}]
    Q_INVOKABLE QVariantList examplesContaining(const QString &word, int max = 3) const;

signals:
    void chaptersChanged();

private:
    explicit WordPacks(QObject *parent = nullptr);
    void refresh();

    const Card *find(const QString &word) const;

    WordPack m_pack;
    QHash<QString, const Card *> m_index; // case-folded front, and front without article
    QHash<QString, QString> m_exampleFa;  // simplified, case-folded example -> Persian
    QList<StarterCard> m_starter;
    int m_starterInBox = 0;
    QVariantList m_chapters;
    int m_wordsInBox = 0;
    QString m_error;
};
