#include "reviewsession.h"
#include "cardstore.h"

ReviewSession::ReviewSession(QObject *parent)
    : QObject(parent)
{}

void ReviewSession::start(int limit)
{
    m_queue = CardStore::instance()->dueCardIds(limit);
    m_total = int(m_queue.size());
    m_correct = m_wrong = 0;
    m_started = true;
    advance();
}

void ReviewSession::answer(bool correct)
{
    if (!hasCard())
        return;
    const int id = m_current.id;

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
    m_queue.removeAll(id);
    advance();
}

QUrl ReviewSession::imageUrl() const
{
    return CardStore::instance()->imageUrl(m_current.image);
}
