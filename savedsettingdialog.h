#ifndef SAVEDSETTINGDIALOG_H
#define SAVEDSETTINGDIALOG_H
#include <QDialog>
#include <QDoubleSpinBox>
#include <QRegularExpression>

class QSpinBox;
class QLabel;
class QPushButton;

class MyDoubleSpinBox : public QDoubleSpinBox {
    Q_OBJECT
public:
    using QDoubleSpinBox::QDoubleSpinBox;
protected:
    QString textFromValue(double value) const override {
        QString s = QString::number(value, 'f', this->decimals());
        s = s.remove(QRegularExpression("0+$"));
        s = s.remove(QRegularExpression("\\.$"));
        return s;
    }
};

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
    MyDoubleSpinBox *spinEsnP;
    QSpinBox *spinEsnSaved;
    // MMNet
    QSpinBox *spinMmnetBatch;
    MyDoubleSpinBox *spinMmnetP1;
    MyDoubleSpinBox *spinMmnetP2;
    MyDoubleSpinBox *spinMmnetP3;
    MyDoubleSpinBox *spinMmnetP4;
    QSpinBox *spinMmnetSaved;
    MyDoubleSpinBox *spinMmnetWd;
    QPushButton *btnOk;
    QPushButton *btnCancel;
};
#endif // SAVEDSETTINGDIALOG_H 