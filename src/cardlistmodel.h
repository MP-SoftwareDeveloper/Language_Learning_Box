#pragma once

#include "card.h"

#include <QAbstractListModel>
#include <QtQml/qqmlregistration.h>

// All cards, newest first, with a case-insensitive text filter over front/back/example.
class CardListModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString filter READ filter WRITE setFilter NOTIFY filterChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Role { IdRole = Qt::UserRole + 1, FrontRole, BackRole, ExampleRole, BoxRole, DueAtRole, ImageUrlRole };
    Q_ENUM(Role)

    explicit CardListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString filter() const { return m_filter; }
    void setFilter(const QString &f);
    int count() const { return int(m_rows.size()); }

    Q_INVOKABLE void reload();

signals:
    void filterChanged();
    void countChanged();

private:
    QList<Card> m_rows;
    QString m_filter;
};
