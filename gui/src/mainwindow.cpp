#include "mainwindow.h"

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProcessEnvironment>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QSpinBox>
#include <QSplitter>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace acsd {
namespace gui {
namespace {

QString jsonToText(const QJsonObject& object) {
  return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Indented));
}

QTableWidgetItem* makeItem(const QString& text) {
  auto* item = new QTableWidgetItem(text);
  item->setFlags(item->flags() & ~Qt::ItemIsEditable);
  return item;
}

}  // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
  buildUi();
  wireSignals();
  updateStatus();
}

MainWindow::~MainWindow() = default;

void MainWindow::buildUi() {
  setWindowTitle(QStringLiteral("ACSD v2 控制台"));
  resize(1180, 780);

  auto* central = new QWidget(this);
  auto* layout = new QVBoxLayout(central);
  layout->setContentsMargins(8, 8, 8, 8);

  layout->addWidget(buildRunPanel());

  tabs_ = new QTabWidget(central);
  tabs_->addTab(buildConfigTab(), QStringLiteral("配置"));
  tabs_->addTab(buildGroupTab(), QStringLiteral("输入分组"));
  tabs_->addTab(buildEventTab(), QStringLiteral("事件"));
  tabs_->addTab(buildProductTab(), QStringLiteral("产品"));
  layout->addWidget(tabs_, 1);

  logEdit_ = new QPlainTextEdit(central);
  logEdit_->setReadOnly(true);
  logEdit_->setMaximumBlockCount(2000);
  logEdit_->setMaximumHeight(140);
  logEdit_->setPlaceholderText(QStringLiteral("GUI 日志"));
  layout->addWidget(logEdit_);

  setCentralWidget(central);
  statusBar()->showMessage(QStringLiteral("就绪"));
}

QWidget* MainWindow::buildRunPanel() {
  auto* box = new QGroupBox(QStringLiteral("运行控制"), this);
  auto* form = new QFormLayout(box);

  auto* cliRow = new QWidget(box);
  auto* cliLayout = new QHBoxLayout(cliRow);
  cliLayout->setContentsMargins(0, 0, 0, 0);
  cliPathEdit_ = new QLineEdit(cliRow);
  cliPathEdit_->setPlaceholderText(QStringLiteral("acsd 可执行文件路径"));
  auto* cliBrowse = new QPushButton(QStringLiteral("浏览…"), cliRow);
  cliLayout->addWidget(cliPathEdit_, 1);
  cliLayout->addWidget(cliBrowse);
  form->addRow(QStringLiteral("CLI"), cliRow);
  connect(cliBrowse, &QPushButton::clicked, this, [this]() {
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("选择 acsd 可执行文件"));
    if (!path.isEmpty()) {
      cliPathEdit_->setText(path);
    }
  });

  auto* configRow = new QWidget(box);
  auto* configLayout = new QHBoxLayout(configRow);
  configLayout->setContentsMargins(0, 0, 0, 0);
  configPathEdit_ = new QLineEdit(configRow);
  configPathEdit_->setPlaceholderText(QStringLiteral("acsd.config/2 配置文件"));
  auto* configOpen = new QPushButton(QStringLiteral("打开…"), configRow);
  auto* configSave = new QPushButton(QStringLiteral("保存…"), configRow);
  configLayout->addWidget(configPathEdit_, 1);
  configLayout->addWidget(configOpen);
  configLayout->addWidget(configSave);
  form->addRow(QStringLiteral("配置文件"), configRow);
  connect(configOpen, &QPushButton::clicked, this, [this]() {
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("打开配置"), QString(),
                                                      QStringLiteral("JSON 配置 (*.json)"));
    if (!path.isEmpty()) {
      loadConfigFile(path);
    }
  });
  connect(configSave, &QPushButton::clicked, this, [this]() {
    QString path = configPathEdit_->text();
    if (path.isEmpty()) {
      path = QFileDialog::getSaveFileName(this, QStringLiteral("保存配置"), QString(),
                                          QStringLiteral("JSON 配置 (*.json)"));
    }
    if (path.isEmpty()) {
      return;
    }
    QString error;
    if (!saveConfigFile(path, &error)) {
      QMessageBox::warning(this, QStringLiteral("保存配置失败"), error);
      return;
    }
    appendLog(QStringLiteral("配置已保存：%1").arg(path));
  });

  auto* workRow = new QWidget(box);
  auto* workLayout = new QHBoxLayout(workRow);
  workLayout->setContentsMargins(0, 0, 0, 0);
  workingDirEdit_ = new QLineEdit(QDir::currentPath(), workRow);
  auto* workBrowse = new QPushButton(QStringLiteral("浏览…"), workRow);
  workLayout->addWidget(workingDirEdit_, 1);
  workLayout->addWidget(workBrowse);
  form->addRow(QStringLiteral("工作目录"), workRow);
  connect(workBrowse, &QPushButton::clicked, this, [this]() {
    const QString path = QFileDialog::getExistingDirectory(this, QStringLiteral("选择工作目录"));
    if (!path.isEmpty()) {
      workingDirEdit_->setText(path);
    }
  });

  auto* buttonRow = new QWidget(box);
  auto* buttonLayout = new QHBoxLayout(buttonRow);
  buttonLayout->setContentsMargins(0, 0, 0, 0);
  runButton_ = new QPushButton(QStringLiteral("运行"), buttonRow);
  cancelButton_ = new QPushButton(QStringLiteral("取消"), buttonRow);
  terminateButton_ = new QPushButton(QStringLiteral("强制终止"), buttonRow);
  probeButton_ = new QPushButton(QStringLiteral("版本探测"), buttonRow);
  runButton_->setDefault(true);
  cancelButton_->setEnabled(false);
  terminateButton_->setEnabled(false);
  buttonLayout->addWidget(runButton_);
  buttonLayout->addWidget(cancelButton_);
  buttonLayout->addWidget(terminateButton_);
  buttonLayout->addWidget(probeButton_);
  buttonLayout->addStretch(1);
  statusLabel_ = new QLabel(QStringLiteral("状态：空闲"), buttonRow);
  versionLabel_ = new QLabel(QStringLiteral("版本：未探测"), buttonRow);
  buttonLayout->addWidget(statusLabel_);
  buttonLayout->addWidget(versionLabel_);
  form->addRow(QString(), buttonRow);

  return box;
}

