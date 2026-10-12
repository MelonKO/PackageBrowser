#include "app/ui/EnvironmentTreeModel.h"

#include <QColor>
#include <QDir>
#include <QFileInfo>
#include <QFont>

EnvironmentTreeModel::EnvironmentTreeModel(QObject* parent)
    : QAbstractItemModel(parent)
{
    m_nodes.resize(1); // root at index 0
    m_nodes[0] = Node{ -1, 0, false, false, QString(), -1, {} };
}

int EnvironmentTreeModel::addNode(int parent, bool isSection, bool isGroup,
                                  const QString& label, int envIndex)
{
    Node n;
    n.parent = parent;
    n.row = m_nodes[parent].children.size();
    n.isSection = isSection;
    n.isGroup = isGroup;
    n.label = label;
    n.envIndex = envIndex;
    m_nodes.append(n);
    m_nodes[parent].children.append(m_nodes.size() - 1);
    return m_nodes.size() - 1;
}

void EnvironmentTreeModel::setEnvironments(const QVector<Environment>& envs)
{
    beginResetModel();
    m_environments = envs;
    m_nodes.clear();
    m_nodes.resize(1);
    m_nodes[0] = Node{ -1, 0, false, false, QString(), -1, {} };

    const int globalSec = addNode(0, true, false, QStringLiteral("Global"), -1);
    for (int i = 0; i < m_environments.size(); ++i)
        if (m_environments[i].scope() == Scope::System || m_environments[i].scope() == Scope::User)
            addNode(globalSec, false, false, QString(), i);

    int projectsSec = -1;
    for (int i = 0; i < m_environments.size(); ++i) {
        if (m_environments[i].scope() != Scope::Project)
            continue;
        if (projectsSec < 0)
            projectsSec = addNode(0, true, false, QStringLiteral("Projects"), -1);
        const QString root = m_environments[i].root();
        const QString parentDir = QFileInfo(root).dir().dirName();
        const QString label = parentDir.isEmpty() ? QDir::toNativeSeparators(root) : parentDir;

        int groupNode = -1;
        for (const int c : m_nodes[projectsSec].children)
            if (m_nodes[c].isGroup && m_nodes[c].label == label) {
                groupNode = c;
                break;
            }
        if (groupNode < 0)
            groupNode = addNode(projectsSec, false, true, label, -1);
        addNode(groupNode, false, false, QString(), i);
    }

    endResetModel();
}

const Environment* EnvironmentTreeModel::environmentAt(const QModelIndex& index) const
{
    if (!index.isValid())
        return nullptr;
    const quintptr id = index.internalId();
    if (id >= m_nodes.size())
        return nullptr;
    const Node& n = m_nodes[id];
    if (n.envIndex >= 0 && n.envIndex < m_environments.size())
        return &m_environments[n.envIndex];
    return nullptr;
}

QModelIndex EnvironmentTreeModel::index(int row, int column, const QModelIndex& parentIdx) const
{
    if (!hasIndex(row, column, parentIdx))
        return QModelIndex();
    int parentNode = 0;
    if (parentIdx.isValid())
        parentNode = parentIdx.internalId();
    if (parentNode < 0 || parentNode >= m_nodes.size())
        return QModelIndex();
    const Node& pn = m_nodes[parentNode];
    if (row < 0 || row >= pn.children.size())
        return QModelIndex();
    return createIndex(row, column, quintptr(pn.children[row]));
}

QModelIndex EnvironmentTreeModel::parent(const QModelIndex& child) const
{
    if (!child.isValid())
        return QModelIndex();
    const quintptr id = child.internalId();
    if (id >= m_nodes.size())
        return QModelIndex();
    const int parentNode = m_nodes[id].parent;
    if (parentNode <= 0)
        return QModelIndex();
    return createIndex(m_nodes[parentNode].row, 0, quintptr(parentNode));
}

int EnvironmentTreeModel::rowCount(const QModelIndex& parentIdx) const
{
    if (!parentIdx.isValid())
        return m_nodes[0].children.size();
    const quintptr id = parentIdx.internalId();
    if (id >= m_nodes.size())
        return 0;
    return m_nodes[id].children.size();
}

int EnvironmentTreeModel::columnCount(const QModelIndex&) const { return 1; }

QVariant EnvironmentTreeModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return QVariant();
    const quintptr id = index.internalId();
    if (id >= m_nodes.size())
        return QVariant();
    const Node& n = m_nodes[id];

    switch (role) {
    case Qt::DisplayRole:
        if (n.isSection || n.isGroup)
            return n.label;
        if (n.envIndex >= 0) {
            const Environment& env = m_environments[n.envIndex];
            const QString name = QFileInfo(env.root()).fileName();
            const QString scope = env.scope() == Scope::System ? QStringLiteral("System")
                                : env.scope() == Scope::User ? QStringLiteral("User")
                                                              : QStringLiteral("Project");
            return QStringLiteral("%1 (%2)").arg(name, scope);
        }
        return QVariant();
    case Qt::ToolTipRole:
        if (n.envIndex >= 0)
            return QDir::toNativeSeparators(m_environments[n.envIndex].root());
        return QVariant();
    case Qt::FontRole:
        if (n.isSection) {
            QFont f;
            f.setBold(true);
            return f;
        }
        return QVariant();
    case Qt::DecorationRole:
        if (n.envIndex >= 0) {
            switch (m_environments[n.envIndex].status()) {
            case EnvStatus::Error: return QColor(Qt::red);
            case EnvStatus::Missing: return QColor(Qt::darkGray);
            case EnvStatus::Untrusted: return QColor(Qt::darkYellow);
            case EnvStatus::Stale: return QColor(Qt::blue);
            case EnvStatus::Ok: return QColor(Qt::darkGreen);
            }
        }
        return QVariant();
    default:
        return QVariant();
    }
}