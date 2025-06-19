#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "mymessagebox.h"
#include <QProcess>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QProgressBar>
#include <QTextStream>
#include <QApplication>
#include <QDebug>
#include <QDateTime>
#include <QElapsedTimer>
#include <QRegularExpression>
#include "worker.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QThread>
#include "savedsettingdialog.h"
#if defined(Q_OS_WIN)
#include <windows.h>
#endif

// 统计目录大小递归函数
static double dirSizeMB(const QString &path) {
    QDir dir(path);
    double size = 0;
    if (!dir.exists()) return 0.0;
    QFileInfoList list = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
    for (const QFileInfo &info : list) {
        if (info.isDir()) {
            size += dirSizeMB(info.absoluteFilePath());
        } else {
            size += info.size();
        }
    }
    return size / (1024.0 * 1024.0);
}

MainWindow::MainWindow(QWidget *parent, bool isDevelop)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , isDevelopMode(isDevelop)
{
    qDebug() << "[MainWindow] Constructed, this=" << this << ", thread=" << QThread::currentThread();
    ui->setupUi(this);
    setWindowTitle("MMNET");
    connect(ui->pushButton_3, &QPushButton::clicked, this, &MainWindow::handleRunModelClicked);
    qDebug() << "[MainWindow] connect pushButton_3 -> handleRunModelClicked";
    
    // 初始化进度监控定时器
    progressTimer = new QTimer(this);
    connect(progressTimer, &QTimer::timeout, this, &MainWindow::updateProgressFromLog);
    // 默认禁用下载结果按钮
    ui->pushButton_download_pred->setEnabled(false);
    // 默认隐藏进度条
    ui->progressBar_step2->setVisible(false);

    // 启动时刷新多选框
    refreshPhenotypeOptions();
}

MainWindow::~MainWindow()
{
    qDebug() << "[MainWindow] Destructor called, this=" << this << ", thread=" << QThread::currentThread();
    if (progressTimer) {
        progressTimer->stop();
    }
    delete ui;
}

// 上传基因文件
void MainWindow::on_pushButton_clicked()
{
    uploadFiles("MMNET/data/gene", "基因");
}

// 上传表型文件  
void MainWindow::on_pushButton_2_clicked()
{
    uploadFiles("MMNET/data/phen", "表型");
    refreshPhenotypeOptions();
}

// 上传待预测文件
void MainWindow::on_pushButton_5_clicked()
{
    uploadFiles("MMNET/data/pred", "待预测");
}

// 第二步训练模型
void MainWindow::handleRunModelClicked()
{
    if (isStep2Running) {
        qDebug() << "[MainWindow] handleRunModelClicked: already running, ignore.";
        return;
    }
    if (selectedPhenotypes.isEmpty()) {
        QMessageBox::warning(this, tr("错误"), tr("请先选择一个或多个表型文件！"));
        return;
    }
    ui->pushButton_3->setEnabled(false);
    // 初始化队列和参数map
    pendingPhenotypes = selectedPhenotypes;
    phenotypeSettings.clear();
    showNextSettingDialog();
}