QWidget* MainWindow::buildConfigTab() {
  auto* page = new QWidget(this);
  auto* layout = new QVBoxLayout(page);

  auto* commandBox = new QGroupBox(QStringLiteral("命令与路径"), page);
  auto* commandForm = new QFormLayout(commandBox);
  commandCombo_ = new QComboBox(commandBox);
  commandCombo_->addItems(configCommandList());
  commandForm->addRow(QStringLiteral("命令"), commandCombo_);

  auto* scientificRow = new QWidget(commandBox);
  auto* scientificLayout = new QHBoxLayout(scientificRow);
  scientificLayout->setContentsMargins(0, 0, 0, 0);
  scientificEdit_ = new QLineEdit(scientificRow);
  scientificEdit_->setPlaceholderText(QStringLiteral("科学请求 JSON 路径（相对配置文件目录）"));
  auto* scientificBrowse = new QPushButton(QStringLiteral("浏览…"), scientificRow);
  scientificLayout->addWidget(scientificEdit_, 1);
  scientificLayout->addWidget(scientificBrowse);
  commandForm->addRow(QStringLiteral("科学文件"), scientificRow);
  connect(scientificBrowse, &QPushButton::clicked, this, [this]() {
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("选择科学文件"), QString(),
                                                      QStringLiteral("JSON (*.json)"));
    if (!path.isEmpty()) {
      scientificEdit_->setText(path);
    }
  });

  auto* outputRow = new QWidget(commandBox);
  auto* outputLayout = new QHBoxLayout(outputRow);
  outputLayout->setContentsMargins(0, 0, 0, 0);
  outputEdit_ = new QLineEdit(outputRow);
  outputEdit_->setPlaceholderText(QStringLiteral("产品输出目录"));
  auto* outputBrowse = new QPushButton(QStringLiteral("浏览…"), outputRow);
  outputLayout->addWidget(outputEdit_, 1);
  outputLayout->addWidget(outputBrowse);
  commandForm->addRow(QStringLiteral("输出目录"), outputRow);
  connect(outputBrowse, &QPushButton::clicked, this, [this]() {
    const QString path = QFileDialog::getExistingDirectory(this, QStringLiteral("选择输出目录"));
    if (!path.isEmpty()) {
      outputEdit_->setText(path);
    }
  });

  auto* catalogueRow = new QWidget(commandBox);
  auto* catalogueLayout = new QHBoxLayout(catalogueRow);
  catalogueLayout->setContentsMargins(0, 0, 0, 0);
  catalogueEdit_ = new QLineEdit(catalogueRow);
  catalogueEdit_->setPlaceholderText(QStringLiteral("星表索引路径（normalize 需要）"));
  auto* catalogueBrowse = new QPushButton(QStringLiteral("浏览…"), catalogueRow);
  catalogueLayout->addWidget(catalogueEdit_, 1);
  catalogueLayout->addWidget(catalogueBrowse);
  commandForm->addRow(QStringLiteral("星表"), catalogueRow);
  connect(catalogueBrowse, &QPushButton::clicked, this, [this]() {
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("选择星表"), QString());
    if (!path.isEmpty()) {
      catalogueEdit_->setText(path);
    }
  });

  profileCombo_ = new QComboBox(commandBox);
  profileCombo_->addItems(configProductProfileList());
  commandForm->addRow(QStringLiteral("产品剖面"), profileCombo_);
  layout->addWidget(commandBox);

  auto* inputBox = new QGroupBox(QStringLiteral("输入"), page);
  auto* inputLayout = new QVBoxLayout(inputBox);
  inputTable_ = new QTableWidget(0, 5, inputBox);
  inputTable_->setHorizontalHeaderLabels({QStringLiteral("路径"), QStringLiteral("expected_hash"),
                                          QStringLiteral("frame_id"), QStringLiteral("calibration"),
                                          QStringLiteral("response")});
  inputTable_->horizontalHeader()->setStretchLastSection(true);
  inputLayout->addWidget(inputTable_);
  auto* inputButtons = new QWidget(inputBox);
  auto* inputButtonLayout = new QHBoxLayout(inputButtons);
  inputButtonLayout->setContentsMargins(0, 0, 0, 0);
  auto* addInput = new QPushButton(QStringLiteral("添加输入"), inputButtons);
  auto* removeInput = new QPushButton(QStringLiteral("删除选中"), inputButtons);
  auto* upInput = new QPushButton(QStringLiteral("上移"), inputButtons);
  auto* downInput = new QPushButton(QStringLiteral("下移"), inputButtons);
  inputButtonLayout->addWidget(addInput);
  inputButtonLayout->addWidget(removeInput);
  inputButtonLayout->addWidget(upInput);
  inputButtonLayout->addWidget(downInput);
  inputButtonLayout->addStretch(1);
  inputLayout->addWidget(inputButtons);
  connect(addInput, &QPushButton::clicked, this, [this]() { inputTable_->insertRow(inputTable_->rowCount()); });
  connect(removeInput, &QPushButton::clicked, this, [this]() {
    const int row = inputTable_->currentRow();
    if (row >= 0) {
      inputTable_->removeRow(row);
    }
  });
  // 上下移动输入行：QTableWidget 没有 takeRow，逐列取放。
  auto moveInputRow = [this](int from, int to) {
    const int columns = inputTable_->columnCount();
    QVector<QTableWidgetItem*> items;
    for (int column = 0; column < columns; ++column) {
      items.append(inputTable_->takeItem(from, column));
    }
    inputTable_->removeRow(from);
    inputTable_->insertRow(to);
    for (int column = 0; column < columns; ++column) {
      inputTable_->setItem(to, column, items.at(column));
    }
    inputTable_->setCurrentCell(to, 0);
  };
  connect(upInput, &QPushButton::clicked, this, [this, moveInputRow]() {
    const int row = inputTable_->currentRow();
    if (row <= 0) {
      return;
    }
    moveInputRow(row, row - 1);
  });
  connect(downInput, &QPushButton::clicked, this, [this, moveInputRow]() {
    const int row = inputTable_->currentRow();
    if (row < 0 || row >= inputTable_->rowCount() - 1) {
      return;
    }
    moveInputRow(row, row + 1);
  });
  layout->addWidget(inputBox, 1);

  auto* resourceBox = new QGroupBox(QStringLiteral("资源与发布"), page);
  auto* resourceForm = new QFormLayout(resourceBox);
  auto* integerValidator = new QRegularExpressionValidator(QRegularExpression(QStringLiteral("^[0-9]+$")), this);

  bytesEdit_ = new QLineEdit(resourceBox);
  bytesEdit_->setValidator(integerValidator);
  bytesEdit_->setText(QString::number(1073741824LL));
  resourceForm->addRow(QStringLiteral("bytes"), bytesEdit_);

  workEdit_ = new QLineEdit(resourceBox);
  workEdit_->setValidator(integerValidator);
  workEdit_->setText(QStringLiteral("10000000"));
  resourceForm->addRow(QStringLiteral("work"), workEdit_);

  spillEdit_ = new QLineEdit(resourceBox);
  spillEdit_->setValidator(integerValidator);
  spillEdit_->setText(QString::number(8589934592LL));
  resourceForm->addRow(QStringLiteral("spill_bytes"), spillEdit_);

  threadsSpin_ = new QSpinBox(resourceBox);
  threadsSpin_->setRange(1, 1024);
  threadsSpin_->setValue(8);
  resourceForm->addRow(QStringLiteral("threads"), threadsSpin_);

  depthSpin_ = new QSpinBox(resourceBox);
  depthSpin_->setRange(1, 256);
  depthSpin_->setValue(64);
  resourceForm->addRow(QStringLiteral("max_depth"), depthSpin_);

  bitsSpin_ = new QSpinBox(resourceBox);
  bitsSpin_->setRange(64, 16384);
  bitsSpin_->setValue(64);
  resourceForm->addRow(QStringLiteral("max_precision_bits"), bitsSpin_);

  scratchEdit_ = new QLineEdit(resourceBox);
  scratchEdit_->setText(QString());
  resourceForm->addRow(QStringLiteral("scratch_directory"), scratchEdit_);

  publicationEnabled_ = new QCheckBox(QStringLiteral("显式覆盖发布参数"), resourceBox);
  resourceForm->addRow(QString(), publicationEnabled_);
  dtypeCombo_ = new QComboBox(resourceBox);
  dtypeCombo_->addItems({QStringLiteral("f64"), QStringLiteral("f32")});
  resourceForm->addRow(QStringLiteral("signal_dtype"), dtypeCombo_);
  tileSpin_ = new QSpinBox(resourceBox);
  tileSpin_->setRange(6, 9);
  tileSpin_->setValue(9);
  resourceForm->addRow(QStringLiteral("tile_log2_width"), tileSpin_);
  durableCheck_ = new QCheckBox(QStringLiteral("durable"), resourceBox);
  durableCheck_->setChecked(true);
  resourceForm->addRow(QString(), durableCheck_);
  layout->addWidget(resourceBox);

  configValidationLabel_ = new QLabel(page);
  configValidationLabel_->setWordWrap(true);
  layout->addWidget(configValidationLabel_);

  return page;
}

