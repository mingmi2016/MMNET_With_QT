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
#include <QCheckBox>
#include <QProcess>
#include "savedsettingdialog.h"

#define IS_DEVELOP_MODE 1 // 1为开发模式，0为正式模式

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
    MainWindow(QWidget *parent = nullptr, bool isDevelopMode = true);
    ~MainWindow();
    void changeProgress(int value); // 参考示例：用于更新进度条

private slots:
    void on_pushButton_clicked();    // 上传基因文件
    void on_pushButton_2_clicked();  // 上传表型文件
    void on_pushButton_5_clicked();  // 上传待预测文件
    void handleTrainModelClicked();  // Train Model
    void on_pushButton_load_model_clicked(); // Load Model
    void on_pushButton_transfer_learning_clicked(); // Transfer Learning
    void showNextTransferLearningDialog(); // Transfer Learning参数弹窗
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
    
    // 检查路径是否包含中文
    bool containsChineseCharacters(const QString &path);

    // phenotype多选相关
    QList<QCheckBox*> phenotypeCheckBoxes;
    QStringList selectedPhenotypes;
    void refreshPhenotypeOptions();

    // 防止重复运行step2
    bool isStep2Running = false;

    // 添加：保存saved值到json的辅助函数
    bool updateSavedValue(const QString &jsonPath, const QString &phenotype, int savedValue);

    // --- 新增：多表型参数弹窗重构相关 ---
    QStringList pendingPhenotypes; // 待处理的表型队列
    struct PhenotypeSetting {
        int esnBatch = 0, esnSaved = 0;
        double esnP = 0.0;
        int mmnetBatch = 0, mmnetSaved = 0;
        double mmnetP1 = 0.0, mmnetP2 = 0.0, mmnetP3 = 0.0, mmnetP4 = 0.0, mmnetWd = 0.0;
    };
    QMap<QString, PhenotypeSetting> phenotypeSettings; // 每个表型的参数
    void showNextSettingDialog(); // 弹出下一个参数设置对话框
    void startTrainingForPhenotypes(); // 所有参数设置完毕后统一训练

    // --- 新增：多表型训练/预测统一弹框 ---
    QStringList trainPhenoQueue; // 训练表型队列
    QList<QString> trainResultMsgs; // 训练结果信息
    void trainNextPhenotype(); // 训练下一个表型
    void showTrainSummary(); // 训练全部完成后弹框

    QStringList predictPhenoQueue; // 预测表型队列
    QList<QString> predictResultMsgs; // 预测结果信息
    void predictNextPhenotype(); // 预测下一个表型
    void showPredictSummary(); // 预测全部完成后弹框

    // --- 新增：异步预测相关 ---
    QProcess *predictProcess = nullptr;
    QString currentPredictPhenotype;

    bool isDevelopMode;

    void startTransferLearningForPhenotypes(); // Transfer Learning helper
};
#endif // MAINWINDOW_H
