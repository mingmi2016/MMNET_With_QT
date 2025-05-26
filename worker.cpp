#include "worker.h"
#include "mainwindow.h"
#include <QProcess>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <QDir>
#include <QElapsedTimer>
#include <QRegularExpression>
#include <QDebug>
#include <QThread>

Worker::Worker(QObject *parent) : QObject(parent), main_window(nullptr), current_progress(0) {
    // 按照参考代码模式：连接内部信号到内部槽
    connect(this, SIGNAL(sendProgressSignal()), this, SLOT(updateProgress()));
}

void Worker::setParams(const QString &exe1, const QString &exe2, const QString &log1, const QString &log2, const QString &json1, const QString &json2, const QString &pheno) {
    exePath1 = exe1; 
    exePath2 = exe2; 
    logPath1 = log1; 
    logPath2 = log2; 
    jsonPath1 = json1; 
    jsonPath2 = json2; 
    phenotype = pheno;
}

void Worker::setMainWindow(MainWindow *mainWin) {
    main_window = mainWin;
}

void Worker::updateProgress() {
    // 按照参考代码模式：直接调用主窗口的方法更新进度
    if (main_window) {
        main_window->changeProgress(current_progress);
    }
}

void Worker::run() {
    qDebug() << "[Worker] ===== Worker::run() called! =====";
    qDebug() << "[Worker] Start run: " << exePath1 << exePath2;
    
    // 检查exe文件是否存在
    qDebug() << "[Worker] Checking exe files:";
    qDebug() << "[Worker] exePath1 exists:" << QFile::exists(exePath1) << ", path:" << exePath1;
    qDebug() << "[Worker] exePath2 exists:" << QFile::exists(exePath2) << ", path:" << exePath2;
    
    // 第一步：generate_genetic_relatedness.exe
    qDebug() << "[Worker] ===== Starting Step 1 =====";
    StepResult r1 = runStep(exePath1, logPath1, jsonPath1, phenotype, true);
    qDebug() << "[Worker] runStep1 finished, ok=" << r1.ok << ", seconds=" << r1.seconds;
    if (!r1.ok) { 
        qDebug() << "[Worker] Step 1 failed, stopping execution";
        qDebug() << "[Worker] Step 1 failure details - check the logs above for process output and errors";
        emit finished(false, "generate_genetic_relatedness.exe 运行失败！", r1.seconds, r1.seconds, 0.0); 
        return; 
    } else {
        qDebug() << "[Worker] Step 1 completed successfully, proceeding to Step 2";
    }
    
    // 第二步：train_mmnet.exe
    qDebug() << "[Worker] ===== Starting Step 2 =====";
    StepResult r2 = runStep(exePath2, logPath2, jsonPath2, phenotype, false);
    qDebug() << "[Worker] runStep2 finished, ok=" << r2.ok << ", seconds=" << r2.seconds;
    double totalSeconds = r1.seconds + r2.seconds;
    if (!r2.ok) { 
        qDebug() << "[Worker] Step 2 failed";
        emit finished(false, "train_mmnet.exe 运行失败！", totalSeconds, r1.seconds, r2.seconds); 
        return; 
    }
    
    qDebug() << "[Worker] Both steps completed successfully";
    emit finished(true, "模型训练已完成！", totalSeconds, r1.seconds, r2.seconds);
}

StepResult Worker::runStep(const QString &exe, const QString &log, const QString &json, const QString &pheno, bool isStep1) {
    QElapsedTimer timer;
    timer.start();
    qDebug() << "[Worker] runStep:" << exe << log << json << pheno;
    
    // 读取配置获取总epoch数
    QFile jsonFile(json);
    if (!jsonFile.open(QIODevice::ReadOnly)) { 
        qDebug() << "[Worker] Failed to open json:" << json;
        return {false, 0.0}; 
    }
    QJsonDocument doc = QJsonDocument::fromJson(jsonFile.readAll());
    QJsonObject obj = doc.object();
    int totalEpoch = 0;
    if (obj.contains(pheno) && obj[pheno].isObject()) {
        QJsonObject phenoObj = obj[pheno].toObject();
        if (phenoObj.contains("saved")) totalEpoch = phenoObj["saved"].toInt();
    }
    if (totalEpoch <= 0) totalEpoch = 100;
    
    // 启动进程
    QProcess process;
    process.setWorkingDirectory(QFileInfo(exe).absolutePath());
    
    // 捕获所有输出
    process.setProcessChannelMode(QProcess::MergedChannels);
    
    qDebug() << "[Worker] Starting process:" << exe;
    qDebug() << "[Worker] Working directory:" << QFileInfo(exe).absolutePath();
    qDebug() << "[Worker] Arguments:" << (QStringList() << "--phenotype" << pheno);
    
    process.start(exe, QStringList() << "--phenotype" << pheno);
    if (!process.waitForStarted()) { 
        qDebug() << "[Worker] Failed to start process:" << exe;
        qDebug() << "[Worker] Error:" << process.errorString();
        return {false, 0.0}; 
    }
    
    qDebug() << "[Worker] Process started, PID:" << process.processId();
    
    // 简化的监控循环，按照参考代码模式
    int lastEpoch = -1;
    while (!process.waitForFinished(500)) {
        int curEpoch = parseEpoch(log);
        qDebug() << "[Worker] parseEpoch returned:" << curEpoch;
        if (curEpoch > lastEpoch) {
            lastEpoch = curEpoch;
            int percent = qMin(100, (int)((curEpoch + 1) * 100.0 / totalEpoch));
            qDebug() << "[Worker] emit progress:" << percent;
            current_progress = percent;
            emit sendProgressSignal();
        }
    }
    
    // 确保最终进度
    current_progress = 100;
    emit sendProgressSignal();
    
    qDebug() << "[Worker] Process finished, exitCode:" << process.exitCode() << ", exitStatus:" << process.exitStatus();
    
    // 捕获进程的所有输出
    QByteArray allOutput = process.readAll();
    if (!allOutput.isEmpty()) {
        qDebug() << "[Worker] Process output:";
        qDebug().noquote() << QString::fromLocal8Bit(allOutput);
    }
    
    // 如果进程失败，输出日志最后20行
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        QFile flog(log);
        if (flog.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QStringList lines;
            QTextStream ts(&flog);
            while (!ts.atEnd()) lines << ts.readLine();
            int n = lines.size();
            qDebug() << "[Worker] Last 20 lines of log:";
            for (int i = qMax(0, n-20); i < n; ++i) {
                qDebug().noquote() << lines[i];
            }
        }
    }
    
    double seconds = timer.elapsed() / 1000.0;
    return {process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0, seconds};
}

int Worker::parseEpoch(const QString &log) {
    QFile f(log);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) { 
        qDebug() << "[Worker] Failed to open log:" << log; 
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
    qDebug() << "[Worker] parseEpoch lastEpoch:" << lastEpoch;
    return lastEpoch;
}

 