void MainWindow::showNextSettingDialog()
{
    if (pendingPhenotypes.isEmpty()) {
        // 优化：如果没有任何表型被设置（全部被取消），不进入训练
        if (phenotypeSettings.isEmpty()) {
            ui->pushButton_3->setEnabled(true);
            return;
        }
        // 全部设置完毕，开始训练
        startTrainingForPhenotypes();
        return;
    }
    QString phenotype = pendingPhenotypes.takeFirst();
    // 读取json参数
    QString esnJson = QDir::currentPath() + "/MMNET/configs/ESN.json";
    QString mmnetJson = QDir::currentPath() + "/MMNET/configs/MMNet.json";
    QFile esnFile(esnJson);
    QFile mmnetFile(mmnetJson);
    int esnBatch = 128, esnSaved = 100;
    double esnP = 0.8;
    int mmnetBatch = 128, mmnetSaved = 100;
    double mmnetP1 = 0.8, mmnetP2 = 0.8, mmnetP3 = 0.8, mmnetP4 = 0.6, mmnetWd = 1e-5;
    // 读取ESN.json
    if (esnFile.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(esnFile.readAll());
        QJsonObject obj = doc.object();
        if (obj.contains(phenotype) && obj[phenotype].isObject()) {
            QJsonObject phenoObj = obj[phenotype].toObject();
            if (phenoObj.contains("batch size")) esnBatch = phenoObj["batch size"].toInt();
            if (phenoObj.contains("p")) esnP = phenoObj["p"].toDouble();
            if (phenoObj.contains("saved")) esnSaved = phenoObj["saved"].toInt();
        }
        esnFile.close();
    }
    // 读取MMNet.json
    if (mmnetFile.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(mmnetFile.readAll());
        QJsonObject obj = doc.object();
        if (obj.contains(phenotype) && obj[phenotype].isObject()) {
            QJsonObject phenoObj = obj[phenotype].toObject();
            if (phenoObj.contains("batch size")) mmnetBatch = phenoObj["batch size"].toInt();
            if (phenoObj.contains("p1")) mmnetP1 = phenoObj["p1"].toDouble();
            if (phenoObj.contains("p2")) mmnetP2 = phenoObj["p2"].toDouble();
            if (phenoObj.contains("p3")) mmnetP3 = phenoObj["p3"].toDouble();
            if (phenoObj.contains("p4")) mmnetP4 = phenoObj["p4"].toDouble();
            if (phenoObj.contains("saved")) mmnetSaved = phenoObj["saved"].toInt();
            if (phenoObj.contains("wd")) mmnetWd = phenoObj["wd"].toDouble();
        }
        mmnetFile.close();
    }
    // 弹出设置参数的对话框（异步）
    SavedSettingDialog *dlg = new SavedSettingDialog(this);
    dlg->setPhenotype(phenotype);
    dlg->setEsnValues(esnBatch, esnP, esnSaved);
    dlg->setMmnetValues(mmnetBatch, mmnetP1, mmnetP2, mmnetP3, mmnetP4, mmnetSaved, mmnetWd);
    connect(dlg, &QDialog::accepted, this, [this, phenotype, dlg, &esnBatch, &esnP, &esnSaved, &mmnetBatch, &mmnetP1, &mmnetP2, &mmnetP3, &mmnetP4, &mmnetSaved, &mmnetWd]() {
        dlg->getEsnValues(esnBatch, esnP, esnSaved);
        dlg->getMmnetValues(mmnetBatch, mmnetP1, mmnetP2, mmnetP3, mmnetP4, mmnetSaved, mmnetWd);
        PhenotypeSetting setting;
        setting.esnBatch = esnBatch;
        setting.esnP = esnP;
        setting.esnSaved = esnSaved;
        setting.mmnetBatch = mmnetBatch;
        setting.mmnetP1 = mmnetP1;
        setting.mmnetP2 = mmnetP2;
        setting.mmnetP3 = mmnetP3;
        setting.mmnetP4 = mmnetP4;
        setting.mmnetSaved = mmnetSaved;
        setting.mmnetWd = mmnetWd;
        phenotypeSettings[phenotype] = setting;
        dlg->deleteLater();
        showNextSettingDialog();
    });
    connect(dlg, &QDialog::rejected, this, [dlg, this]() {
        dlg->deleteLater();
        showNextSettingDialog();
    });
    dlg->open(); // 异步弹窗
}

