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
    qDebug() << "[Worker] Constructed, this=" << this << ", thread=" << QThread::currentThread();
    // 按照参考代码模式：连接内部信号到内部槽
    bool conn = connect(this, SIGNAL(sendProgressSignal()), this, SLOT(updateProgress()));
    qDebug() << "[Worker] sendProgressSignal->updateProgress connected:" << conn;
}

void Worker::setParams(const QString &exe1, const QString &exe2, const QString &log1, const QString &log2, const QString &json1, const QString &json2, const QString &pheno) {
    exePath1 = exe1; 
    exePath2 = exe2; 
    logPath1 = log1; 
    logPath2 = log2; 
    jsonPath1 = json1; 
    jsonPath2 = json2; 
    phenotype = pheno;
    qDebug() << "[Worker] setParams called, this=" << this << ", exe1=" << exe1 << ", exe2=" << exe2 << ", pheno=" << pheno;
}

void Worker::setMainWindow(MainWindow *mainWin) {
    main_window = mainWin;
    qDebug() << "[Worker] setMainWindow called, this=" << this << ", mainWin=" << mainWin;
}

void Worker::updateProgress() {
    qDebug() << "[Worker] updateProgress called, this=" << this << ", progress=" << current_progress << ", thread=" << QThread::currentThread();
    // 按照参考代码模式：直接调用主窗口的方法更新进度
    if (main_window) {
        main_window->changeProgress(current_progress);
    } else {
        qDebug() << "[Worker] main_window is nullptr!";
    }
}

int Worker::calculateOverallProgress(int stepProgress, bool isStep1) {
    if (isStep1) {
        // 第一步：0-50%
        return stepProgress / 2;
    } else {
        // 第二步：50-100%
        return 50 + stepProgress / 2;
    }
}

void Worker::run() {
    qDebug() << "[Worker] ===== Worker::run() called! ===== this=" << this << ", thread=" << QThread::currentThread() << ", func=" << Q_FUNC_INFO;
    qDebug() << "[Worker] Start run: " << exePath1 << exePath2;
    
    // 检查exe文件是否存在
    qDebug() << "[Worker] Checking exe files:";
    qDebug() << "[Worker] exePath1 exists:" << QFile::exists(exePath1) << ", path:" << exePath1;
    qDebug() << "[Worker] exePath2 exists:" << QFile::exists(exePath2) << ", path:" << exePath2;
    
    // 第一步：generate_genetic_relatedness.exe (0-50%)
    qDebug() << "[Worker] ===== Starting Step 1 =====";
    StepResult r1 = runStep(exePath1, logPath1, jsonPath1, phenotype, true);
    qDebug() << "[Worker] runStep1 finished, ok=" << r1.ok << ", seconds=" << r1.seconds;
    if (!r1.ok) { 
        qDebug() << "[Worker] Step 1 failed, stopping execution";
        qDebug() << "[Worker] Step 1 failure details - check the logs above for process output and errors";
        emit finished(false, "generate_genetic_relatedness.exe 运行失败！", r1.seconds, r1.seconds, 0.0,
                     r1.startTime, r1.endTime, QDateTime(), QDateTime()); 
        return; 
    } else {
        qDebug() << "[Worker] Step 1 completed successfully, proceeding to Step 2";
    }
    
    // 第二步：train_mmnet.exe (50-100%)
    qDebug() << "[Worker] ===== Starting Step 2 =====";
    StepResult r2 = runStep(exePath2, logPath2, jsonPath2, phenotype, false);
    qDebug() << "[Worker] runStep2 finished, ok=" << r2.ok << ", seconds=" << r2.seconds;
    double totalSeconds = r1.seconds + r2.seconds;
    if (!r2.ok) { 
        qDebug() << "[Worker] Step 2 failed";
        emit finished(false, "train_mmnet.exe 运行失败！", totalSeconds, r1.seconds, r2.seconds,
                     r1.startTime, r1.endTime, r2.startTime, r2.endTime); 
        return; 
    }
    
    qDebug() << "[Worker] Both steps completed successfully";
    emit finished(true, "模型训练已完成！", totalSeconds, r1.seconds, r2.seconds,
                 r1.startTime, r1.endTime, r2.startTime, r2.endTime);
}

