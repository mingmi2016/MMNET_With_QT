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

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    qDebug() << "[MainWindow] Constructed, this=" << this << ", thread=" << QThread::currentThread();
    ui->setupUi(this);
    connect(ui->pushButton_3, &QPushButton::clicked, this, &MainWindow::on_pushButton_3_clicked);
    
    // 初始化进度监控定时器
    progressTimer = new QTimer(this);
    connect(progressTimer, &QTimer::timeout, this, &MainWindow::updateProgressFromLog);
    // 初始化phenotype单选组
    phenotypeGroup = new QButtonGroup(this);
    phenotypeGroup->setExclusive(true);
    connect(phenotypeGroup, SIGNAL(buttonClicked(int)), this, SLOT(onPhenotypeSelected()));
    refreshPhenotypeOptions();
    // 默认禁用下载结果按钮
    ui->pushButton_download_pred->setEnabled(false);
    // 默认隐藏进度条
    ui->progressBar_step2->setVisible(false);
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
void MainWindow::on_pushButton_3_clicked()
{
    qDebug() << "[MainWindow] on_pushButton_3_clicked, this=" << this << ", thread=" << QThread::currentThread();
    if (selectedPhenotype.isEmpty()) {
        QMessageBox::warning(this, tr("错误"), tr("请先选择一个表型文件！"));
        return;
    }
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
        MyMessageBox msgBox(this);
        msgBox.setMySize(300, 150);
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setWindowTitle(tr("错误"));
        msgBox.setText(tr("无法创建 step1.log 文件"));
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.exec();
        return;
    }
    step1Log.close();
    
    if (!step2Log.open(QIODevice::WriteOnly)) {
        MyMessageBox msgBox(this);
        msgBox.setMySize(300, 150);
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setWindowTitle(tr("错误"));
        msgBox.setText(tr("无法创建 step2.log 文件"));
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.exec();
        return;
    }
    step2Log.close();
    
    // 路径准备
    QString exePath1 = QDir::currentPath() + "/MMNET/generate_genetic_relatedness.exe";
    QString exePath2 = QDir::currentPath() + "/MMNET/train_mmnet.exe";
    
    // 检查exe文件是否存在
    qDebug() << "Checking exe files:";
    qDebug() << "exePath1 exists:" << QFile::exists(exePath1) << ", path:" << exePath1;
    qDebug() << "exePath2 exists:" << QFile::exists(exePath2) << ", path:" << exePath2;
    
    QString log1 = QDir::currentPath() + "/MMNET/step1.log";
    QString log2 = QDir::currentPath() + "/MMNET/step2.log";
    QString json1 = QDir::currentPath() + "/MMNET/configs/ESN.json";
    QString json2 = QDir::currentPath() + "/MMNET/configs/MMNet.json";
    QString phenotype = selectedPhenotype;
    
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
    worker->setMainWindow(this); // 参考示例：设置主窗口指针
    worker->moveToThread(workerThread);
    bool startedConn = connect(workerThread, &QThread::started, worker, &Worker::run);
    qDebug() << "[MainWindow] workerThread->started -> worker->run connected:" << startedConn;
    
    // 只连接finished信号，progress信号由Worker内部处理
    bool finishedConnected = connect(worker, &Worker::finished, this, &MainWindow::step2Finished, Qt::QueuedConnection);
    qDebug() << "[MainWindow] worker->finished -> step2Finished connected:" << finishedConnected;
    
    connect(worker, &Worker::finished, workerThread, &QThread::quit);
    connect(workerThread, &QThread::finished, worker, &QObject::deleteLater);
    
    ui->progressBar_step2->setValue(0);
    step2StartTime = QDateTime::currentDateTime();
    
    workerThread->start();
}

void MainWindow::updateStep2Progress(int percent) {
    qDebug() << "[MainWindow] Received progress signal:" << percent << "%";
    ui->progressBar_step2->setValue(percent);
    ui->progressBar_step2->repaint(); // 强制立即刷新显示
}

