#ifndef SAVEDSETTINGDIALOG_H
#define SAVEDSETTINGDIALOG_H
#include <QDialog>
#include <QDoubleSpinBox>
class QSpinBox;
class QLabel;
class QPushButton;
class SavedSettingDialog : public QDialog {
    Q_OBJECT
public:
    explicit SavedSettingDialog(QWidget *parent = nullptr);
    void setPhenotype(const QString &pheno);
    void setEsnValues(int batchSize, double p, int saved);
    void setMmnetValues(int batchSize, double p1, double p2, double p3, double p4, int saved, double wd);
    void getEsnValues(int &batchSize, double &p, int &saved) const;
    void getMmnetValues(int &batchSize, double &p1, double &p2, double &p3, double &p4, int &saved, double &wd) const;
private:
    QLabel *labelTitle;
    // ESN
    QSpinBox *spinEsnBatch;
    QDoubleSpinBox *spinEsnP;
    QSpinBox *spinEsnSaved;
    // MMNet
    QSpinBox *spinMmnetBatch;
    QDoubleSpinBox *spinMmnetP1;
    QDoubleSpinBox *spinMmnetP2;
    QDoubleSpinBox *spinMmnetP3;
    QDoubleSpinBox *spinMmnetP4;
    QSpinBox *spinMmnetSaved;
    QDoubleSpinBox *spinMmnetWd;
    QPushButton *btnOk;
    QPushButton *btnCancel;
};
#endif // SAVEDSETTINGDIALOG_H 