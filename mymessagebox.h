#ifndef MYMESSAGEBOX_H
#define MYMESSAGEBOX_H

#include <QMessageBox>
#include <QResizeEvent>

class MyMessageBox : public QMessageBox
{
    Q_OBJECT
public:
    explicit MyMessageBox(QWidget *parent = nullptr);
    void setMySize(int width, int height);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    int _width;
    int _height;
};

#endif // MYMESSAGEBOX_H 