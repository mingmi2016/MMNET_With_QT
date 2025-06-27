#ifndef MYMESSAGEBOX_H
#define MYMESSAGEBOX_H

#include <QDialog>
#include <QResizeEvent>
#include <QLabel>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QPixmap>
#include <QFileInfo>
#include <QPushButton>
#include <QMessageBox>

class MyMessageBox : public QDialog
{
    Q_OBJECT
public:
    explicit MyMessageBox(QWidget *parent = nullptr);
    void setMySize(int width, int height);
    void setImage(const QString &imagePath);
    void setText(const QString &text);
    void setIcon(QMessageBox::Icon icon);
    void setWindowTitle(const QString &title);
    void setStandardButtons(QMessageBox::StandardButtons buttons);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    int _width;
    int _height;
    QLabel *imageLabel;
    QLabel *textLabel;
    QLabel *iconLabel;
    QVBoxLayout *mainLayout;
    QPushButton *okButton;
};

#endif // MYMESSAGEBOX_H 