QWidget* MainWindow::buildGroupTab() {
  auto* page = new QWidget(this);
  auto* layout = new QVBoxLayout(page);
  auto* hint = new QLabel(
      QStringLiteral("输入按 frame_id（normalize）或产品（mosaic/export）分组；选中分组可过滤事件表。"), page);
  hint->setWordWrap(true);
  layout->addWidget(hint);
  groupTree_ = new QTreeWidget(page);
  groupTree_->setHeaderLabels({QStringLiteral("分组"), QStringLiteral("数量"), QStringLiteral("说明")});
  groupTree_->setColumnCount(3);
  layout->addWidget(groupTree_, 1);
  return page;
}

QWidget* MainWindow::buildEventTab() {
  auto* page = new QWidget(this);
  auto* layout = new QVBoxLayout(page);

  progressBar_ = new QProgressBar(page);
  progressBar_->setRange(0, 100);
  progressBar_->setValue(0);
  progressBar_->setFormat(QStringLiteral("%p%"));
  layout->addWidget(progressBar_);

  auto* splitter = new QSplitter(Qt::Vertical, page);
  eventTable_ = new QTableWidget(0, 7, splitter);
  eventTable_->setHorizontalHeaderLabels({QStringLiteral("seq"), QStringLiteral("事件"), QStringLiteral("frame_id"),
                                          QStringLiteral("block"), QStringLiteral("code"),
                                          QStringLiteral("进度"), QStringLiteral("消息")});
  eventTable_->horizontalHeader()->setStretchLastSection(true);
  eventTable_->setSortingEnabled(false);
  splitter->addWidget(eventTable_);

  auto* lower = new QWidget(splitter);
  auto* lowerLayout = new QHBoxLayout(lower);
  lowerLayout->setContentsMargins(0, 0, 0, 0);
  summaryEdit_ = new QPlainTextEdit(lower);
  summaryEdit_->setReadOnly(true);
  summaryEdit_->setPlaceholderText(QStringLiteral("run_summary"));
  detailEdit_ = new QPlainTextEdit(lower);
  detailEdit_->setReadOnly(true);
  detailEdit_->setPlaceholderText(QStringLiteral("选中事件的 assessment / 细节"));
  lowerLayout->addWidget(summaryEdit_, 1);
  lowerLayout->addWidget(detailEdit_, 1);
  splitter->addWidget(lower);
  splitter->setStretchFactor(0, 3);
  splitter->setStretchFactor(1, 1);
  layout->addWidget(splitter, 1);
  return page;
}

