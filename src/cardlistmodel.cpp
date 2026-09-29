#include "cardlistmodel.h"
#include "cardstore.h"

CardListModel::CardListModel(QObject *parent)
    : QAbstractListModel(parent)
{
    connect(CardStore::instance(), &CardStore::changed, this, &CardListModel::reload);
    reload();
}

int CardListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_rows.size());
}

QVariant CardListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_rows.size())
        return {};
    const Card &c = m_rows.at(index.row());
    switch (role) {
    case IdRole: return c.id;
    case Qt::DisplayRole:
    case FrontRole: return c.front;
    case BackRole: return c.back;
    case ExampleRole: return c.example;
    case BoxRole: return c.box;
    case DueAtRole: return c.dueAt;
    case ImageUrlRole: return CardStore::instance()->imageUrl(c.image);
    default: return {};
    }
}

QHash<int, QByteArray> CardListModel::roleNames() const
{
    return {
        {IdRole, "cardId"},
        {FrontRole, "front"},
        {BackRole, "back"},
        {ExampleRole, "example"},
        {BoxRole, "box"},
        {DueAtRole, "dueAt"},
        {ImageUrlRole, "imageUrl"},
    };
}

void CardListModel::setFilter(const QString &f)
{
    if (f == m_filter)
        return;
    m_filter = f;
    emit filterChanged();
    reload();
}

void CardListModel::reload()
{
    const QString needle = m_filter.trimmed();
    QList<Card> rows = CardStore::instance()->allCards();
    if (!needle.isEmpty()) {
        rows.removeIf([&](const Card &c) {
            return !c.front.contains(needle, Qt::CaseInsensitive)
                && !c.back.contains(needle, Qt::CaseInsensitive)
                && !c.example.contains(needle, Qt::CaseInsensitive)
                && !c.deck.contains(needle, Qt::CaseInsensitive);
        });
    }
    const int oldCount = count();
    beginResetModel();
    m_rows = std::move(rows);
    endResetModel();
    if (oldCount != count())
        emit countChanged();
}
