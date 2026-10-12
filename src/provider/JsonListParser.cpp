#include "provider/JsonListParser.h"

#include "normalization/NameNormalizer.h"

#include <QJsonArray>
#include <QJsonObject>

namespace pb {

QJsonValue jsonAtPath(const QJsonValue& root, const QString& path)
{
    if (path.isEmpty())
        return root;

    QJsonValue current = root;
    const QStringList segments = path.split(QLatin1Char('.'), Qt::SkipEmptyParts);
    for (const QString& segment : segments) {
        if (!current.isObject())
            return QJsonValue(QJsonValue::Undefined);
        current = current.toObject().value(segment);
        if (current.isUndefined())
            return current;
    }
    return current;
}

bool extractInstallations(const QJsonValue& container,
                          const QString& nameKey,
                          const QString& versionKey,
                          Ecosystem ecosystem,
                          const EnvironmentId& envId,
                          QVector<Installation>& out)
{
    if (container.isArray()) {
        const QJsonArray arr = container.toArray();
        out.reserve(out.size() + arr.size());
        for (const QJsonValue& element : arr) {
            const QJsonObject obj = element.toObject();
            const QString name = obj.value(nameKey).toString();
            if (name.isEmpty())
                continue;
            out.append(Installation(envId, normalizeName(ecosystem, name), name,
                                    obj.value(versionKey).toString()));
        }
        return true;
    }

    if (container.isObject()) {
        const QJsonObject obj = container.toObject();
        out.reserve(out.size() + obj.size());
        for (auto it = obj.begin(); it != obj.end(); ++it) {
            const QJsonObject child = it.value().toObject();
            QString name = child.value(nameKey).toString();
            if (name.isEmpty())
                name = it.key(); // the object is keyed by package name
            if (name.isEmpty())
                continue;
            out.append(Installation(envId, normalizeName(ecosystem, name), name,
                                    child.value(versionKey).toString()));
        }
        return true;
    }

    return false;
}

} // namespace pb