QWidget* MainWindow::buildProductTab() {
  auto* page = new QWidget(this);
  auto* layout = new QVBoxLayout(page);
  auto* row = new QWidget(page);
  auto* rowLayout = new QHBoxLayout(row);
  rowLayout->setContentsMargins(0, 0, 0, 0);
  productDirEdit_ = new QLineEdit(row);
  productDirEdit_->setPlaceholderText(QStringLiteral("产品目录（含 manifest.json）"));
  auto* browse = new QPushButton(QStringLiteral("浏览…"), row);
  auto* read = new QPushButton(QStringLiteral("读取 manifest"), row);
  rowLayout->addWidget(productDirEdit_, 1);
  rowLayout->addWidget(browse);
  rowLayout->addWidget(read);
  layout->addWidget(row);

  auto* splitter = new QSplitter(Qt::Vertical, page);
  productTree_ = new QTreeWidget(splitter);
  productTree_->setColumnCount(3);
  productTree_->setHeaderLabels({QStringLiteral("项"), QStringLiteral("值"), QStringLiteral("路径")});
  splitter->addWidget(productTree_);
  productRawEdit_ = new QPlainTextEdit(splitter);
  productRawEdit_->setReadOnly(true);
  splitter->addWidget(productRawEdit_);
  splitter->setStretchFactor(0, 2);
  splitter->setStretchFactor(1, 1);
  layout->addWidget(splitter, 1);

  connect(browse, &QPushButton::clicked, this, [this]() {
    const QString path = QFileDialog::getExistingDirectory(this, QStringLiteral("选择产品目录"));
    if (!path.isEmpty()) {
      productDirEdit_->setText(path);
    }
  });
  connect(read, &QPushButton::clicked, this, [this]() {
    const ProductManifestView view = loadProductManifest(productDirEdit_->text());
    if (!view.ok) {
      appendLog(QStringLiteral("产品读取失败：%1").arg(view.error));
    }
  });
  return page;
}

ProductManifestView MainWindow::loadProductManifest(const QString& directory) {
  const ProductManifestView view = readProductManifest(directory);
  productTree_->clear();
  if (!view.ok) {
    productRawEdit_->setPlainText(view.error);
    return view;
  }
  auto addRow = [&](const QString& key, const QString& value, const QString& path) {
    auto* item = new QTreeWidgetItem(productTree_);
    item->setText(0, key);
    item->setText(1, value);
    item->setText(2, path);
  };
  addRow(QStringLiteral("阶段 stage"), view.stage, QString());
  addRow(QStringLiteral("剖面 profile"), view.profile, QString());
  addRow(QStringLiteral("完整 complete"),
         view.hasComplete ? (view.complete ? QStringLiteral("true") : QStringLiteral("false"))
                          : QStringLiteral("（未提供）"),
         QString());
  addRow(QStringLiteral("能力 capabilities"), view.capabilities.join(QStringLiteral(", ")), QString());
  addRow(QStringLiteral("产品 product_id"), view.productId, QString());
  addRow(QStringLiteral("版本 format/major/minor"),
         QStringLiteral("%1 / %2 / %3").arg(view.format).arg(view.major).arg(view.minor), QString());
  addRow(QStringLiteral("根对象 root_object"), view.rootObject, QString());
  addRow(QStringLiteral("配置哈希"), view.configurationHash, QString());
  for (const ProductObjectEntry& object : view.objects) {
    addRow(QStringLiteral("对象 %1").arg(object.type.isEmpty() ? object.hash : object.type),
           QStringLiteral("%1 (v%2.%3)").arg(object.hash).arg(object.major).arg(object.minor),
           object.path.isEmpty() ? QString() : QDir(view.directory).filePath(object.path));
  }
  for (const ProductBlockEntry& block : view.blocks) {
    QStringList shape;
    for (qint64 dim : block.shape) {
      shape.append(QString::number(dim));
    }
    addRow(QStringLiteral("块 %1").arg(block.id),
           QStringLiteral("%1 shape=[%2]").arg(block.dtype, shape.join(QStringLiteral(","))),
           block.path.isEmpty() ? QString() : QDir(view.directory).filePath(block.path));
  }
  for (const ProductAlgorithmEntry& algorithm : view.algorithms) {
    addRow(QStringLiteral("算法 %1").arg(algorithm.name),
           QStringLiteral("%1 (%2)").arg(algorithm.version, algorithm.sourceHash), QString());
  }
  productRawEdit_->setPlainText(jsonToText(view.raw));
  appendLog(QStringLiteral("已读取产品 manifest：%1").arg(view.directory));
  return view;
}

int MainWindow::inputGroupCount() const {
  return groupTree_ == nullptr ? 0 : groupTree_->topLevelItemCount();
}

