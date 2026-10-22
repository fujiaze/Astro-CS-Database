// ACSD v2 GUI —— 主窗口
//
// 功能：配置编辑、输入分组、运行/取消、事件显示、产品浏览。
// 只消费 CLI 配置 / JSONL 事件 / 产品目录，不解析内部 C++ 对象，不做科学运算。
#pragma once

#include <QMainWindow>
#include <QJsonObject>
#include <QVector>

#include "config.h"
#include "product.h"
#include "protocol.h"
#include "runner.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QSpinBox;
class QTableWidget;
class QTabWidget;
class QTreeWidget;
class QTreeWidgetItem;

namespace acsd {
namespace gui {

class MainWindow : public QMainWindow {
  Q_OBJECT

 public:
  explicit MainWindow(QWidget* parent = nullptr);
  ~MainWindow() override;

  // ---- 供冒烟模式与测试驱动的公开接口 --------------------------------------
  bool setCliPath(const QString& path, QString* error = nullptr);
  QString cliPath() const;
  bool loadConfigFile(const QString& path, QString* error = nullptr);
  // 用 `acsd template <command>` 的静态模板填充配置编辑器。
  bool loadTemplateFromCli(const QString& command, QString* error = nullptr);
  bool saveConfigFile(const QString& path, QString* error = nullptr);
  QString configFilePath() const;
  AcsdConfig currentConfig() const;
  void setCurrentConfig(const AcsdConfig& config);
  void setWorkingDirectory(const QString& directory);
  bool startRun(QString* error = nullptr);
  // 直接用给定的配置文件运行（不重写配置、不落临时文件）。
  // 命令取自该配置文件的 command 字段。冒烟用真实 CLI 时使用，
  // 避免在别人的目录里写运行配置。
  bool startRunWithConfig(const QString& configPath, QString* error = nullptr);
  bool cancelRun();
  void forceTerminate();
  ProbeResult probeVersion();
  bool versionCompatible() const { return versionCompatible_; }
  QString versionReason() const { return versionReason_; }
  void setProcessEnvironmentVariable(const QString& key, const QString& value);
  QJsonObject evidenceSnapshot() const;
  // 读取产品目录里的 manifest.json 并刷新产品页；返回解析结果供冒烟检查。
  ProductManifestView loadProductManifest(const QString& directory);
  int inputGroupCount() const;
  const QVector<AcsdEvent>& events() const { return runner_.events(); }
  RunController* runner() { return &runner_; }
  QString eventStreamFilter() const { return groupFilter_; }

 signals:
  void logAppended(const QString& text);
  void runStateChanged(int state);
  void runFinished(int state);

 private:
  void buildUi();
  QWidget* buildRunPanel();
  QWidget* buildConfigTab();
  QWidget* buildGroupTab();
  QWidget* buildEventTab();
  QWidget* buildProductTab();
  void wireSignals();

  void applyConfigToWidgets(const AcsdConfig& config);
  AcsdConfig collectConfig() const;
  void refreshInputGroups();
  void rebuildEventTable();
  void appendLog(const QString& text);
  void updateStatus();
  void onEventReceived(const AcsdEvent& event);
  void onStateChanged(RunState state);
  void onRunFinished(RunState state);
  void onGroupSelectionChanged();
  bool writeRunConfig(QString* path, QString* error) const;
  bool checkVersionForRun(QString* reason) const;

  // 运行控制
  QLineEdit* cliPathEdit_ = nullptr;
  QLineEdit* configPathEdit_ = nullptr;
  QLineEdit* workingDirEdit_ = nullptr;
  QPushButton* runButton_ = nullptr;
  QPushButton* cancelButton_ = nullptr;
  QPushButton* terminateButton_ = nullptr;
  QPushButton* probeButton_ = nullptr;
  QLabel* statusLabel_ = nullptr;
  QLabel* versionLabel_ = nullptr;

  // 配置编辑
  QComboBox* commandCombo_ = nullptr;
  QTableWidget* inputTable_ = nullptr;
  QLineEdit* scientificEdit_ = nullptr;
  QLineEdit* outputEdit_ = nullptr;
  QLineEdit* catalogueEdit_ = nullptr;
  QComboBox* profileCombo_ = nullptr;
  QLineEdit* bytesEdit_ = nullptr;
  QLineEdit* workEdit_ = nullptr;
  QLineEdit* spillEdit_ = nullptr;
  QSpinBox* threadsSpin_ = nullptr;
  QSpinBox* depthSpin_ = nullptr;
  QSpinBox* bitsSpin_ = nullptr;
  QLineEdit* scratchEdit_ = nullptr;
  QCheckBox* publicationEnabled_ = nullptr;
  QComboBox* dtypeCombo_ = nullptr;
  QSpinBox* tileSpin_ = nullptr;
  QCheckBox* durableCheck_ = nullptr;
  QLabel* configValidationLabel_ = nullptr;

  // 输入分组
  QTreeWidget* groupTree_ = nullptr;

  // 事件
  QTableWidget* eventTable_ = nullptr;
  QProgressBar* progressBar_ = nullptr;
  QPlainTextEdit* summaryEdit_ = nullptr;
  QPlainTextEdit* detailEdit_ = nullptr;
  QPlainTextEdit* logEdit_ = nullptr;

  // 产品
  QLineEdit* productDirEdit_ = nullptr;
  QTreeWidget* productTree_ = nullptr;
  QPlainTextEdit* productRawEdit_ = nullptr;

  QTabWidget* tabs_ = nullptr;

  RunController runner_;
  QString configFilePath_;
  QString groupFilter_;
  bool versionCompatible_ = true;
  QString versionReason_;
  ProbeResult lastProbe_;
};

}  // namespace gui
}  // namespace acsd