void MainWindow::startTrainingForPhenotypes()
{
    // 1. 先整体读取ESN.json和MMNet.json
    QString esnJson = QDir::currentPath() + "/MMNET/configs/ESN.json";
    QString mmnetJson = QDir::currentPath() + "/MMNET/configs/MMNet.json";
    QFile esnFile(esnJson);
    QFile mmnetFile(mmnetJson);
    QJsonObject esnObj, mmnetObj;
    if (esnFile.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(esnFile.readAll());
        esnObj = doc.object();
        esnFile.close();
    }
    if (mmnetFile.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(mmnetFile.readAll());
        mmnetObj = doc.object();
        mmnetFile.close();
    }
    // 2. 循环所有表型，修改对象
    for (auto it = phenotypeSettings.begin(); it != phenotypeSettings.end(); ++it) {
        const QString &phenotype = it.key();
        const PhenotypeSetting &setting = it.value();
        // 修改esnObj
        QJsonObject phenoObj = esnObj.value(phenotype).toObject();
        phenoObj["batch size"] = setting.esnBatch;
        phenoObj["p"] = setting.esnP;
        phenoObj["saved"] = setting.esnSaved;
        esnObj[phenotype] = phenoObj;
        // 修改mmnetObj
        QJsonObject mmnetPhenoObj = mmnetObj.value(phenotype).toObject();
        mmnetPhenoObj["batch size"] = setting.mmnetBatch;
        mmnetPhenoObj["p1"] = setting.mmnetP1;
        mmnetPhenoObj["p2"] = setting.mmnetP2;
        mmnetPhenoObj["p3"] = setting.mmnetP3;
        mmnetPhenoObj["p4"] = setting.mmnetP4;
        mmnetPhenoObj["saved"] = setting.mmnetSaved;
        mmnetPhenoObj["wd"] = setting.mmnetWd;
        mmnetObj[phenotype] = mmnetPhenoObj;
    }
    // 3. 一次性写回
    if (esnFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QJsonDocument newDoc(esnObj);
        esnFile.write(newDoc.toJson(QJsonDocument::Indented));
        esnFile.close();
    }
    if (mmnetFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QJsonDocument newDoc(mmnetObj);
        mmnetFile.write(newDoc.toJson(QJsonDocument::Indented));
        mmnetFile.close();
    }
    // 4. 初始化训练队列和结果，开始逐个训练
    trainPhenoQueue.clear();
    trainResultMsgs.clear();
    for (auto it = phenotypeSettings.begin(); it != phenotypeSettings.end(); ++it) {
        trainPhenoQueue << it.key();
    }
    trainNextPhenotype();
}

void MainWindow::trainNextPhenotype()
{
    if (trainPhenoQueue.isEmpty()) {
        showTrainSummary();
        return;
    }
    const QString phenotype = trainPhenoQueue.takeFirst();
    ui->progressBar_step2->setFormat(tr("训练进度（%1）：%p%").arg(phenotype));
    // 训练流程（原for循环剩余部分）
    isStep2Running = true;
    static int callCount = 0;
    ++callCount;
    qDebug() << "[MainWindow] handleRunModelClicked called, count=" << callCount << ", this=" << this << ", thread=" << QThread::currentThread();
    // 显示进度条并强制刷新
    ui->progressBar_step2->setVisible(true);
    ui->progressBar_step2->repaint();
    QApplication::processEvents();
    // 创建空的日志文件
    QFile step1Log(QDir::currentPath() + "/MMNET/step1.log");
    QFile step2Log(QDir::currentPath() + "/MMNET/step2.log");
    if (step1Log.exists()) step1Log.remove();
    if (step2Log.exists()) step2Log.remove();
    if (!step1Log.open(QIODevice::WriteOnly)) {
        trainResultMsgs << phenotype + tr(": 无法创建 step1.log 文件");
        trainNextPhenotype();
        return;
    }
    step1Log.close();
    if (!step2Log.open(QIODevice::WriteOnly)) {
        trainResultMsgs << phenotype + tr(": 无法创建 step2.log 文件");
        trainNextPhenotype();
        return;
    }
    step2Log.close();
    // 路径准备
    QString exePath1 = QDir::currentPath() + "/MMNET/generate_genetic_relatedness.exe";
    QString exePath2 = QDir::currentPath() + "/MMNET/train_mmnet.exe";
    qDebug() << "Checking exe files:";
    qDebug() << "exePath1 exists:" << QFile::exists(exePath1) << ", path:" << exePath1;
    qDebug() << "exePath2 exists:" << QFile::exists(exePath2) << ", path:" << exePath2;
    QString log1 = QDir::currentPath() + "/MMNET/step1.log";
    QString log2 = QDir::currentPath() + "/MMNET/step2.log";
    QString json1 = QDir::currentPath() + "/MMNET/configs/ESN.json";
    QString json2 = QDir::currentPath() + "/MMNET/configs/MMNet.json";
    if (workerThread) {
        qDebug() << "[MainWindow] Deleting old workerThread, thread=" << workerThread;
        workerThread->quit();
        workerThread->wait();
        delete workerThread;
        workerThread = nullptr;
    }
    workerThread = new QThread(this);
    worker = new Worker();
    qDebug() << "[MainWindow] New Worker created, worker=" << worker << ", workerThread=" << workerThread;
    worker->setParams(exePath1, exePath2, log1, log2, json1, json2, phenotype);
    worker->setMainWindow(this);
    worker->moveToThread(workerThread);
    connect(workerThread, &QThread::started, worker, &Worker::run);
    connect(worker, &Worker::finished, this, &MainWindow::step2Finished, Qt::QueuedConnection);
    connect(worker, &Worker::finished, workerThread, &QThread::quit);
    connect(workerThread, &QThread::finished, worker, &QObject::deleteLater);
    ui->progressBar_step2->setValue(0);
    step2StartTime = QDateTime::currentDateTime();
    workerThread->start();
    // 等待训练完成（同步）
    while (workerThread->isRunning()) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
    }
    // 训练完成后重命名mmnet.pt
    QString ptFile = QDir::currentPath() + "/MMNET/mmnet.pt";
    QString ptTarget = QDir::currentPath() + QString("/MMNET/%1_mmnet.pt").arg(phenotype);
    if (QFile::exists(ptFile)) {
        if (QFile::exists(ptTarget)) QFile::remove(ptTarget);
        QFile::rename(ptFile, ptTarget);
    }
    // step2Finished会自动推进下一个
}

