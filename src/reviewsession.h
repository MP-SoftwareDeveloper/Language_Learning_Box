#pragma once

#include "card.h"

#include <QObject>
#include <QSet>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

// One pass over the due cards. A card answered wrong is written back to box 1
// and re-queued at the end of this session; re-asks don't touch the database.
class ReviewSession : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(bool hasCard READ hasCard NOTIFY currentChanged)
    Q_PROPERTY(bool finished READ finished NOTIFY currentChanged)
    Q_PROPERTY(int cardId READ cardId NOTIFY currentChanged)
    Q_PROPERTY(QString front READ front NOTIFY currentChanged)
    Q_PROPERTY(QString back READ back NOTIFY currentChanged)
    Q_PROPERTY(QString example READ example NOTIFY currentChanged)
    Q_PROPERTY(QUrl imageUrl READ imageUrl NOTIFY currentChanged)
    Q_PROPERTY(int box READ box NOTIFY currentChanged)
    Q_PROPERTY(bool isRetry READ isRetry NOTIFY currentChanged)
    Q_PROPERTY(int remaining READ remaining NOTIFY currentChanged)
    Q_PROPERTY(int total READ total NOTIFY currentChanged)
    Q_PROPERTY(int correctCount READ correctCount NOTIFY currentChanged)
    Q_PROPERTY(int wrongCount READ wrongCount NOTIFY currentChanged)

public:
    explicit ReviewSession(QObject *parent = nullptr);

    Q_INVOKABLE void start(int limit = 0);
    Q_INVOKABLE void answer(bool correct);
    // Re-read the current card after it was edited (or skip it if it was deleted).
    Q_INVOKABLE void reloadCurrent();
    // Put the current card into `box` by hand (1..5, 6 = Learned; scheduled with that box's
    // interval) and continue with the next card. Not counted as known or wrong.
    Q_INVOKABLE void moveCurrent(int box);

    bool hasCard() const { return m_current.id >= 0; }
    bool finished() const { return m_started && !hasCard(); }
    int cardId() const { return m_current.id; }
    QString front() const { return m_current.front; }
    QString back() const { return m_current.back; }
    QString example() const { return m_current.example; }
    QUrl imageUrl() const;
    int box() const { return m_current.box; }
    bool isRetry() const { return m_failed.contains(m_current.id); }
    int remaining() const { return int(m_queue.size()) + (hasCard() ? 1 : 0); }
    int total() const { return m_total; }
    int correctCount() const { return m_correct; }
    int wrongCount() const { return m_wrong; }

signals:
    void currentChanged();

private:
    void advance();

    QList<int> m_queue;
    QSet<int> m_failed;
    Card m_current;
    bool m_started = false;
    int m_total = 0;
    int m_correct = 0;
    int m_wrong = 0;
};