// 参考示例：更新进度条的方法
void MainWindow::changeProgress(int value) {
    qDebug() << "[MainWindow] changeProgress called with value:" << value << ", this=" << this << ", thread=" << QThread::currentThread();
    ui->progressBar_step2->setValue(value);
    ui->progressBar_step2->repaint(); // 强制立即刷新
}

void MainWindow::step2Finished(bool success, const QString &msg, double seconds, double exe1Seconds, double exe2Seconds,
                               const QDateTime &step1Start, const QDateTime &step1End, 
                               const QDateTime &step2Start, const QDateTime &step2End) {
    qDebug() << "[MainWindow] step2Finished called, this=" << this << ", thread=" << QThread::currentThread();
    ui->progressBar_step2->setValue(100);
    
    QString timeMsg = QString("\n本次运行耗时：%1 秒\n第一步: %2 秒\n第二步: %3 秒").arg(seconds, 0, 'f', 2).arg(exe1Seconds, 0, 'f', 2).arg(exe2Seconds, 0, 'f', 2);
    MyMessageBox msgBox(this);
    msgBox.setMySize(300, 150);
    msgBox.setIcon(success ? QMessageBox::Information : QMessageBox::Warning);
    msgBox.setWindowTitle(success ? tr("运行完成") : tr("错误"));
    msgBox.setText(msg + timeMsg);
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.exec();

    // 获取GPU型号
    QString gpuName = "Unknown";
    QProcess gpuProc;
    gpuProc.start("wmic", QStringList() << "path" << "win32_VideoController" << "get" << "name");
    if (gpuProc.waitForFinished(2000)) {
        QString output = gpuProc.readAllStandardOutput();
        QStringList lines = output.split("\n", Qt::SkipEmptyParts);
        if (lines.size() > 1) {
            gpuName = lines[1].trimmed();
        }
    }

    // 读取ESN.json和MMNET.json的epoch
    int esnEpoch = -1, mmnetEpoch = -1;
    QFile esnFile(QDir::currentPath() + "/MMNET/configs/ESN.json");
    if (esnFile.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(esnFile.readAll());
        QJsonObject obj = doc.object();
        if (obj.contains("culmlength") && obj["culmlength"].isObject()) {
            QJsonObject phenoObj = obj["culmlength"].toObject();
            if (phenoObj.contains("saved")) esnEpoch = phenoObj["saved"].toInt();
        }
    }
    QFile mmnetFile(QDir::currentPath() + "/MMNET/configs/MMNet.json");
    if (mmnetFile.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(mmnetFile.readAll());
        QJsonObject obj = doc.object();
        if (obj.contains("culmlength") && obj["culmlength"].isObject()) {
            QJsonObject phenoObj = obj["culmlength"].toObject();
            if (phenoObj.contains("saved")) mmnetEpoch = phenoObj["saved"].toInt();
        }
    }
    // 统计目录大小递归函数
    double geneSize = dirSizeMB(QDir::currentPath() + "/MMNET/data/gene");
    double phenSize = dirSizeMB(QDir::currentPath() + "/MMNET/data/phen");
    // 写入日志
    QFile logFile(QDir::currentPath() + "/run_history.log");
    if (logFile.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&logFile);
        out << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss") << ", ";
        out << (success ? "成功" : "失败") << ", ";
        out << "总耗时: " << seconds << "秒, ";
        
        // 详细时间记录
        if (!step1Start.isNull() && !step1End.isNull()) {
            out << "第一步开始: " << step1Start.toString("yyyy-MM-dd HH:mm:ss") << ", ";
            out << "第一步结束: " << step1End.toString("yyyy-MM-dd HH:mm:ss") << ", ";
        }
        out << "第一步耗时: " << exe1Seconds << "秒, ";
        
        if (!step2Start.isNull() && !step2End.isNull()) {
            out << "第二步开始: " << step2Start.toString("yyyy-MM-dd HH:mm:ss") << ", ";
            out << "第二步结束: " << step2End.toString("yyyy-MM-dd HH:mm:ss") << ", ";
        }
        out << "第二步耗时: " << exe2Seconds << "秒, ";
        
        out << "GPU: " << gpuName << ", ";
        out << "ESN epoch: " << esnEpoch << ", MMNET epoch: " << mmnetEpoch << ", ";
        out << "gene目录: " << QString::number(geneSize, 'f', 2) << " MB, ";
        out << "phen目录: " << QString::number(phenSize, 'f', 2) << " MB\n";
        logFile.close();
    }
}

