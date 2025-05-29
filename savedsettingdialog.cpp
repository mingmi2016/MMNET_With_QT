#include "savedsettingdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSpinBox>
#include <QPushButton>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QFormLayout>
SavedSettingDialog::SavedSettingDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle(tr("设置模型参数"));
    labelTitle = new QLabel(this);
    // ESN
    spinEsnBatch = new QSpinBox(this);
    spinEsnBatch->setRange(1, 100000);
    spinEsnP = new QDoubleSpinBox(this);
    spinEsnP->setRange(0, 1);
    spinEsnP->setDecimals(4);
    spinEsnSaved = new QSpinBox(this);
    spinEsnSaved->setRange(1, 10000);
    // MMNet
    spinMmnetBatch = new QSpinBox(this);
    spinMmnetBatch->setRange(1, 100000);
    spinMmnetP1 = new QDoubleSpinBox(this);
    spinMmnetP1->setRange(0, 1);
    spinMmnetP1->setDecimals(4);
    spinMmnetP2 = new QDoubleSpinBox(this);
    spinMmnetP2->setRange(0, 1);
    spinMmnetP2->setDecimals(4);
    spinMmnetP3 = new QDoubleSpinBox(this);
    spinMmnetP3->setRange(0, 1);
    spinMmnetP3->setDecimals(4);
    spinMmnetP4 = new QDoubleSpinBox(this);
    spinMmnetP4->setRange(0, 1);
    spinMmnetP4->setDecimals(4);
    spinMmnetSaved = new QSpinBox(this);
    spinMmnetSaved->setRange(1, 10000);
    spinMmnetWd = new QDoubleSpinBox(this);
    spinMmnetWd->setRange(0, 1);
    spinMmnetWd->setDecimals(8);
    // 布局
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(labelTitle);
    // ESN部分
    QGroupBox *esnGroup = new QGroupBox(tr("ESN参数"), this);
    QFormLayout *esnLayout = new QFormLayout();
    esnLayout->addRow(tr("batch size:"), spinEsnBatch);
    esnLayout->addRow(tr("p:"), spinEsnP);
    esnLayout->addRow(tr("saved (epoch):"), spinEsnSaved);
    esnGroup->setLayout(esnLayout);
    mainLayout->addWidget(esnGroup);
    // MMNet部分
    QGroupBox *mmnetGroup = new QGroupBox(tr("MMNet参数"), this);
    QFormLayout *mmnetLayout = new QFormLayout();
    mmnetLayout->addRow(tr("batch size:"), spinMmnetBatch);
    mmnetLayout->addRow(tr("p1:"), spinMmnetP1);
    mmnetLayout->addRow(tr("p2:"), spinMmnetP2);
    mmnetLayout->addRow(tr("p3:"), spinMmnetP3);
    mmnetLayout->addRow(tr("p4:"), spinMmnetP4);
    mmnetLayout->addRow(tr("saved (epoch):"), spinMmnetSaved);
    mmnetLayout->addRow(tr("wd:"), spinMmnetWd);
    mmnetGroup->setLayout(mmnetLayout);
    mainLayout->addWidget(mmnetGroup);
    // 按钮
    btnOk = new QPushButton(tr("确定"), this);
    btnCancel = new QPushButton(tr("取消"), this);
    QHBoxLayout *btnRow = new QHBoxLayout();
    btnRow->addStretch();
    btnRow->addWidget(btnOk);
    btnRow->addWidget(btnCancel);
    mainLayout->addLayout(btnRow);
    setLayout(mainLayout);
    connect(btnOk, &QPushButton::clicked, this, &SavedSettingDialog::accept);
    connect(btnCancel, &QPushButton::clicked, this, &SavedSettingDialog::reject);
}
void SavedSettingDialog::setPhenotype(const QString &pheno) {
    labelTitle->setText(tr("表型: ") + pheno);
}
void SavedSettingDialog::setEsnValues(int batchSize, double p, int saved) {
    spinEsnBatch->setValue(batchSize);
    spinEsnP->setValue(p);
    spinEsnSaved->setValue(saved);
}
void SavedSettingDialog::setMmnetValues(int batchSize, double p1, double p2, double p3, double p4, int saved, double wd) {
    spinMmnetBatch->setValue(batchSize);
    spinMmnetP1->setValue(p1);
    spinMmnetP2->setValue(p2);
    spinMmnetP3->setValue(p3);
    spinMmnetP4->setValue(p4);
    spinMmnetSaved->setValue(saved);
    spinMmnetWd->setValue(wd);
}
void SavedSettingDialog::getEsnValues(int &batchSize, double &p, int &saved) const {
    batchSize = spinEsnBatch->value();
    p = spinEsnP->value();
    saved = spinEsnSaved->value();
}
void SavedSettingDialog::getMmnetValues(int &batchSize, double &p1, double &p2, double &p3, double &p4, int &saved, double &wd) const {
    batchSize = spinMmnetBatch->value();
    p1 = spinMmnetP1->value();
    p2 = spinMmnetP2->value();
    p3 = spinMmnetP3->value();
    p4 = spinMmnetP4->value();
    saved = spinMmnetSaved->value();
    wd = spinMmnetWd->value();
} 