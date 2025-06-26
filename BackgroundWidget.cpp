#include "BackgroundWidget.h"
#include <QPainter>

BackgroundWidget::BackgroundWidget(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
}

void BackgroundWidget::setBackground(const QPixmap &pix)
{
    m_pixmap = pix;
    update();
}

void BackgroundWidget::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    if (!m_pixmap.isNull())
        painter.drawPixmap(rect(), m_pixmap);
} 