#include "reviewsession.h"
#include "cardstore.h"

ReviewSession::ReviewSession(QObject *parent)
    : QObject(parent)
{}

void ReviewSession::start(int limit)
{
    m_queue = CardStore::instance()->dueCardIds(limit);
    m_failed.clear();
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

    if (m_failed.contains(id)) {
        // Re-ask within the session: already rescheduled to box 1, only drill it.
        if (correct)
            m_failed.remove(id);
        else
            m_queue.append(id);
    } else {
        CardStore::instance()->recordAnswer(id, correct);
        if (correct) {
            ++m_correct;
        } else {
            ++m_wrong;
            m_failed.insert(id);
            m_queue.append(id);
        }
    }
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
        m_failed.remove(id);
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
        m_failed.remove(m_current.id);
        advance();
    }
}

void ReviewSession::moveCurrent(int box)
{
    if (!hasCard())
        return;
    const int id = m_current.id;
    if (box == m_current.box && !m_failed.contains(id))
        return;
    if (!CardStore::instance()->moveCard(id, box))
        return;
    m_failed.remove(id);
    m_queue.removeAll(id);
    advance();
}

QUrl ReviewSession::imageUrl() const
{
    return CardStore::instance()->imageUrl(m_current.image);
}
