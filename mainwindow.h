#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QThread>
#include <QDateTime>
#include <QTimer>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class Worker;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_pushButton_clicked();
    void on_pushButton_3_clicked();
    void on_pushButton_4_clicked();
    void updateStep2Progress(int percent);    void step2Finished(bool success, const QString &msg, double seconds, double exe1Seconds, double exe2Seconds);    void updatePredictStatus(const QString &msg);    void updateProgressFromLog();public:    void changeProgress(int value); // 参考示例：用于更新进度条

private:
    Ui::MainWindow *ui;
    QThread *workerThread = nullptr;
    Worker *worker = nullptr;
    QDateTime step2StartTime;
    
    // 添加定时器和相关变量用于实时进度更新
    QTimer *progressTimer = nullptr;
    QString currentLogPath;
    int currentTotalEpoch = 0;
    int lastParsedEpoch = -1;
    
    void startProgressMonitoring(const QString &logPath, int totalEpoch);
    void stopProgressMonitoring();
    int parseEpochFromLog(const QString &logPath);
};
#endif // MAINWINDOW_H
