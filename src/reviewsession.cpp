#include "reviewsession.h"
#include "cardstore.h"

#include <QRandomGenerator>
#include <algorithm>

ReviewSession::ReviewSession(QObject *parent)
    : QObject(parent)
{}

void ReviewSession::start(int limit)
{
    m_queue = CardStore::instance()->dueCardIds(limit);
    // Random order, so boxes that were filled A to Z (e.g. A1_complete) are not asked alphabetically.
    std::shuffle(m_queue.begin(), m_queue.end(), *QRandomGenerator::global());
    m_total = int(m_queue.size());
    m_correct = m_wrong = 0;
    m_history.clear();
    m_started = true;
    advance();
}

void ReviewSession::answer(bool correct)
{
    if (!hasCard())
        return;
    const int id = m_current.id;
    m_history.append({m_current, correct ? 1 : 0});

    // Leitner: a wrong answer sends the card back to box 1 (due tomorrow); it is not asked again today.
    CardStore::instance()->recordAnswer(id, correct);
    if (correct)
        ++m_correct;
    else
        ++m_wrong;
    advance();
}

void ReviewSession::advance()
{
    m_current = Card{};
    while (!m_queue.isEmpty()) {
        const int id = m_queue.takeFirst();
        if (auto c = CardStore::instance()->cardById(id)) { // skip cards deleted meanwhile
            m_current = *c;
            break;
        }
    }
    emit currentChanged();
}

void ReviewSession::reloadCurrent()
{
    if (!hasCard())
        return;
    if (auto c = CardStore::instance()->cardById(m_current.id)) {
        m_current = *c;
        emit currentChanged();
    } else {
        advance();
    }
}

void ReviewSession::moveCurrent(int box)
{
    if (!hasCard())
        return;
    const int id = m_current.id;
    if (box == m_current.box)
        return;
    if (!CardStore::instance()->moveCard(id, box))
        return;
    m_history.append({m_current, -1});
    m_queue.removeAll(id);
    advance();
}

void ReviewSession::goBack()
{
    CardStore *store = CardStore::instance();
    while (!m_history.isEmpty()) {
        const Step s = m_history.takeLast();
        if (!store->restoreProgress(s.before.id, s.before.box, s.before.dueAt, s.before.reviews, s.before.lapses))
            continue; // deleted meanwhile: try the one before
        const auto c = store->cardById(s.before.id);
        if (!c)
            continue;
        if (s.kind == 1)
            m_correct = qMax(0, m_correct - 1);
        else if (s.kind == 0)
            m_wrong = qMax(0, m_wrong - 1);
        if (hasCard())
            m_queue.prepend(m_current.id); // the card on screen comes again right after
        m_current = *c;
        emit currentChanged();
        return;
    }
}

QUrl ReviewSession::imageUrl() const
{
    return CardStore::instance()->imageUrl(m_current.image);
}
