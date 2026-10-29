#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSerialPort>

#include "protocolimu.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class FrameParser;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onRefreshPorts();
    void onConnectToggled();
    void onFrameReady(quint8 type, const QByteArray &payload);
    void onSerialErrorOccurred(QSerialPort::SerialPortError error);

    void onSendMode();
    void onCalibAccel();
    void onCalibGyro();
    void onTimeSync();
    void onRequestTelemetry();
    void onReset();

private:
    void setConnected(bool connected);
    void appendLog(const QString &text);
    void sendFrame(const QByteArray &frame, const QString &description);
    void updateTelemetry(const ImuProtocol::TelemetryData &t);

    Ui::MainWindow *ui;

    QSerialPort *m_serial;
    FrameParser *m_parser;

    qint64 m_lastTimeSyncSentMs = 0;
};
#endif // MAINWINDOW_H
