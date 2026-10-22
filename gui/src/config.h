// ACSD v2 GUI —— 配置文档（acsd.config/2）
//
// 规范来源：schemas/config.schema.json、API.md「三命令配置遵循 config.schema.json」。
// GUI 只做结构编辑与本地校验；科学合法性由 CLI 判定。
#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

namespace acsd {
namespace gui {

struct ConfigInput {
  QString path;
  QString expectedHash;  // 空 = 不写该键
  QString frameId;       // 空 = 不写该键
  QString calibration;   // 空 = 不写该键
  QString response;      // 空 = 不写该键
};

struct ConfigResources {
  qint64 bytes = 1073741824;      // 1 GiB
  qint64 work = 10000000;
  qint64 spillBytes = 8589934592; // 8 GiB
  int threads = 8;
  int maxDepth = 64;
  int maxPrecisionBits = 64;
  QString scratchDirectory;
};

struct PublicationSettings {
  bool present = false;
  QString signalDtype = QStringLiteral("f64");
  int tileLog2Width = 9;
  bool durable = true;
};

struct AcsdConfig {
  QString schema = QStringLiteral("acsd.config/2");
  QString command = QStringLiteral("normalize");
  QVector<ConfigInput> inputs;
  QString scientific;
  QString output;
  QString catalogue;  // 可选（normalize 需要）
  ConfigResources resources;
  QString productProfile = QStringLiteral("full");
  PublicationSettings publication;

  QJsonObject toJson() const;
  static bool fromJson(const QJsonObject& object, AcsdConfig* out, QString* error);
};

// 本地校验结果：Error 会阻止运行，Warning 只提示（CLI 才是权威）。
struct ConfigDiagnostic {
  QString level;  // "error" / "warning"
  QString field;
  QString message;
};

struct ConfigValidation {
  QVector<ConfigDiagnostic> diagnostics;
  bool hasError = false;
  bool hasWarning = false;
};

ConfigValidation validateConfig(const AcsdConfig& config);

QStringList configCommandList();
QStringList configProductProfileList();

}  // namespace gui
}  // namespace acsd
