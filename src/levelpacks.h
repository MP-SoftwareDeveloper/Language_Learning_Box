#pragma once

#include "levelpack.h"

#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

// Built-in level packs (German A1 and A2, three levels each, themed chapters; meanings in
// English and Persian). Exposed to QML as a singleton; levels or single chapters are added to
// the current learning box on request. German learning boxes only.
class LevelPacks : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // [{id, title, subtitle, cefr, total, inBox (cards in its own box), learned, due, boxId (-1 = none),
    //   chapters: [{number, title, total, inBox (in the current box)}]}]
    Q_PROPERTY(QVariantList levels READ levels NOTIFY changed)
    Q_PROPERTY(int totalWords READ totalWords CONSTANT)
    Q_PROPERTY(int wordsInBox READ wordsInBox NOTIFY changed)
    Q_PROPERTY(bool available READ available NOTIFY changed) // the current learning box is German
    Q_PROPERTY(QString error READ error CONSTANT)

public:
    static LevelPacks *create(QQmlEngine *, QJSEngine *);

    QVariantList levels() const { return m_levels; }
    int totalWords() const;
    int wordsInBox() const { return m_wordsInBox; }
    bool available() const;
    QString error() const { return m_error; }

    // Adds the words that are not already in the current learning box, with the meaning in the
    // chosen language (Settings), and saves the English and Persian meanings and example
    // translations so the fa / en switch works offline. Return the count added.
    Q_INVOKABLE int addChapter(const QString &levelId, int number);
    Q_INVOKABLE int addLevel(const QString &levelId);
    // Every level is its own learning box (named like the level, e.g. "A1 · Level 1") that shows up
    // in the learning-box list. installBoxes creates the missing ones, filled with all words; with
    // force=false it does so only once (first start). The current learning box stays selected.
    // Returns how many boxes were created.
    Q_INVOKABLE int installBoxes(bool force);
    // Selects the level's learning box (creating it if it was deleted). false on error.
    Q_INVOKABLE bool openLevel(const QString &levelId);
    // Words of one chapter for a preview: [{front, back, example, exampleFa, exampleEn, inBox}]
    Q_INVOKABLE QVariantList chapterWords(const QString &levelId, int number) const;

signals:
    void changed();

private:
    explicit LevelPacks(QObject *parent = nullptr);
    void refresh();
    int addWords(const LevelPack &pack, const LevelChapter &chapter);
    int boxId(const LevelPack &pack) const; // -1 = no learning box with the level's name
    int createBox(const LevelPack &pack);   // creates, selects and fills it; -1 on error

    QList<LevelPack> m_packs;
    QVariantList m_levels;
    int m_wordsInBox = 0;
    QString m_error;
};