void MainWindow::step2Finished(bool success, const QString &msg, double seconds, double exe1Seconds, double exe2Seconds,
                               const QDateTime &step1Start, const QDateTime &step1End, 
                               const QDateTime &step2Start, const QDateTime &step2End) {
    qDebug() << "[MainWindow] step2Finished called, this=" << this << ", thread=" << QThread::currentThread();
    isStep2Running = false;
    ui->pushButton_3->setEnabled(true); // 恢复按钮
    if (success) {
        ui->progressBar_step2->setValue(100);
    }
    // 解析step2.log最后一行的决定系数
    QString step2LogPath = QDir::currentPath() + "/MMNET/step2.log";
    QString lastLine;
    QFile step2Log(step2LogPath);
    if (step2Log.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&step2Log);
        while (!in.atEnd()) {
            QString line = in.readLine();
            if (!line.trimmed().isEmpty()) lastLine = line;
        }
        step2Log.close();
    }
    qDebug() << "[Debug] step2.log lastLine:" << lastLine;
    QString trainR2, valR2;
    QRegularExpression reTrain("train_R2\\s*=\\s*([\\d\\.\\-eE]+)");
    QRegularExpression reVal("val_R2\\s*=\\s*([\\d\\.\\-eE]+)");
    auto mTrain = reTrain.match(lastLine);
    auto mVal = reVal.match(lastLine);
    if (mTrain.hasMatch()) trainR2 = mTrain.captured(1);
    if (mVal.hasMatch()) valR2 = mVal.captured(1);
    qDebug() << "[Debug] trainR2:" << trainR2 << ", valR2:" << valR2;
    QString r2Msg;
    if (!trainR2.isEmpty() && !valR2.isEmpty()) {
        r2Msg = QString("\n决定系数 R²：训练集 %1，验证集 %2").arg(trainR2).arg(valR2);
    } else {
        r2Msg = "\n决定系数 R²：未能解析到train_R2/val_R2";
    }
    // 失败时不强制100%，保留最后进度
    QString timeMsg;
    auto formatTime = [](double t) {
        if (t >= 60.0) return QString("%1 分钟").arg(t / 60.0, 0, 'f', 2);
        else return QString("%1 秒").arg(t, 0, 'f', 2);
    };
    if (seconds >= 60.0) {
        double minutes = seconds / 60.0;
        timeMsg = QString("\n本次运行耗时：%1\n第一步: %2\n第二步: %3")
            .arg(QString::number(minutes, 'f', 2) + " 分钟")
            .arg(formatTime(exe1Seconds))
            .arg(formatTime(exe2Seconds));
    } else {
        timeMsg = QString("\n本次运行耗时：%1\n第一步: %2\n第二步: %3")
            .arg(formatTime(seconds))
            .arg(formatTime(exe1Seconds))
            .arg(formatTime(exe2Seconds));
    }
    // 收集本次训练结果
    QString summary = (success ? tr("[成功]") : tr("[失败]")) + r2Msg + timeMsg;
    trainResultMsgs << summary;
    // 写入日志等原有逻辑...
    // ...（省略原有日志写入代码）...
    // 自动训练下一个
    trainNextPhenotype();
}