void MainWindow::wireSignals() {
  connect(runButton_, &QPushButton::clicked, this, [this]() {
    QString error;
    if (!startRun(&error)) {
      appendLog(QStringLiteral("启动失败：%1").arg(error));
    }
  });
  connect(cancelButton_, &QPushButton::clicked, this, [this]() {
    if (!cancelRun()) {
      appendLog(QStringLiteral("取消请求未发出"));
    }
  });
  connect(terminateButton_, &QPushButton::clicked, this, [this]() { forceTerminate(); });
  connect(probeButton_, &QPushButton::clicked, this, [this]() { probeVersion(); });

  connect(&runner_, &RunController::eventReceived, this, &MainWindow::onEventReceived);
  connect(&runner_, &RunController::stateChanged, this, &MainWindow::onStateChanged);
  connect(&runner_, &RunController::runFinished, this, &MainWindow::onRunFinished);
  connect(&runner_, &RunController::logMessage, this, &MainWindow::appendLog);

  connect(commandCombo_, &QComboBox::currentTextChanged, this, [this](const QString&) {
    const bool isNormalize = commandCombo_->currentText() == QStringLiteral("normalize");
    catalogueEdit_->setEnabled(isNormalize);
    refreshInputGroups();
  });
  connect(inputTable_, &QTableWidget::itemChanged, this, [this]() { refreshInputGroups(); });
  connect(groupTree_, &QTreeWidget::itemSelectionChanged, this, &MainWindow::onGroupSelectionChanged);
  connect(eventTable_, &QTableWidget::itemSelectionChanged, this, [this]() {
    const int row = eventTable_->currentRow();
    if (row < 0 || row >= eventTable_->rowCount()) {
      detailEdit_->clear();
      return;
    }
    const QTableWidgetItem* item = eventTable_->item(row, 0);
    if (item == nullptr) {
      return;
    }
    bool ok = false;
    const int index = item->data(Qt::UserRole).toInt(&ok);
    if (!ok || index < 0 || index >= runner_.events().size()) {
      return;
    }
    const AcsdEvent& event = runner_.events().at(index);
    QJsonObject detail;
    detail.insert(QStringLiteral("seq"), event.seq);
    detail.insert(QStringLiteral("event"), event.kindName);
    detail.insert(QStringLiteral("run_id"), event.runId);
    if (event.hasFrameId) {
      detail.insert(QStringLiteral("frame_id"), event.frameId);
    }
    if (event.block.has_value()) {
      detail.insert(QStringLiteral("block"), *event.block);
    }
    detail.insert(QStringLiteral("code"), event.code);
    detail.insert(QStringLiteral("message"), event.message);
    if (event.hasAssessment) {
      detail.insert(QStringLiteral("assessment"), event.assessment);
    }
    if (event.hasSummary) {
      detail.insert(QStringLiteral("summary"), event.summary);
    }
    detailEdit_->setPlainText(jsonToText(detail));
  });
}

void MainWindow::appendLog(const QString& text) {
  if (text.trimmed().isEmpty()) {
    return;
  }
  const QString line = QStringLiteral("[%1] %2")
                           .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")), text);
  logEdit_->appendPlainText(line);
  emit logAppended(line);
}

void MainWindow::updateStatus() {
  const RunState state = runner_.state();
  statusLabel_->setText(QStringLiteral("状态：%1").arg(runStateName(state)));
  const bool running = state == RunState::Running || state == RunState::Cancelling;
  runButton_->setEnabled(!running && versionCompatible_);
  cancelButton_->setEnabled(running);
  terminateButton_->setEnabled(running);
  statusBar()->showMessage(runStateDescription(state));
}

void MainWindow::onStateChanged(RunState state) {
  updateStatus();
  emit runStateChanged(static_cast<int>(state));
}

void MainWindow::onRunFinished(RunState state) {
  updateStatus();
  summaryEdit_->setPlainText(runner_.summary().isEmpty()
                                  ? QStringLiteral("（没有 run_summary）")
                                  : jsonToText(runner_.summary()));
  appendLog(QStringLiteral("运行结束：%1（%2）").arg(runStateName(state), runStateDescription(state)));
  if (!runner_.fatalReason().isEmpty()) {
    appendLog(QStringLiteral("协议/失败原因：%1").arg(runner_.fatalReason()));
  }
  for (const QString& warning : runner_.warnings()) {
    appendLog(QStringLiteral("告警：%1").arg(warning));
  }
  emit runFinished(static_cast<int>(state));
}

void MainWindow::onEventReceived(const AcsdEvent& event) {
  rebuildEventTable();
  if (event.kind == EventKind::BlockProgress && event.total > 0) {
    progressBar_->setRange(0, 100);
    progressBar_->setValue(static_cast<int>((event.completed * 100) / event.total));
  }
  if (event.kind == EventKind::RunFinished) {
    progressBar_->setValue(event.exitCode.has_value() && *event.exitCode == 0 ? 100 : progressBar_->value());
  }
}

void MainWindow::rebuildEventTable() {
  const QSignalBlocker blocker(eventTable_);
  eventTable_->setSortingEnabled(false);
  eventTable_->setRowCount(0);
  const QVector<AcsdEvent>& events = runner_.events();
  int row = 0;
  for (int index = 0; index < events.size(); ++index) {
    const AcsdEvent& event = events.at(index);
    if (!groupFilter_.isEmpty()) {
      if (!event.hasFrameId || event.frameId != groupFilter_) {
        continue;
      }
    }
    eventTable_->insertRow(row);
    auto* seqItem = makeItem(QString::number(event.seq));
    seqItem->setData(Qt::UserRole, index);
    eventTable_->setItem(row, 0, seqItem);
    eventTable_->setItem(row, 1, makeItem(event.valid ? event.kindName : QStringLiteral("<协议错误>")));
    eventTable_->setItem(row, 2, makeItem(event.hasFrameId ? event.frameId : QString()));
    eventTable_->setItem(row, 3, makeItem(event.block.has_value() ? QString::number(*event.block) : QString()));
    eventTable_->setItem(row, 4, makeItem(event.code));
    eventTable_->setItem(
        row, 5,
        makeItem(event.total > 0 ? QStringLiteral("%1/%2").arg(event.completed).arg(event.total)
                                 : QString::number(event.completed)));
    eventTable_->setItem(row, 6, makeItem(event.fatal ? event.error : event.message));
    if (event.kind == EventKind::Issue) {
      for (int column = 0; column < eventTable_->columnCount(); ++column) {
        eventTable_->item(row, column)->setBackground(QColor(255, 244, 214));
      }
    }
    if (event.fatal) {
      for (int column = 0; column < eventTable_->columnCount(); ++column) {
        eventTable_->item(row, column)->setBackground(QColor(255, 214, 214));
      }
    }
    ++row;
  }
}

