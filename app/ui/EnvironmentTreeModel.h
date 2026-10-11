#pragma once

#include "domain/Environment.h"
#include "domain/EnvironmentId.h"

#include <QAbstractItemModel>
#include <QVector>

// Tree model of environments: root -> section ("Global" / "Projects") ->
// entries. Global enlists system/user environments; Projects groups project
// environments by the parent directory of their root.
// Environment status is exposed as Qt::DecorationRole.
// See docs/UI на Qt.md.
class EnvironmentTreeModel : public QAbstractItemModel {
    Q_OBJECT

public:
    explicit EnvironmentTreeModel(QObject* parent = nullptr);

    void setEnvironments(const QVector<Environment>& envs);

    const Environment* environmentAt(const QModelIndex& index) const;

    QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;

private:
    struct Node {
        int parent = -1;
        int row = 0;
        bool isSection = false;
        bool isGroup = false;
        QString label;
        int envIndex = -1;
        QVector<int> children;
    };

    enum class Section { Global, Projects };
    int addNode(int parent, bool isSection, bool isGroup, const QString& label, int envIndex);

    QVector<Environment> m_environments;
    QVector<Node> m_nodes;
};