void MainWindow::showTrainSummary()
{
    QString msg = tr("所有表型训练已完成！\n\n");
    for (int i = 0; i < trainResultMsgs.size(); ++i) {
        msg += phenotypeSettings.keys().at(i) + ":\n" + trainResultMsgs.at(i) + "\n\n";
    }
    MyMessageBox msgBox(this);
    msgBox.setMySize(500, 300);
    msgBox.setIcon(QMessageBox::Information);
    msgBox.setWindowTitle(tr("训练完成"));
    msgBox.setText(msg);
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.exec();
    QTimer::singleShot(0, this, [this](){ ui->pushButton_3->setEnabled(true); });
}

void MainWindow::on_pushButton_4_clicked()
{
    if (selectedPhenotypes.isEmpty()) {
        QMessageBox::warning(this, tr("错误"), tr("请先选择一个或多个表型文件！"));
        return;
    }
    // 优化：点击后立即显示"预测中..."并禁用按钮，防止重复点击
    ui->pushButton_4->setText(tr("预测中..."));
    ui->pushButton_4->setEnabled(false);
    predictPhenoQueue = selectedPhenotypes;
    predictResultMsgs.clear();
    predictNextPhenotype();
}

void MainWindow::predictNextPhenotype()
{
    if (predictPhenoQueue.isEmpty()) {
        showPredictSummary();
        return;
    }
    currentPredictPhenotype = predictPhenoQueue.takeFirst();
    // 检查是否有待预测文件
    QString predDir = QDir::currentPath() + "/MMNET/data/pred";
    QDir dir(predDir);
    QStringList filters;
    filters << currentPredictPhenotype + ".csv" << currentPredictPhenotype + ".pt" << currentPredictPhenotype + ".xls" << currentPredictPhenotype + ".xlsx";
    QStringList found = dir.entryList(filters, QDir::Files);
    if (found.isEmpty()) {
        predictResultMsgs << currentPredictPhenotype + tr(": 未找到待预测文件");
        QTimer::singleShot(0, this, &MainWindow::predictNextPhenotype);
        return;
    }
    // 运行 pred.exe，指定模型和输出
    QString exePath = QDir::currentPath() + "/MMNET/pred.exe";
    QString modelPath = QDir::currentPath() + QString("/MMNET/saved/%1_mmnet.pt").arg(currentPredictPhenotype);
    QString outputPath = QDir::currentPath() + QString("/MMNET/%1_MMNet_pred.csv").arg(currentPredictPhenotype);
    if (!QFile::exists(exePath)) {
        predictResultMsgs << currentPredictPhenotype + tr(": 找不到 pred.exe");
        QTimer::singleShot(0, this, &MainWindow::predictNextPhenotype);
        return;
    }
    if (!QFile::exists(modelPath)) {
        predictResultMsgs << currentPredictPhenotype + tr(": 找不到模型文件");
        QTimer::singleShot(0, this, &MainWindow::predictNextPhenotype);
        return;
    }
    // 异步QProcess
    if (predictProcess) {
        predictProcess->deleteLater();
        predictProcess = nullptr;
    }
    predictProcess = new QProcess(this);
    predictProcess->setWorkingDirectory(QDir::currentPath() + "/MMNET");
#if defined(Q_OS_WIN)
    if (!isDevelopMode) {
        predictProcess->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *args) {
            args->flags |= CREATE_NO_WINDOW;
        });
    }