void MainWindow::refreshInputGroups() {
  if (groupTree_ == nullptr) {
    return;
  }
  const QSignalBlocker blocker(groupTree_);
  groupTree_->clear();
  auto* all = new QTreeWidgetItem(groupTree_);
  all->setText(0, QStringLiteral("全部输入"));
  all->setText(1, QString::number(inputTable_->rowCount()));
  all->setText(2, QStringLiteral("不过滤事件表"));
  all->setData(0, Qt::UserRole, QString());

  const QString command = commandCombo_->currentText();
  QMap<QString, QStringList> groups;
  for (int row = 0; row < inputTable_->rowCount(); ++row) {
    QTableWidgetItem* pathItem = inputTable_->item(row, 0);
    QTableWidgetItem* frameItem = inputTable_->item(row, 2);
    const QString path = pathItem != nullptr ? pathItem->text() : QString();
    QString key;
    if (command == QStringLiteral("normalize")) {
      key = frameItem != nullptr && !frameItem->text().isEmpty() ? frameItem->text()
                                                                : QStringLiteral("（未分组）");
    } else {
      key = QStringLiteral("产品输入");
    }
    groups[key].append(path);
  }
  for (auto it = groups.constBegin(); it != groups.constEnd(); ++it) {
    auto* group = new QTreeWidgetItem(groupTree_);
    group->setText(0, it.key());
    group->setText(1, QString::number(it.value().size()));
    group->setText(2, command == QStringLiteral("normalize") ? QStringLiteral("按 frame_id 分组")
                                                             : QStringLiteral("产品清单"));
    group->setData(0, Qt::UserRole, it.key() == QStringLiteral("（未分组）") ? QString() : it.key());
    for (const QString& path : it.value()) {
      auto* child = new QTreeWidgetItem(group);
      child->setText(0, path);
      child->setText(2, path);
    }
  }
  groupTree_->expandAll();
}

void MainWindow::onGroupSelectionChanged() {
  QTreeWidgetItem* item = groupTree_->currentItem();
  if (item == nullptr) {
    groupFilter_.clear();
    rebuildEventTable();
    return;
  }
  groupFilter_ = item->data(0, Qt::UserRole).toString();
  rebuildEventTable();
}

AcsdConfig MainWindow::currentConfig() const {
  AcsdConfig config;
  config.command = commandCombo_->currentText();
  for (int row = 0; row < inputTable_->rowCount(); ++row) {
    ConfigInput input;
    auto cell = [&](int column) {
      QTableWidgetItem* item = inputTable_->item(row, column);
      return item != nullptr ? item->text().trimmed() : QString();
    };
    input.path = cell(0);
    input.expectedHash = cell(1);
    input.frameId = cell(2);
    input.calibration = cell(3);
    input.response = cell(4);
    config.inputs.append(input);
  }
  config.scientific = scientificEdit_->text().trimmed();
  config.output = outputEdit_->text().trimmed();
  config.catalogue = catalogueEdit_->text().trimmed();
  config.productProfile = profileCombo_->currentText();
  config.resources.bytes = bytesEdit_->text().trimmed().toLongLong();
  config.resources.work = workEdit_->text().trimmed().toLongLong();
  config.resources.spillBytes = spillEdit_->text().trimmed().toLongLong();
  config.resources.threads = threadsSpin_->value();
  config.resources.maxDepth = depthSpin_->value();
  config.resources.maxPrecisionBits = bitsSpin_->value();
  config.resources.scratchDirectory = scratchEdit_->text().trimmed();
  config.publication.present = publicationEnabled_->isChecked();
  config.publication.signalDtype = dtypeCombo_->currentText();
  config.publication.tileLog2Width = tileSpin_->value();
  config.publication.durable = durableCheck_->isChecked();
  return config;
}

void MainWindow::setCurrentConfig(const AcsdConfig& config) {
  applyConfigToWidgets(config);
}

void MainWindow::applyConfigToWidgets(const AcsdConfig& config) {
  const int commandIndex = commandCombo_->findText(config.command);
  if (commandIndex >= 0) {
    commandCombo_->setCurrentIndex(commandIndex);
  }
  const QSignalBlocker inputBlocker(inputTable_);
  inputTable_->setRowCount(0);
  for (const ConfigInput& input : config.inputs) {
    const int row = inputTable_->rowCount();
    inputTable_->insertRow(row);
    inputTable_->setItem(row, 0, new QTableWidgetItem(input.path));
    inputTable_->setItem(row, 1, new QTableWidgetItem(input.expectedHash));
    inputTable_->setItem(row, 2, new QTableWidgetItem(input.frameId));
    inputTable_->setItem(row, 3, new QTableWidgetItem(input.calibration));
    inputTable_->setItem(row, 4, new QTableWidgetItem(input.response));
  }
  scientificEdit_->setText(config.scientific);
  outputEdit_->setText(config.output);
  catalogueEdit_->setText(config.catalogue);
  const int profileIndex = profileCombo_->findText(config.productProfile);
  if (profileIndex >= 0) {
    profileCombo_->setCurrentIndex(profileIndex);
  }
  bytesEdit_->setText(QString::number(config.resources.bytes));
  workEdit_->setText(QString::number(config.resources.work));
  spillEdit_->setText(QString::number(config.resources.spillBytes));
  threadsSpin_->setValue(config.resources.threads);
  depthSpin_->setValue(config.resources.maxDepth);
  bitsSpin_->setValue(config.resources.maxPrecisionBits);
  scratchEdit_->setText(config.resources.scratchDirectory);
  publicationEnabled_->setChecked(config.publication.present);
  const int dtypeIndex = dtypeCombo_->findText(config.publication.signalDtype);
  if (dtypeIndex >= 0) {
    dtypeCombo_->setCurrentIndex(dtypeIndex);
  }
  tileSpin_->setValue(config.publication.tileLog2Width);
  durableCheck_->setChecked(config.publication.durable);
  refreshInputGroups();
}

bool MainWindow::setCliPath(const QString& path, QString* error) {
  if (path.trimmed().isEmpty()) {
    if (error != nullptr) {
      *error = QStringLiteral("CLI 路径为空");
    }
    return false;
  }
  if (!QFileInfo::exists(path)) {
    if (error != nullptr) {
      *error = QStringLiteral("CLI 不存在：%1").arg(path);
    }
    return false;
  }
  cliPathEdit_->setText(path);
  return true;
}

