#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QThread>
#include <QDateTime>
#include <QTimer>
#include <QFileDialog>
#include <QMessageBox>
#include <QDir>
#include <QButtonGroup>
#include <QRadioButton>
#include "savedsettingdialog.h"

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
    void changeProgress(int value); // 参考示例：用于更新进度条

private slots:
    void on_pushButton_clicked();    // 上传基因文件
    void on_pushButton_2_clicked();  // 上传表型文件
    void on_pushButton_5_clicked();  // 上传待预测文件
    void on_pushButton_3_clicked();
    void on_pushButton_4_clicked();
    void on_pushButton_download_pred_clicked();
    void updateStep2Progress(int percent);
    void step2Finished(bool success, const QString &msg, double seconds, double exe1Seconds, double exe2Seconds,
                      const QDateTime &step1Start, const QDateTime &step1End, 
                      const QDateTime &step2Start, const QDateTime &step2End);
    void updatePredictStatus(const QString &msg);
    void updateProgressFromLog();
    void onPhenotypeSelected();

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
    
    // 文件上传相关方法
    void uploadFiles(const QString &targetDir, const QString &fileType);
    bool isValidFileFormat(const QString &fileName);

    // phenotype单选组相关
    QButtonGroup *phenotypeGroup = nullptr;
    QString selectedPhenotype;
    void refreshPhenotypeOptions();

    // 防止重复运行step2
    bool isStep2Running = false;

    // 添加：保存saved值到json的辅助函数
    bool updateSavedValue(const QString &jsonPath, const QString &phenotype, int savedValue);
};
#endif // MAINWINDOW_H