#endif
    QStringList args;
    args << "--phenotype" << currentPredictPhenotype;
    connect(predictProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this](int exitCode, QProcess::ExitStatus exitStatus) {
        if (exitStatus != QProcess::NormalExit || exitCode != 0) {
            predictResultMsgs << currentPredictPhenotype + tr(": pred.exe 运行失败");
        } else {
            predictResultMsgs << currentPredictPhenotype + tr(": 预测完成！");
        }
        QTimer::singleShot(0, this, &MainWindow::predictNextPhenotype);
    });
    predictProcess->start(exePath, args);
}

void MainWindow::showPredictSummary()
{
    QString msg = tr("所有表型预测已完成！\n\n");
    for (const QString &line : predictResultMsgs) {
        msg += line + "\n";
    }
    MyMessageBox msgBox(this);
    msgBox.setMySize(500, 300);
    msgBox.setIcon(QMessageBox::Information);
    msgBox.setWindowTitle(tr("预测完成"));
    msgBox.setText(msg);
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.exec();
    // 恢复按钮文本和可用状态
    ui->pushButton_4->setText(tr("开始预测"));
    ui->pushButton_4->setEnabled(true);
    ui->pushButton_download_pred->setEnabled(true);
}

void MainWindow::refreshPhenotypeOptions() {
    qDebug() << "[refreshPhenotypeOptions] called";
    // 清空旧的
    QLayout *layout = ui->groupBox_step2->layout();
    if (!layout) { qDebug() << "[refreshPhenotypeOptions] no layout found"; return; }
    // 查找并移除旧的checkbox布局
    QVBoxLayout *vLayout = qobject_cast<QVBoxLayout*>(ui->groupBox_step2->layout());
    if (vLayout) {
        for (int i = vLayout->count() - 1; i >= 0; --i) {
            QLayoutItem *item = vLayout->itemAt(i);
            QHBoxLayout *oldCheckLayout = qobject_cast<QHBoxLayout*>(item ? item->layout() : nullptr);
            if (oldCheckLayout) {
                // 检查是否全是QCheckBox
                bool allCheck = true;
                for (int j = 0; j < oldCheckLayout->count(); ++j) {
                    QWidget *w = oldCheckLayout->itemAt(j) ? oldCheckLayout->itemAt(j)->widget() : nullptr;
                    if (!w || !qobject_cast<QCheckBox*>(w)) { allCheck = false; break; }
                }
                if (allCheck) {
                    QLayoutItem *removed = vLayout->takeAt(i);
                    delete removed->layout();
                    qDebug() << "[refreshPhenotypeOptions] removed old checkLayout at" << i;
                }
            }
        }
    }
    phenotypeCheckBoxes.clear();
    selectedPhenotypes.clear();
    // 遍历MMNET/data/phen目录
    QString phenDir = QDir::currentPath() + "/MMNET/data/phen";
    QDir dir(phenDir);
    QStringList filters;
    filters << "*.csv" << "*.pt" << "*.xls" << "*.xlsx";
    QFileInfoList fileList = dir.entryInfoList(filters, QDir::Files | QDir::NoDotAndDotDot);
    qDebug() << "[refreshPhenotypeOptions] found" << fileList.size() << "phenotype files";
    // 创建新的多选框布局
    QHBoxLayout *checkLayout = new QHBoxLayout();
    for (const QFileInfo &info : fileList) {
        QString baseName = info.completeBaseName();
        QCheckBox *check = new QCheckBox(baseName, ui->groupBox_step2);
        phenotypeCheckBoxes.append(check);
        checkLayout->addWidget(check);
        connect(check, &QCheckBox::checkStateChanged, this, &MainWindow::onPhenotypeSelected);
        qDebug() << "[refreshPhenotypeOptions] add QCheckBox for" << baseName;
    }
    // 插入到Run Model按钮上方
    if (vLayout) {
        vLayout->insertLayout(0, checkLayout);
        qDebug() << "[refreshPhenotypeOptions] insert checkLayout at top of vLayout";
    }
    // 启用/禁用Run Model按钮
    ui->pushButton_3->setEnabled(!fileList.isEmpty());
}

