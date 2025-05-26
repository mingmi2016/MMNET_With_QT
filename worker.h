#ifndef WORKER_H
#define WORKER_H

#include <QObject>
#include <QString>
#include <QDateTime>

struct StepResult {
    bool ok;
    double seconds;
    QDateTime startTime;
    QDateTime endTime;
};

class MainWindow; // 前向声明

class Worker : public QObject
{
    Q_OBJECT

public:
    explicit Worker(QObject *parent = nullptr);
    void setParams(const QString &exe1, const QString &exe2, const QString &log1, const QString &log2, const QString &json1, const QString &json2, const QString &pheno);
    void setMainWindow(MainWindow *mainWin); // 设置主窗口指针
    
public slots:
    void run();
    void updateProgress(); // 内部槽函数，用于更新进度
    
signals:
    void sendProgressSignal(); // 内部信号
    void finished(bool success, const QString &msg, double seconds, double exe1Seconds, double exe2Seconds, 
                 const QDateTime &step1Start, const QDateTime &step1End, 
                 const QDateTime &step2Start, const QDateTime &step2End);
    
private:
    QString exePath1, exePath2, logPath1, logPath2, jsonPath1, jsonPath2, phenotype;
    MainWindow *main_window; // 主窗口指针
    int current_progress; // 当前进度值
    
    StepResult runStep(const QString &exe, const QString &log, const QString &json, const QString &pheno, bool isStep1);
    int parseEpoch(const QString &log);
    int calculateOverallProgress(int stepProgress, bool isStep1); // 计算整体进度
};

#endif // WORKER_H 