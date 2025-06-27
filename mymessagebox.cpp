#include "mymessagebox.h"
#include <QPixmap>
#include <QFileInfo>
#include <QGridLayout>
#include <QDebug>
#include <QScreen>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QSpacerItem>
#include <QStyle>
#include <QApplication>

MyMessageBox::MyMessageBox(QWidget *parent) :
    QDialog(parent),
    _width(0),
    _height(0),
    imageLabel(nullptr),
    textLabel(nullptr),
    iconLabel(nullptr),
    mainLayout(nullptr),
    okButton(nullptr)
{
    setWindowFlags(Qt::Dialog | Qt::WindowCloseButtonHint | Qt::WindowMaximizeButtonHint | Qt::WindowMinimizeButtonHint);
    
    // 创建主布局
    mainLayout = new QVBoxLayout(this);
    
    // 创建顶部水平布局（用于图标和文本）
    QHBoxLayout *topLayout = new QHBoxLayout();
    topLayout->setAlignment(Qt::AlignTop); // 整个顶部布局靠上对齐
    
    // 创建图标标签
    iconLabel = new QLabel(this);
    iconLabel->setFixedSize(48, 48); // 增加图标大小
    iconLabel->setAlignment(Qt::AlignTop); // 图标靠上对齐
    topLayout->addWidget(iconLabel, 0, Qt::AlignTop); // 使用布局的alignment参数
    
    // 创建文本标签
    textLabel = new QLabel(this);
    textLabel->setWordWrap(true);
    textLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft); // 文本靠上靠左对齐
    textLabel->setContentsMargins(10, 5, 0, 0); // 添加一些边距，使文本与图标对齐更自然
    topLayout->addWidget(textLabel, 1, Qt::AlignTop); // 使用布局的alignment参数
    
    mainLayout->addLayout(topLayout);
    
    // 创建图片标签
    imageLabel = new QLabel(this);
    imageLabel->setAlignment(Qt::AlignCenter);
    imageLabel->setMinimumSize(400, 300);
    imageLabel->setStyleSheet("QLabel { background-color: #f0f0f0; border: 1px solid #cccccc; }");
    imageLabel->setText(tr("Image will be displayed here"));
    imageLabel->hide();
    mainLayout->addWidget(imageLabel);
    
    // 添加垂直弹簧，确保内容靠上
    mainLayout->addStretch(1);
    
    // 创建按钮布局
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    
    okButton = new QPushButton(tr("OK"), this);
    okButton->setDefault(true);
    buttonLayout->addWidget(okButton);
    
    mainLayout->addLayout(buttonLayout);
    
    // 连接按钮信号
    connect(okButton, &QPushButton::clicked, this, &QDialog::accept);
    
    setLayout(mainLayout);
}

void MyMessageBox::setMySize(int width, int height)
{
    _width = width;
    _height = height;
    resize(width, height);
}

void MyMessageBox::setText(const QString &text)
{
    textLabel->setText(text);
}

void MyMessageBox::setIcon(QMessageBox::Icon icon)
{
    QStyle *style = QApplication::style();
    int iconSize = style->pixelMetric(QStyle::PM_MessageBoxIconSize);
    QIcon::Mode mode = QIcon::Normal;
    
    QPixmap pixmap;
    switch (icon) {
        case QMessageBox::Information:
            pixmap = style->standardIcon(QStyle::SP_MessageBoxInformation).pixmap(iconSize, iconSize, mode);
            break;
        case QMessageBox::Warning:
            pixmap = style->standardIcon(QStyle::SP_MessageBoxWarning).pixmap(iconSize, iconSize, mode);
            break;
        case QMessageBox::Critical:
            pixmap = style->standardIcon(QStyle::SP_MessageBoxCritical).pixmap(iconSize, iconSize, mode);
            break;
        case QMessageBox::Question:
            pixmap = style->standardIcon(QStyle::SP_MessageBoxQuestion).pixmap(iconSize, iconSize, mode);
            break;
        default:
            break;
    }
    
    if (!pixmap.isNull()) {
        iconLabel->setPixmap(pixmap);
        iconLabel->show();
    } else {
        iconLabel->hide();
    }
}

void MyMessageBox::setWindowTitle(const QString &title)
{
    QDialog::setWindowTitle(title);
}

void MyMessageBox::setStandardButtons(QMessageBox::StandardButtons buttons)
{
    // 目前只支持OK按钮，如果需要可以扩展
    okButton->setVisible(buttons & QMessageBox::Ok);
}

void MyMessageBox::setImage(const QString &imagePath)
{
    qDebug() << "[MyMessageBox] Setting image from path:" << imagePath;
    
    if (!imageLabel) {
        qDebug() << "[MyMessageBox] Error: imageLabel is null";
        return;
    }

    if (imagePath.isEmpty()) {
        qDebug() << "[MyMessageBox] Error: image path is empty";
        imageLabel->hide();
        return;
    }

    QFileInfo fileInfo(imagePath);
    if (!fileInfo.exists()) {
        qDebug() << "[MyMessageBox] Error: image file does not exist";
        imageLabel->setText(tr("Image file not found: %1").arg(imagePath));
        imageLabel->show();
        return;
    }

    QPixmap originalPixmap(imagePath);
    if (originalPixmap.isNull()) {
        qDebug() << "[MyMessageBox] Error: failed to load image";
        imageLabel->setText(tr("Failed to load image: %1").arg(imagePath));
        imageLabel->show();
        return;
    }

    qDebug() << "[MyMessageBox] Original image size:" << originalPixmap.size();
    
    QScreen *screen = QGuiApplication::primaryScreen();
    QRect screenGeometry = screen->availableGeometry();
    
    // 设置固定的目标大小（基于主窗口大小 800x600）
    int targetWidth = 850;  // 主窗口宽度 + 50
    int targetHeight = 650; // 主窗口高度 + 50
    
    QPixmap pixmap = originalPixmap;
    // 如果原图比目标尺寸大，就缩放到目标尺寸
    if (originalPixmap.width() > targetWidth || originalPixmap.height() > targetHeight) {
        pixmap = originalPixmap.scaled(targetWidth - 100, targetHeight - 200, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        qDebug() << "[MyMessageBox] Scaled image size:" << pixmap.size();
    } else {
        // 如果原图比较小，保持原始大小
        pixmap = originalPixmap;
    }

    imageLabel->setPixmap(pixmap);
    imageLabel->setMinimumSize(pixmap.size());
    imageLabel->show();
    qDebug() << "[MyMessageBox] Image set successfully";
    
    // 调整对话框大小以适应图片
    int dialogWidth = qMax(targetWidth, pixmap.width() + 100);  // 确保至少有目标宽度
    int dialogHeight = qMax(targetHeight, pixmap.height() + 200); // 确保至少有目标高度
    
    // 设置对话框的最小尺寸
    setMinimumSize(dialogWidth, dialogHeight);
    
    // 设置初始尺寸
    resize(dialogWidth, dialogHeight);
    
    qDebug() << "[MyMessageBox] Initial dialog size:" << size();
}

void MyMessageBox::resizeEvent(QResizeEvent *event)
{
    QDialog::resizeEvent(event);
    qDebug() << "[MyMessageBox] Dialog resized to:" << size();
} 