void MainWindow::onPhenotypeSelected() {
    selectedPhenotypes.clear();
    for (QCheckBox *check : phenotypeCheckBoxes) {
        if (check->isChecked()) {
            selectedPhenotypes.append(check->text());
        }
    }
    ui->pushButton_3->setEnabled(!selectedPhenotypes.isEmpty());
}

void MainWindow::on_pushButton_download_pred_clicked()
{
    if (selectedPhenotypes.isEmpty()) {
        QMessageBox::warning(this, tr("错误"), tr("请先选择一个或多个表型文件！"));
        return;
    }
    QString dirPath = QFileDialog::getExistingDirectory(this, tr("选择保存目录"), QDir::homePath());
    if (dirPath.isEmpty()) return;
    QStringList successFiles, failedFiles;
    for (const QString &phenotype : selectedPhenotypes) {
        QString srcFile = QDir::currentPath() + QString("/MMNET/%1_MMNet_pred.csv").arg(phenotype);
        QString savePath = dirPath + QString("/%1_MMNet_pred.csv").arg(phenotype);
        if (!QFile::exists(srcFile)) {
            failedFiles << phenotype;
            continue;
        }
        if (QFile::exists(savePath)) QFile::remove(savePath);
        if (QFile::copy(srcFile, savePath)) {
            successFiles << phenotype;
        } else {
            failedFiles << phenotype;
        }
    }
    QString msg;
    if (!successFiles.isEmpty()) {
        msg += tr("成功保存以下表型结果：\n") + successFiles.join(", ") + "\n";
    }
    if (!failedFiles.isEmpty()) {
        msg += tr("以下表型结果文件未找到或保存失败：\n") + failedFiles.join(", ");
    }
    MyMessageBox msgBox(this);
    msgBox.setMySize(400, 220);
    msgBox.setIcon(failedFiles.isEmpty() ? QMessageBox::Information : QMessageBox::Warning);
    msgBox.setWindowTitle(tr("下载结果"));
    msgBox.setText(msg);
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.exec();
}