void MainWindow::on_pushButton_4_clicked()
{
    if (selectedPhenotype.isEmpty()) {
        QMessageBox::warning(this, tr("错误"), tr("请先选择一个表型文件！"));
        return;
    }
    // 检查是否有待预测文件
    QString predDir = QDir::currentPath() + "/MMNET/data/pred";
    QDir dir(predDir);
    QStringList filters = {selectedPhenotype + ".csv", selectedPhenotype + ".pt", selectedPhenotype + ".xls", selectedPhenotype + ".xlsx"};
    QStringList found = dir.entryList(filters, QDir::Files);
    if (found.isEmpty()) {
        QMessageBox::warning(this, tr("错误"), tr("请先上传待预测文件（文件名需与表型一致）！"));
        return;
    }
    // 防止重复触发
    if (!ui->pushButton_4->isEnabled()) return;
    
    QString oldText = ui->pushButton_4->text();
    ui->pushButton_4->setText("预测中...");
    ui->pushButton_4->setEnabled(false);
    ui->pushButton_4->repaint();
    
    // 运行 pred.exe
    QString exePath = QDir::currentPath() + "/MMNET/pred.exe";
    if (!QFile::exists(exePath)) {
        MyMessageBox msgBox(this);
        msgBox.setMySize(300, 150);
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setWindowTitle(tr("错误"));
        msgBox.setText(tr("找不到 pred.exe"));
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.exec();
        ui->pushButton_4->setText(oldText);
        ui->pushButton_4->setEnabled(true);
        return;
    }

    QProcess *process = new QProcess(this);
    process->setWorkingDirectory(QDir::currentPath() + "/MMNET");
    process->start(exePath);
    process->waitForFinished(-1);

    if (process->exitStatus() != QProcess::NormalExit || process->exitCode() != 0) {
        MyMessageBox msgBox(this);
        msgBox.setMySize(300, 150);
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setWindowTitle(tr("错误"));
        msgBox.setText(tr("pred.exe 运行失败！"));
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.exec();
        process->deleteLater();
        ui->pushButton_4->setText(oldText);
        ui->pushButton_4->setEnabled(true);
        return;
    }

    MyMessageBox msgBox(this);
    msgBox.setMySize(300, 150);
    msgBox.setIcon(QMessageBox::Information);
    msgBox.setWindowTitle(tr("运行完成"));
    msgBox.setText(tr("预测已完成！"));
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.exec();
    
    ui->pushButton_4->setText(oldText);
    ui->pushButton_4->setEnabled(true);
    process->deleteLater();
    // 预测成功后启用下载按钮
    ui->pushButton_download_pred->setEnabled(true);
}

void MainWindow::updatePredictStatus(const QString &msg) {
    ui->label_predict_status->setText(msg);
}

// 进度监控相关方法
void MainWindow::startProgressMonitoring(const QString &logPath, int totalEpoch) {
    currentLogPath = logPath;
    currentTotalEpoch = totalEpoch;
    lastParsedEpoch = -1;
    progressTimer->start(500); // 每500ms检查一次
}

