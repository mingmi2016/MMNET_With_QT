#include "mainwindow.h"
#include <QApplication>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QDir>
#include <QSettings>
#include <QDebug>

#if defined(Q_OS_WIN)
#include <QtGlobal>
Q_DECL_EXPORT
#endif

QFile *g_logFile = nullptr;
void myMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    if (!g_logFile) return;
    QTextStream out(g_logFile);
    QString time = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    QString txt;
    switch (type) {
    case QtDebugMsg:    txt = QString("[%1] Debug: %2").arg(time, msg); break;
    case QtWarningMsg:  txt = QString("[%1] Warning: %2").arg(time, msg); break;
    case QtCriticalMsg: txt = QString("[%1] Critical: %2").arg(time, msg); break;
    case QtFatalMsg:    txt = QString("[%1] Fatal: %2").arg(time, msg); break;
    case QtInfoMsg:     txt = QString("[%1] Info: %2").arg(time, msg); break;
    }
    out << txt << Qt::endl;
    out.flush();
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    // 读取配置文件，判断是否开发模式（直接文本读取，彻底规避QSettings问题）
    QString configPath = QDir::currentPath() + "/config.ini";
    bool isDevelopMode = true;
    QFile f(configPath);
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        while (!f.atEnd()) {
            QString line = f.readLine().trimmed();
            if (line.startsWith("DevelopMode", Qt::CaseInsensitive)) {
                QString value = line.section('=', 1).trimmed().toLower();
                isDevelopMode = (value == "true" || value == "1");
                break;
            }
        }
        f.close();
    }
    // 只有开发模式下才输出调试信息
    if (isDevelopMode) {
        qDebug() << "[DEBUG] isDevelopMode (manual parse):" << isDevelopMode;
        qDebug() << "[DEBUG] CurrentPath:" << QDir::currentPath();
        qDebug() << "[DEBUG] config.ini path:" << configPath;
        QFile f2(configPath);
        if (f2.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qDebug() << "[DEBUG] config.ini content:";
            while (!f2.atEnd()) {
                qDebug() << f2.readLine();
            }
            f2.close();
        } else {
            qDebug() << "[DEBUG] config.ini cannot be opened!";
        }
    }
    if (!isDevelopMode) {
        QDir().mkpath(QDir::currentPath() + "/MMNET");
        g_logFile = new QFile(QDir::currentPath() + "/MMNET/qt_app.log");
        g_logFile->open(QIODevice::Append | QIODevice::Text);
        qInstallMessageHandler(myMessageHandler);
    }
    MainWindow w(nullptr, isDevelopMode);
    w.show();
    int ret = a.exec();
    if (g_logFile) { g_logFile->close(); delete g_logFile; g_logFile = nullptr; }
    return ret;
}