QString MainWindow::cliPath() const { return cliPathEdit_->text().trimmed(); }

QString MainWindow::configFilePath() const { return configFilePath_; }

void MainWindow::setWorkingDirectory(const QString& directory) {
  if (!directory.trimmed().isEmpty()) {
    workingDirEdit_->setText(directory);
  }
}

bool MainWindow::loadConfigFile(const QString& path, QString* error) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    if (error != nullptr) {
      *error = QStringLiteral("无法读取配置 %1：%2").arg(path, file.errorString());
    }
    return false;
  }
  const QByteArray payload = file.readAll();
  file.close();

  QJsonParseError perr{};
  const QJsonDocument document = QJsonDocument::fromJson(payload, &perr);
  if (perr.error != QJsonParseError::NoError || !document.isObject()) {
    if (error != nullptr) {
      *error = QStringLiteral("配置不是合法 JSON 对象：%1").arg(perr.errorString());
    }
    return false;
  }
  const QJsonObject object = document.object();

  // 版本探测：配置 major 不兼容时拒绝加载（不启动）。
  const ConfigVersion version = parseConfigSchema(object);
  if (!version.ok) {
    if (error != nullptr) {
      *error = version.detail;
    }
    appendLog(QStringLiteral("拒绝加载配置：%1").arg(version.detail));
    return false;
  }
  if (version.major != kConfigMajor) {
    const QString reason = QStringLiteral("配置 schema major %1 与 GUI 支持的 %2 不兼容，拒绝启动")
                               .arg(version.major)
                               .arg(kConfigMajor);
    if (error != nullptr) {
      *error = reason;
    }
    appendLog(reason);
    return false;
  }

  AcsdConfig config;
  QString parseError;
  if (!AcsdConfig::fromJson(object, &config, &parseError)) {
    if (error != nullptr) {
      *error = parseError;
    }
    return false;
  }
  applyConfigToWidgets(config);
  configFilePath_ = path;
  configPathEdit_->setText(path);
  const QDir dir = QFileInfo(path).absoluteDir();
  if (workingDirEdit_->text().isEmpty()) {
    workingDirEdit_->setText(dir.absolutePath());
  }
  const ConfigValidation validation = validateConfig(config);
  QStringList messages;
  for (const ConfigDiagnostic& diagnostic : validation.diagnostics) {
    messages.append(QStringLiteral("[%1] %2：%3").arg(diagnostic.level, diagnostic.field, diagnostic.message));
  }
  configValidationLabel_->setText(messages.isEmpty()
                                      ? QStringLiteral("配置结构校验通过")
                                      : messages.join(QStringLiteral("\n")));
  appendLog(QStringLiteral("已加载配置：%1").arg(path));
  if (validation.hasError) {
    if (error != nullptr) {
      *error = messages.join(QStringLiteral("; "));
    }
    return false;
  }
  return true;
}

bool MainWindow::saveConfigFile(const QString& path, QString* error) {
  const AcsdConfig config = currentConfig();
  const ConfigValidation validation = validateConfig(config);
  if (validation.hasError) {
    QStringList messages;
    for (const ConfigDiagnostic& diagnostic : validation.diagnostics) {
      if (diagnostic.level == QStringLiteral("error")) {
        messages.append(QStringLiteral("%1：%2").arg(diagnostic.field, diagnostic.message));
      }
    }
    if (error != nullptr) {
      *error = messages.join(QStringLiteral("; "));
    }
    return false;
  }
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    if (error != nullptr) {
      *error = QStringLiteral("无法写入 %1：%2").arg(path, file.errorString());
    }
    return false;
  }
  const QByteArray payload = QJsonDocument(config.toJson()).toJson(QJsonDocument::Indented);
  file.write(payload);
  file.close();
  configFilePath_ = path;
  configPathEdit_->setText(path);
  return true;
}

bool MainWindow::writeRunConfig(QString* path, QString* error) const {
  const AcsdConfig config = currentConfig();
  const ConfigValidation validation = validateConfig(config);
  if (validation.hasError) {
    if (error != nullptr) {
      *error = QStringLiteral("配置校验未通过：%1").arg(
          validation.diagnostics.isEmpty() ? QString() : validation.diagnostics.first().message);
    }
    return false;
  }
  // 运行用配置写在用户配置文件同目录，保证相对路径解析一致。
  QDir targetDir;
  if (!configFilePath_.isEmpty()) {
    targetDir = QFileInfo(configFilePath_).absoluteDir();
  } else if (!workingDirEdit_->text().trimmed().isEmpty()) {
    targetDir = QDir(workingDirEdit_->text().trimmed());
  } else {
    targetDir = QDir::current();
  }
  const QString runPath = targetDir.filePath(QStringLiteral("acsd-gui-run-config.json"));
  QFile file(runPath);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    if (error != nullptr) {
      *error = QStringLiteral("无法写入运行配置 %1：%2").arg(runPath, file.errorString());
    }
    return false;
  }
  file.write(QJsonDocument(config.toJson()).toJson(QJsonDocument::Indented));
  file.close();
  *path = runPath;
  return true;
}

bool MainWindow::checkVersionForRun(QString* reason) const {
  if (!versionCompatible_) {
    if (reason != nullptr) {
      *reason = versionReason_;
    }
    return false;
  }
  const AcsdConfig config = currentConfig();
  const ConfigVersion version = parseConfigSchema(config.toJson());
  if (!version.ok) {
    if (reason != nullptr) {
      *reason = version.detail;
    }
    return false;
  }
  if (version.major != kConfigMajor) {
    if (reason != nullptr) {
      *reason = QStringLiteral("配置 schema major %1 与 GUI 支持的 %2 不兼容，拒绝启动")
                    .arg(version.major)
                    .arg(kConfigMajor);
    }
    return false;
  }
  return true;
}