void MainWindow::stopProgressMonitoring() {
    if (progressTimer) {
        progressTimer->stop();
    }
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

void MainWindow::updateProgressFromLog() {
    int curEpoch = parseEpochFromLog(currentLogPath);
    if (curEpoch > lastParsedEpoch) {
        lastParsedEpoch = curEpoch;
        int percent = qMin(100, (int)((curEpoch + 1) * 100.0 / currentTotalEpoch));
        qDebug() << "[MainWindow] Real-time progress from log:" << percent << "%, epoch:" << curEpoch;
        ui->progressBar_step2->setValue(percent);
    }
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

// 验证文件格式
bool MainWindow::isValidFileFormat(const QString &fileName) {
    QFileInfo fileInfo(fileName);
    QString suffix = fileInfo.suffix().toLower();
    
    QStringList validFormats = {"csv", "pt", "xls", "xlsx"};
    return validFormats.contains(suffix);
}

void MainWindow::refreshPhenotypeOptions() {
    // 清空旧的
    QLayout *layout = ui->groupBox_step2->layout();
    if (!layout) return;
    // 查找并移除旧的单选按钮
    QList<QRadioButton*> oldRadios = ui->groupBox_step2->findChildren<QRadioButton*>();
    for (QRadioButton *btn : oldRadios) {
        phenotypeGroup->removeButton(btn);
        layout->removeWidget(btn);
        btn->deleteLater();
    }
    // 遍历MMNET/data/phen目录
    QString phenDir = QDir::currentPath() + "/MMNET/data/phen";
    QDir dir(phenDir);
    QStringList filters;
    filters << "*.csv" << "*.pt" << "*.xls" << "*.xlsx";
    QFileInfoList fileList = dir.entryInfoList(filters, QDir::Files | QDir::NoDotAndDotDot);
    int idx = 0;
    QHBoxLayout *radioLayout = new QHBoxLayout();
    selectedPhenotype.clear();
    for (const QFileInfo &info : fileList) {
        QString baseName = info.completeBaseName();
        QRadioButton *radio = new QRadioButton(baseName, ui->groupBox_step2);
        phenotypeGroup->addButton(radio, idx++);
        radioLayout->addWidget(radio);
        if (selectedPhenotype.isEmpty()) {
            radio->setChecked(true);
            selectedPhenotype = baseName;
        }
    }
    // 插入到Run Model按钮上方
    QVBoxLayout *vLayout = qobject_cast<QVBoxLayout*>(ui->groupBox_step2->layout());
    if (vLayout) {
        vLayout->insertLayout(0, radioLayout);
    }
    // 启用/禁用Run Model按钮
    if (fileList.isEmpty()) {
        ui->pushButton_3->setEnabled(false);
    } else {
        ui->pushButton_3->setEnabled(true);
    }
}

void MainWindow::onPhenotypeSelected() {
    QAbstractButton *btn = phenotypeGroup->checkedButton();
    if (btn) {
        selectedPhenotype = btn->text();
        ui->pushButton_3->setEnabled(true);
    } else {
        selectedPhenotype.clear();
        ui->pushButton_3->setEnabled(false);
    }
}

void MainWindow::on_pushButton_download_pred_clicked()
{
    QString srcFile = QDir::currentPath() + "/MMNET/MMNet_pred.csv";
    if (!QFile::exists(srcFile)) {
        QMessageBox::warning(this, tr("错误"), tr("未找到结果文件 MMNet_pred.csv，请先完成预测！"));
        return;
    }
    QString savePath = QFileDialog::getSaveFileName(this, tr("保存结果文件"), QDir::homePath() + "/MMNet_pred.csv", tr("CSV文件 (*.csv);;所有文件 (*.*)"));
    if (savePath.isEmpty()) return;
    if (QFile::exists(savePath)) QFile::remove(savePath);
    if (QFile::copy(srcFile, savePath)) {
        QMessageBox::information(this, tr("成功"), tr("结果文件已保存到：\n") + savePath);
    } else {
        QMessageBox::warning(this, tr("错误"), tr("保存失败，请检查目标路径权限！"));
    }
}



