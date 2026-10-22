#include "config.h"

#include <QJsonArray>
#include <QRegularExpression>

#include "protocol.h"

namespace acsd {
namespace gui {
namespace {

ConfigDiagnostic error(const QString& field, const QString& message) {
  return ConfigDiagnostic{QStringLiteral("error"), field, message};
}

ConfigDiagnostic warning(const QString& field, const QString& message) {
  return ConfigDiagnostic{QStringLiteral("warning"), field, message};
}

}  // namespace

QStringList configCommandList() {
  return {QStringLiteral("normalize"), QStringLiteral("mosaic"), QStringLiteral("export")};
}

QStringList configProductProfileList() {
  return {QStringLiteral("full"), QStringLiteral("signal_only"), QStringLiteral("external_inputs")};
}

QJsonObject AcsdConfig::toJson() const {
  QJsonObject root;
  root.insert(QStringLiteral("schema"), schema);
  root.insert(QStringLiteral("command"), command);

  QJsonArray inputArray;
  for (const ConfigInput& input : inputs) {
    QJsonObject item;
    item.insert(QStringLiteral("path"), input.path);
    if (!input.expectedHash.isEmpty()) {
      item.insert(QStringLiteral("expected_hash"), input.expectedHash);
    }
    if (!input.frameId.isEmpty()) {
      item.insert(QStringLiteral("frame_id"), input.frameId);
    }
    if (!input.calibration.isEmpty()) {
      item.insert(QStringLiteral("calibration"), input.calibration);
    }
    if (!input.response.isEmpty()) {
      item.insert(QStringLiteral("response"), input.response);
    }
    inputArray.append(item);
  }
  root.insert(QStringLiteral("inputs"), inputArray);
  root.insert(QStringLiteral("scientific"), scientific);
  root.insert(QStringLiteral("output"), output);
  if (!catalogue.isEmpty()) {
    root.insert(QStringLiteral("catalogue"), catalogue);
  }

  QJsonObject resourceObject;
  resourceObject.insert(QStringLiteral("bytes"), resources.bytes);
  resourceObject.insert(QStringLiteral("work"), resources.work);
  resourceObject.insert(QStringLiteral("spill_bytes"), resources.spillBytes);
  resourceObject.insert(QStringLiteral("threads"), resources.threads);
  resourceObject.insert(QStringLiteral("max_depth"), resources.maxDepth);
  resourceObject.insert(QStringLiteral("max_precision_bits"), resources.maxPrecisionBits);
  resourceObject.insert(QStringLiteral("scratch_directory"), resources.scratchDirectory);
  root.insert(QStringLiteral("resources"), resourceObject);
  root.insert(QStringLiteral("product_profile"), productProfile);

  if (publication.present) {
    QJsonObject publicationObject;
    publicationObject.insert(QStringLiteral("signal_dtype"), publication.signalDtype);
    publicationObject.insert(QStringLiteral("tile_log2_width"), publication.tileLog2Width);
    publicationObject.insert(QStringLiteral("durable"), publication.durable);
    root.insert(QStringLiteral("publication"), publicationObject);
  }
  return root;
}

bool AcsdConfig::fromJson(const QJsonObject& object, AcsdConfig* out, QString* error) {
  if (out == nullptr) {
    return false;
  }
  AcsdConfig config;

  config.schema = object.value(QStringLiteral("schema")).toString(config.schema);
  config.command = object.value(QStringLiteral("command")).toString(config.command);
  config.scientific = object.value(QStringLiteral("scientific")).toString();
  config.output = object.value(QStringLiteral("output")).toString();
  config.catalogue = object.value(QStringLiteral("catalogue")).toString();
  config.productProfile = object.value(QStringLiteral("product_profile")).toString(config.productProfile);

  const QJsonValue inputsValue = object.value(QStringLiteral("inputs"));
  if (inputsValue.isArray()) {
    for (const QJsonValue& item : inputsValue.toArray()) {
      if (!item.isObject()) {
        if (error != nullptr) {
          *error = QStringLiteral("inputs 数组元素不是对象");
        }
        return false;
      }
      const QJsonObject inputObject = item.toObject();
      ConfigInput input;
      input.path = inputObject.value(QStringLiteral("path")).toString();
      input.expectedHash = inputObject.value(QStringLiteral("expected_hash")).toString();
      input.frameId = inputObject.value(QStringLiteral("frame_id")).toString();
      input.calibration = inputObject.value(QStringLiteral("calibration")).toString();
      input.response = inputObject.value(QStringLiteral("response")).toString();
      config.inputs.append(input);
    }
  } else if (!inputsValue.isUndefined()) {
    if (error != nullptr) {
      *error = QStringLiteral("inputs 不是数组");
    }
    return false;
  }

  const QJsonValue resourcesValue = object.value(QStringLiteral("resources"));
  if (resourcesValue.isObject()) {
    const QJsonObject resourceObject = resourcesValue.toObject();
    config.resources.bytes =
        static_cast<qint64>(resourceObject.value(QStringLiteral("bytes")).toDouble(config.resources.bytes));
    config.resources.work =
        static_cast<qint64>(resourceObject.value(QStringLiteral("work")).toDouble(config.resources.work));
    config.resources.spillBytes = static_cast<qint64>(
        resourceObject.value(QStringLiteral("spill_bytes")).toDouble(config.resources.spillBytes));
    config.resources.threads =
        resourceObject.value(QStringLiteral("threads")).toInt(config.resources.threads);
    config.resources.maxDepth =
        resourceObject.value(QStringLiteral("max_depth")).toInt(config.resources.maxDepth);
    config.resources.maxPrecisionBits = resourceObject
                                            .value(QStringLiteral("max_precision_bits"))
                                            .toInt(config.resources.maxPrecisionBits);
    config.resources.scratchDirectory =
        resourceObject.value(QStringLiteral("scratch_directory")).toString();
  }

  const QJsonValue publicationValue = object.value(QStringLiteral("publication"));
  if (publicationValue.isObject()) {
    const QJsonObject publicationObject = publicationValue.toObject();
    config.publication.present = true;
    config.publication.signalDtype =
        publicationObject.value(QStringLiteral("signal_dtype")).toString(config.publication.signalDtype);
    config.publication.tileLog2Width = publicationObject
                                           .value(QStringLiteral("tile_log2_width"))
                                           .toInt(config.publication.tileLog2Width);
    config.publication.durable =
        publicationObject.value(QStringLiteral("durable")).toBool(config.publication.durable);
  }

  *out = config;
  return true;
}

ConfigValidation validateConfig(const AcsdConfig& config) {
  ConfigValidation result;

  // schema major —— 不兼容直接拒绝运行。
  const ConfigVersion version = parseConfigSchema(config.toJson());
  if (!version.ok) {
    result.diagnostics.append(error(QStringLiteral("schema"), version.detail));
    result.hasError = true;
  } else if (version.major != kConfigMajor) {
    result.diagnostics.append(
        error(QStringLiteral("schema"),
              QStringLiteral("配置 schema major %1 与 GUI 支持的 %2 不兼容，拒绝运行")
                  .arg(version.major)
                  .arg(kConfigMajor)));
    result.hasError = true;
  }

  if (!configCommandList().contains(config.command)) {
    result.diagnostics.append(error(QStringLiteral("command"), QStringLiteral("命令非法：%1").arg(config.command)));
    result.hasError = true;
  }
  if (!configProductProfileList().contains(config.productProfile)) {
    result.diagnostics.append(
        error(QStringLiteral("product_profile"), QStringLiteral("产品剖面非法：%1").arg(config.productProfile)));
    result.hasError = true;
  }
  if (config.inputs.isEmpty()) {
    result.diagnostics.append(error(QStringLiteral("inputs"), QStringLiteral("至少需要一个输入")));
    result.hasError = true;
  }
  if (config.scientific.trimmed().isEmpty()) {
    result.diagnostics.append(error(QStringLiteral("scientific"), QStringLiteral("科学文件路径为空")));
    result.hasError = true;
  }
  if (config.output.trimmed().isEmpty()) {
    result.diagnostics.append(error(QStringLiteral("output"), QStringLiteral("输出目录为空")));
    result.hasError = true;
  }

  static const QRegularExpression hashPattern(QStringLiteral("^sha256:[0-9a-f]{64}$"));
  const bool isNormalize = config.command == QStringLiteral("normalize");
  for (int i = 0; i < config.inputs.size(); ++i) {
    const ConfigInput& input = config.inputs.at(i);
    const QString label = QStringLiteral("inputs[%1]").arg(i);
    if (input.path.trimmed().isEmpty()) {
      result.diagnostics.append(error(label, QStringLiteral("输入路径为空")));
      result.hasError = true;
      continue;
    }
    if (isNormalize) {
      if (input.frameId.trimmed().isEmpty()) {
        result.diagnostics.append(
            warning(label + QStringLiteral(".frame_id"),
                    QStringLiteral("config.schema.json 要求 normalize 输入带 frame_id；CLI 会判 invalid_request")));
        result.hasWarning = true;
      }
      if (input.response.trimmed().isEmpty()) {
        result.diagnostics.append(
            warning(label + QStringLiteral(".response"),
                    QStringLiteral("config.schema.json 要求 normalize 输入带 response 路径")));
        result.hasWarning = true;
      }
    } else {
      if (input.expectedHash.trimmed().isEmpty()) {
        result.diagnostics.append(
            warning(label + QStringLiteral(".expected_hash"),
                    QStringLiteral("config.schema.json 要求 %1 输入带 expected_hash").arg(config.command)));
        result.hasWarning = true;
      } else if (!hashPattern.match(input.expectedHash).hasMatch()) {
        result.diagnostics.append(
            error(label + QStringLiteral(".expected_hash"),
                  QStringLiteral("expected_hash 必须形如 sha256:<64 位小写十六进制>")));
        result.hasError = true;
      }
    }
  }
  if (isNormalize && config.catalogue.trimmed().isEmpty()) {
    result.diagnostics.append(
        warning(QStringLiteral("catalogue"), QStringLiteral("config.schema.json 要求 normalize 带 catalogue")));
    result.hasWarning = true;
  }
  if (config.command == QStringLiteral("export") && config.inputs.size() > 1) {
    result.diagnostics.append(error(QStringLiteral("inputs"), QStringLiteral("export 只允许一个输入")));
    result.hasError = true;
  }

  const ConfigResources& resources = config.resources;
  if (resources.bytes < 1) {
    result.diagnostics.append(error(QStringLiteral("resources.bytes"), QStringLiteral("bytes 必须 ≥ 1")));
    result.hasError = true;
  }
  if (resources.work < 0) {
    result.diagnostics.append(error(QStringLiteral("resources.work"), QStringLiteral("work 必须 ≥ 0")));
    result.hasError = true;
  }
  if (resources.spillBytes < 0) {
    result.diagnostics.append(error(QStringLiteral("resources.spill_bytes"), QStringLiteral("spill_bytes 必须 ≥ 0")));
    result.hasError = true;
  }
  if (resources.threads < 1 || resources.threads > 1024) {
    result.diagnostics.append(error(QStringLiteral("resources.threads"), QStringLiteral("threads 必须在 1..1024")));
    result.hasError = true;
  }
  if (resources.maxDepth < 1 || resources.maxDepth > 256) {
    result.diagnostics.append(error(QStringLiteral("resources.max_depth"), QStringLiteral("max_depth 必须在 1..256")));
    result.hasError = true;
  }
  if (resources.maxPrecisionBits < 64 || resources.maxPrecisionBits > 16384) {
    result.diagnostics.append(
        error(QStringLiteral("resources.max_precision_bits"), QStringLiteral("max_precision_bits 必须在 64..16384")));
    result.hasError = true;
  }
  if (resources.scratchDirectory.trimmed().isEmpty()) {
    result.diagnostics.append(
        error(QStringLiteral("resources.scratch_directory"), QStringLiteral("scratch_directory 为空")));
    result.hasError = true;
  }

  if (config.publication.present) {
    if (config.publication.signalDtype != QStringLiteral("f32") &&
        config.publication.signalDtype != QStringLiteral("f64")) {
      result.diagnostics.append(
          error(QStringLiteral("publication.signal_dtype"), QStringLiteral("signal_dtype 必须是 f32 或 f64")));
      result.hasError = true;
    }
    if (config.publication.tileLog2Width < 6 || config.publication.tileLog2Width > 9) {
      result.diagnostics.append(
          error(QStringLiteral("publication.tile_log2_width"), QStringLiteral("tile_log2_width 必须在 6..9")));
      result.hasError = true;
    }
  }

  return result;
}

}  // namespace gui
}  // namespace acsd
