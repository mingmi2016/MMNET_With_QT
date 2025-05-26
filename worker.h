#ifndef WORKER_H
#define WORKER_H

#include <QObject>
#include <QString>

struct StepResult {
    bool ok;
    double seconds;
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
    
signals:
    void progress(int percent); // 直接发射给MainWindow的进度信号
    void finished(bool success, const QString &msg, double seconds, double exe1Seconds, double exe2Seconds);
    
private:
    QString exePath1, exePath2, logPath1, logPath2, jsonPath1, jsonPath2, phenotype;
    
    StepResult runStep(const QString &exe, const QString &log, const QString &json, const QString &pheno, bool isStep1);
    int parseEpoch(const QString &log);
};

#endif // WORKER_H 