StepResult Worker::runStep(const QString &exe, const QString &log, const QString &json, const QString &pheno, bool isStep1) {
    QElapsedTimer timer;
    timer.start();
    QDateTime startTime = QDateTime::currentDateTime();
    qDebug() << "[Worker] runStep:" << exe << log << json << pheno;
    qDebug() << "[Worker] Start time:" << startTime.toString("yyyy-MM-dd hh:mm:ss");
    
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
    // 现在totalEpoch表示最大epoch编号（如2），实际epoch数为totalEpoch+1
    
    // 启动进程
    QProcess process;
    process.setWorkingDirectory(QFileInfo(exe).absolutePath());
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
    // 监控进度（只在进程运行时解析log）
    int lastEpoch = -1;
    qDebug() << "[Worker] Starting monitoring loop for log:" << log << ", isStep1:" << isStep1;
    int lastPrintedReturned = INT_MIN;
    while (process.state() == QProcess::Running) {
        process.waitForFinished(500);
        int curEpoch = parseEpoch(log);
        if (curEpoch != lastPrintedReturned) {
            qDebug() << "[Worker] parseEpoch returned:" << curEpoch << "for log:" << log;
            lastPrintedReturned = curEpoch;
        }
        if (curEpoch > lastEpoch) {
            lastEpoch = curEpoch;
            // 优化进度百分比计算：(curEpoch+1)/(totalEpoch+1)
            int percent = qMin(100, (int)(((curEpoch + 1) * 100.0) / (totalEpoch + 1)));
            qDebug() << "[Worker] Step progress:" << percent << "% , isStep1:" << isStep1;
            current_progress = calculateOverallProgress(percent, isStep1);
            qDebug() << "[Worker] Overall progress:" << current_progress << "%";
            emit sendProgressSignal();
        }
        // 如果已经到最后一个epoch，提前跳出循环
        if (curEpoch >= totalEpoch) {
            break;
        }
    }
    // 等进程完全退出
    if (process.state() != QProcess::NotRunning) {
        process.waitForFinished(-1);
    }
    // 进程结束后，做一次最终进度
    current_progress = calculateOverallProgress(100, isStep1);
    qDebug() << "[Worker] Final progress for step (isStep1=" << isStep1 << "):" << current_progress << "%";
    emit sendProgressSignal();
    QDateTime endTime = QDateTime::currentDateTime();
    qDebug() << "[Worker] Process finished, exitCode:" << process.exitCode() << ", exitStatus:" << process.exitStatus();
    qDebug() << "[Worker] End time:" << endTime.toString("yyyy-MM-dd hh:mm:ss");
    // 捕获进程的所有输出
    QByteArray allOutput = process.readAll();
    if (!allOutput.isEmpty()) {
        qDebug() << "[Worker] Process output:";
        qDebug().noquote() << QString::fromLocal8Bit(allOutput);
    }
    // 如果进程失败，输出日志最后20行，并立即return
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
        double seconds = timer.elapsed() / 1000.0;
        return {false, seconds, startTime, endTime};
    }
    double seconds = timer.elapsed() / 1000.0;
    return {process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0, seconds, startTime, endTime};
}

int Worker::parseEpoch(const QString &log) {
    static QMap<QString, int> lastPrintedEpoch; // 记录每个log文件上次打印的epoch
    QFile f(log);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) { 
        if (lastPrintedEpoch.value(log, INT_MIN) != -2) {
            qDebug() << "[Worker] parseEpoch failed to open log:" << log;
            lastPrintedEpoch[log] = -2;
        }
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
    // 只有epoch变化时才打印
    if (lastPrintedEpoch.value(log, INT_MIN) != lastEpoch) {
        qDebug() << "[Worker] parseEpoch lastEpoch:" << lastEpoch << ", log:" << log;
        lastPrintedEpoch[log] = lastEpoch;
    }
    return lastEpoch;
}

 