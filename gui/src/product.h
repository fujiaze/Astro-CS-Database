// ACSD v2 GUI —— 产品浏览（只读产品目录里的 manifest.json）
//
// 规范来源：include/acsd/contracts.hpp 的 ProductManifest / ObjectEntry /
//           BlockDescriptor / AlgorithmIdentity。GUI 只读目录，不执行产品携带的任何内容。
#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

namespace acsd {
namespace gui {

struct ProductObjectEntry {
  QString hash;
  QString path;
  QString type;
  int major = 0;
  int minor = 0;
};

struct ProductBlockEntry {
  QString id;
  QString path;
  QString dtype;
  QVector<qint64> shape;
  QString indexId;
  QString codec;
  qint64 begin = 0;
  qint64 count = 0;
  QString rawSha256;
};

struct ProductAlgorithmEntry {
  QString name;
  QString version;
  QString sourceHash;
};

struct ProductManifestView {
  bool ok = false;
  QString error;
  QString directory;
  QString format;
  int major = 0;
  int minor = 0;
  QString stage;
  QString productId;
  qint64 revision = 0;
  QString rootObject;
  QString profile;
  bool complete = false;
  bool hasComplete = false;
  QStringList capabilities;
  QStringList inputs;
  QVector<ProductObjectEntry> objects;
  QVector<ProductBlockEntry> blocks;
  QVector<ProductAlgorithmEntry> algorithms;
  QString configurationHash;
  QJsonObject assessment;
  QJsonObject raw;
};

// 读取 <directory>/manifest.json。找不到或不是合法 JSON 时 ok=false 并给出原因。
ProductManifestView readProductManifest(const QString& directory);

}  // namespace gui
}  // namespace acsd
