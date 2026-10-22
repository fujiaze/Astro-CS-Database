#include "product.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonValue>

namespace acsd {
namespace gui {
namespace {

QString stageToString(const QJsonValue& value) {
  if (value.isString()) {
    return value.toString();
  }
  if (value.isDouble()) {
    const int index = value.toInt(-1);
    switch (index) {
      case 0: return QStringLiteral("normalize");
      case 1: return QStringLiteral("mosaic");
      case 2: return QStringLiteral("export_image");
      default: break;
    }
  }
  return QString();
}

QStringList stringListOf(const QJsonValue& value) {
  QStringList result;
  if (value.isArray()) {
    for (const QJsonValue& item : value.toArray()) {
      if (item.isString()) {
        result.append(item.toString());
      } else if (item.isDouble()) {
        result.append(QString::number(item.toDouble()));
      }
    }
  } else if (value.isString()) {
    result.append(value.toString());
  }
  return result;
}

}  // namespace

ProductManifestView readProductManifest(const QString& directory) {
  ProductManifestView view;
  view.directory = directory;

  const QDir dir(directory);
  if (!dir.exists()) {
    view.error = QStringLiteral("产品目录不存在：%1").arg(directory);
    return view;
  }
  const QString manifestPath = dir.filePath(QStringLiteral("manifest.json"));
  QFile file(manifestPath);
  if (!file.exists()) {
    view.error = QStringLiteral("产品目录缺少 manifest.json：%1").arg(manifestPath);
    return view;
  }
  if (!file.open(QIODevice::ReadOnly)) {
    view.error = QStringLiteral("无法读取 %1：%2").arg(manifestPath, file.errorString());
    return view;
  }
  const QByteArray payload = file.readAll();
  file.close();

  QJsonParseError perr{};
  const QJsonDocument document = QJsonDocument::fromJson(payload, &perr);
  if (perr.error != QJsonParseError::NoError || !document.isObject()) {
    view.error = QStringLiteral("manifest.json 不是合法 JSON 对象：%1").arg(perr.errorString());
    return view;
  }
  const QJsonObject object = document.object();
  view.raw = object;

  view.format = object.value(QStringLiteral("format")).toString();
  view.major = object.value(QStringLiteral("major")).toInt(0);
  view.minor = object.value(QStringLiteral("minor")).toInt(0);
  view.stage = stageToString(object.value(QStringLiteral("stage")));
  view.productId = object.value(QStringLiteral("product_id")).toString();
  view.revision = static_cast<qint64>(object.value(QStringLiteral("revision")).toDouble(0));
  view.rootObject = object.value(QStringLiteral("root_object")).toString();
  view.profile = object.value(QStringLiteral("profile")).toString();
  if (object.contains(QStringLiteral("complete"))) {
    view.hasComplete = true;
    view.complete = object.value(QStringLiteral("complete")).toBool(false);
  }
  view.capabilities = stringListOf(object.value(QStringLiteral("capabilities")));
  view.inputs = stringListOf(object.value(QStringLiteral("inputs")));
  view.configurationHash = object.value(QStringLiteral("configuration_hash")).toString();
  if (object.value(QStringLiteral("assessment")).isObject()) {
    view.assessment = object.value(QStringLiteral("assessment")).toObject();
  }

  const QJsonValue objectsValue = object.value(QStringLiteral("objects"));
  if (objectsValue.isArray()) {
    for (const QJsonValue& item : objectsValue.toArray()) {
      if (!item.isObject()) {
        continue;
      }
      const QJsonObject entry = item.toObject();
      ProductObjectEntry objectEntry;
      objectEntry.hash = entry.value(QStringLiteral("hash")).toString();
      objectEntry.path = entry.value(QStringLiteral("path")).toString();
      objectEntry.type = entry.value(QStringLiteral("type")).toString();
      objectEntry.major = entry.value(QStringLiteral("major")).toInt(0);
      objectEntry.minor = entry.value(QStringLiteral("minor")).toInt(0);
      view.objects.append(objectEntry);
    }
  }

  const QJsonValue blocksValue = object.value(QStringLiteral("blocks"));
  if (blocksValue.isArray()) {
    for (const QJsonValue& item : blocksValue.toArray()) {
      if (!item.isObject()) {
        continue;
      }
      const QJsonObject entry = item.toObject();
      ProductBlockEntry block;
      block.id = entry.value(QStringLiteral("id")).toString();
      block.path = entry.value(QStringLiteral("path")).toString();
      block.dtype = entry.value(QStringLiteral("dtype")).toString();
      block.indexId = entry.value(QStringLiteral("index_id")).toString();
      block.codec = entry.value(QStringLiteral("codec")).toString();
      block.begin = static_cast<qint64>(entry.value(QStringLiteral("begin")).toDouble(0));
      block.count = static_cast<qint64>(entry.value(QStringLiteral("count")).toDouble(0));
      block.rawSha256 = entry.value(QStringLiteral("raw_sha256")).toString();
      if (entry.value(QStringLiteral("shape")).isArray()) {
        for (const QJsonValue& dim : entry.value(QStringLiteral("shape")).toArray()) {
          block.shape.append(static_cast<qint64>(dim.toDouble(0)));
        }
      }
      view.blocks.append(block);
    }
  }

  const QJsonValue algorithmsValue = object.value(QStringLiteral("algorithm_set"));
  if (algorithmsValue.isArray()) {
    for (const QJsonValue& item : algorithmsValue.toArray()) {
      if (!item.isObject()) {
        continue;
      }
      const QJsonObject entry = item.toObject();
      ProductAlgorithmEntry algorithm;
      algorithm.name = entry.value(QStringLiteral("name")).toString();
      algorithm.version = entry.value(QStringLiteral("version")).toString();
      algorithm.sourceHash = entry.value(QStringLiteral("source_hash")).toString();
      view.algorithms.append(algorithm);
    }
  }

  view.ok = true;
  return view;
}

}  // namespace gui
}  // namespace acsd