ProbeResult MainWindow::probeVersion() {
  lastProbe_ = probeCliVersion(cliPath(), workingDirEdit_->text().trimmed());
  versionCompatible_ = true;
  versionReason_.clear();
  if (!lastProbe_.executed) {
    versionReason_ = lastProbe_.error;
    versionCompatible_ = false;
    versionLabel_->setText(QStringLiteral("版本：探测失败"));
    appendLog(QStringLiteral("版本探测失败：%1").arg(lastProbe_.error));
    updateStatus();
    return lastProbe_;
  }
  if (lastProbe_.version.eventMajor.has_value() && *lastProbe_.version.eventMajor != kEventMajor) {
    versionCompatible_ = false;
    versionReason_ = QStringLiteral("事件协议 major %1 与 GUI 支持的 %2 不兼容，拒绝启动")
                         .arg(*lastProbe_.version.eventMajor)
                         .arg(kEventMajor);
    versionLabel_->setText(QStringLiteral("版本：不兼容"));
    appendLog(versionReason_);
    updateStatus();
    return lastProbe_;
  }
  if (lastProbe_.version.eventMajor.has_value()) {
    versionLabel_->setText(QStringLiteral("版本：acsd.events/%1（%2）")
                               .arg(*lastProbe_.version.eventMajor)
                               .arg(lastProbe_.version.productVersion.isEmpty()
                                        ? QStringLiteral("未给出产品版本")
                                        : lastProbe_.version.productVersion));
    appendLog(QStringLiteral("版本探测：%1").arg(lastProbe_.version.detail));
  } else {
    versionLabel_->setText(QStringLiteral("版本：未知（以事件流协议为准）"));
    appendLog(QStringLiteral("版本探测未给出事件协议 major，运行时以首个事件的 protocol 为准"));
  }
  updateStatus();
  return lastProbe_;
}

bool MainWindow::startRun(QString* error) {
  if (runner_.state() == RunState::Running || runner_.state() == RunState::Cancelling) {
    if (error != nullptr) {
      *error = QStringLiteral("已有运行在进行中");
    }
    return false;
  }
  QString reason;
  if (!checkVersionForRun(&reason)) {
    if (error != nullptr) {
      *error = reason;
    }
    appendLog(QStringLiteral("拒绝启动：%1").arg(reason));
    return false;
  }
  QString runConfigPath;
  if (!writeRunConfig(&runConfigPath, error)) {
    return false;
  }
  eventTable_->setRowCount(0);
  summaryEdit_->clear();
  detailEdit_->clear();
  progressBar_->setValue(0);
  groupFilter_.clear();
  const QString command = currentConfig().command;
  const QString workDir = workingDirEdit_->text().trimmed();
  if (!runner_.start(cliPath(), command, runConfigPath, workDir, error)) {
    return false;
  }
  appendLog(QStringLiteral("运行配置：%1").arg(runConfigPath));
  updateStatus();
  return true;
}

bool MainWindow::cancelRun() {
  const bool sent = runner_.requestCancel();
  updateStatus();
  return sent;
}

void MainWindow::forceTerminate() {
  runner_.forceTerminate();
  updateStatus();
}

void MainWindow::setProcessEnvironmentVariable(const QString& key, const QString& value) {
  QProcessEnvironment environment = runner_.processEnvironment();
  environment.insert(key, value);
  runner_.setProcessEnvironment(environment);
}

QJsonObject MainWindow::evidenceSnapshot() const {
  QJsonObject snapshot;
  snapshot.insert(QStringLiteral("cli"), cliPath());
  snapshot.insert(QStringLiteral("config_file"), configFilePath_);
  snapshot.insert(QStringLiteral("command"), currentConfig().command);
  snapshot.insert(QStringLiteral("state"), runStateName(runner_.state()));
  snapshot.insert(QStringLiteral("state_code"), static_cast<int>(runner_.state()));
  snapshot.insert(QStringLiteral("state_description"), runStateDescription(runner_.state()));
  snapshot.insert(QStringLiteral("run_id"), runner_.runId());
  if (runner_.exitCode().has_value()) {
    snapshot.insert(QStringLiteral("exit_code"), *runner_.exitCode());
  }
  snapshot.insert(QStringLiteral("process_exit_code"), runner_.processExitCode());
  snapshot.insert(QStringLiteral("cancel_sent"), runner_.cancelSent());
  snapshot.insert(QStringLiteral("cancel_acknowledged"), runner_.cancelAcknowledged());
  snapshot.insert(QStringLiteral("fatal_reason"), runner_.fatalReason());
  snapshot.insert(QStringLiteral("summary"), runner_.summary());
  snapshot.insert(QStringLiteral("version_probe_detail"), lastProbe_.version.detail);
  if (lastProbe_.version.eventMajor.has_value()) {
    snapshot.insert(QStringLiteral("version_event_major"), *lastProbe_.version.eventMajor);
  }
  QJsonArray events;
  for (const AcsdEvent& event : runner_.events()) {
    QJsonObject item;
    item.insert(QStringLiteral("seq"), event.seq);
    item.insert(QStringLiteral("event"), event.kindName);
    item.insert(QStringLiteral("valid"), event.valid);
    item.insert(QStringLiteral("fatal"), event.fatal);
    item.insert(QStringLiteral("error"), event.error);
    item.insert(QStringLiteral("run_id"), event.runId);
    if (event.hasFrameId) {
      item.insert(QStringLiteral("frame_id"), event.frameId);
    }
    if (event.block.has_value()) {
      item.insert(QStringLiteral("block"), *event.block);
    }
    item.insert(QStringLiteral("code"), event.code);
    item.insert(QStringLiteral("completed"), event.completed);
    item.insert(QStringLiteral("total"), event.total);
    item.insert(QStringLiteral("message"), event.message);
    if (event.hasAssessment) {
      item.insert(QStringLiteral("assessment"), event.assessment);
    }
    if (event.exitCode.has_value()) {
      item.insert(QStringLiteral("exit_code"), *event.exitCode);
    }
    if (event.hasSummary) {
      item.insert(QStringLiteral("summary"), event.summary);
    }
    events.append(item);
  }
  snapshot.insert(QStringLiteral("events"), events);
  return snapshot;
}

}  // namespace gui
}  // namespace acsd