bool MainWindow::updateSavedValue(const QString &jsonPath, const QString &phenotype, int savedValue)
{
    QFile file(jsonPath);
    if (!file.open(QIODevice::ReadOnly)) return false;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    if (!doc.isObject()) return false;
    QJsonObject obj = doc.object();
    if (!obj.contains(phenotype) || !obj[phenotype].isObject()) return false;
    QJsonObject phenoObj = obj[phenotype].toObject();
    phenoObj["saved"] = savedValue;
    obj[phenotype] = phenoObj;
    QJsonDocument newDoc(obj);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    file.write(newDoc.toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

// 文件上传功能实现
void MainWindow::uploadFiles(const QString &targetDir, const QString &fileType) {
    // 创建目标目录（如果不存在）
    QDir dir;
    QString fullTargetPath = QDir::currentPath() + "/" + targetDir;
    if (!dir.exists(fullTargetPath)) {
        if (!dir.mkpath(fullTargetPath)) {
            MyMessageBox msgBox(this);
            msgBox.setMySize(300, 150);
            msgBox.setIcon(QMessageBox::Warning);
            msgBox.setWindowTitle(tr("错误"));
            msgBox.setText(tr("无法创建目标目录：") + fullTargetPath);
            msgBox.setStandardButtons(QMessageBox::Ok);
            msgBox.exec();
            return;
        }
    }
    // 打开文件选择对话框
    QStringList fileNames = QFileDialog::getOpenFileNames(
        this,
        tr("选择") + fileType + tr("文件"),
        QDir::homePath(),
        tr("支持的文件格式 (*.csv *.pt *.xls *.xlsx);;CSV文件 (*.csv);;PyTorch文件 (*.pt);;Excel文件 (*.xls *.xlsx);;所有文件 (*.*)")
    );
    if (fileNames.isEmpty()) {
        return; // 用户取消了选择
    }
    // 验证文件格式并复制文件
    QStringList successFiles;
    QStringList failedFiles;
    for (const QString &fileName : fileNames) {
        QFileInfo fileInfo(fileName);
        // 验证文件格式
        if (!isValidFileFormat(fileInfo.fileName())) {
            failedFiles.append(fileInfo.fileName() + tr(" (不支持的格式)"));
            continue;
        }
        // 复制文件到目标目录
        QString targetFilePath = fullTargetPath + "/" + fileInfo.fileName();
        // 如果目标文件已存在，询问是否覆盖
        if (QFile::exists(targetFilePath)) {
            QMessageBox::StandardButton reply = QMessageBox::question(
                this,
                tr("文件已存在"),
                tr("文件 ") + fileInfo.fileName() + tr(" 已存在，是否覆盖？"),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No
            );
            if (reply == QMessageBox::No) {
                failedFiles.append(fileInfo.fileName() + tr(" (用户取消覆盖)"));
                continue;
            }
            // 删除现有文件
            QFile::remove(targetFilePath);
        }
        // 复制文件
        if (QFile::copy(fileName, targetFilePath)) {
            successFiles.append(fileInfo.fileName());
        } else {
            failedFiles.append(fileInfo.fileName() + tr(" (复制失败)"));
        }
    }
    // 显示结果
    QString resultMsg;
    if (!successFiles.isEmpty()) {
        resultMsg += tr("成功上传 ") + QString::number(successFiles.size()) + tr(" 个") + fileType + tr("文件：\n");
        resultMsg += successFiles.join("\n") + "\n\n";
    }
    if (!failedFiles.isEmpty()) {
        resultMsg += tr("失败 ") + QString::number(failedFiles.size()) + tr(" 个文件：\n");
        resultMsg += failedFiles.join("\n");
    }
    MyMessageBox msgBox(this);
    msgBox.setMySize(400, 200);
    msgBox.setIcon(failedFiles.isEmpty() ? QMessageBox::Information : QMessageBox::Warning);
    msgBox.setWindowTitle(tr("上传结果"));
    msgBox.setText(resultMsg);
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.exec();
}

void MainWindow::updateStep2Progress(int percent) {
    qDebug() << "[MainWindow] Received progress signal:" << percent << "%";
    ui->progressBar_step2->setValue(percent);
    ui->progressBar_step2->repaint(); // 强制立即刷新显示
}

void MainWindow::changeProgress(int value) {
    qDebug() << "[MainWindow] changeProgress called with value:" << value << ", this=" << this << ", thread=" << QThread::currentThread();
    ui->progressBar_step2->setValue(value);
    ui->progressBar_step2->repaint(); // 强制立即刷新
}

void MainWindow::updatePredictStatus(const QString &msg) {
    ui->label_predict_status->setText(msg);
}

void MainWindow::updateProgressFromLog() {
    int curEpoch = parseEpochFromLog(currentLogPath);
    if (curEpoch > lastParsedEpoch) {
        lastParsedEpoch = curEpoch;
        int percent = qMin(100, (int)((curEpoch + 1) * 100.0 / currentTotalEpoch));
        qDebug() << "[MainWindow] Real-time progress from log:" << percent << "% , epoch:" << curEpoch;
        ui->progressBar_step2->setValue(percent);
    }
}

bool MainWindow::isValidFileFormat(const QString &fileName) {
    QFileInfo fileInfo(fileName);
    QString suffix = fileInfo.suffix().toLower();
    QStringList validFormats = {"csv", "pt", "xls", "xlsx"};
    return validFormats.contains(suffix);
}

int MainWindow::parseEpochFromLog(const QString &logPath) {
    QFile f(logPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return -1;
    }
    int lastEpoch = -1;
    QTextStream in(&f);
    while (!in.atEnd()) {
        QString line = in.readLine();
        QRegularExpression re("epoch = (\\d+)");
        auto match = re.match(line);
        if (match.hasMatch()) {
            lastEpoch = match.captured(1).toInt();
        }
    }
    return lastEpoch;
}



