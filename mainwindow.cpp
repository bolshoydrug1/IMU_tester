#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include "frameparser.h"

#include <QDateTime>
#include <QIntValidator>
#include <QMessageBox>
#include <QSerialPortInfo>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_serial(new QSerialPort(this))
    , m_parser(new FrameParser(this))
{
    ui->setupUi(this);

    ui->baudCombo->setValidator(new QIntValidator(1, 6000000, ui->baudCombo));
    ui->baudCombo->setCurrentText(QStringLiteral("1000000")); // скорость USART1 по умолчанию в прошивке

    ui->modeCombo->addItem(QStringLiteral("100 Гц"), ImuProtocol::Mode100Hz);
    ui->modeCombo->addItem(QStringLiteral("250 Гц"), ImuProtocol::Mode250Hz);
    ui->modeCombo->addItem(QStringLiteral("500 Гц"), ImuProtocol::Mode500Hz);
    ui->modeCombo->addItem(QStringLiteral("1000 Гц"), ImuProtocol::Mode1000Hz);
    ui->modeCombo->addItem(QStringLiteral("По запросу"), ImuProtocol::ModeOnRequest);
    ui->modeCombo->setCurrentIndex(2); // 500 Гц — режим по умолчанию в прошивке

    setConnected(false);
    onRefreshPorts();

    connect(ui->refreshButton, &QPushButton::clicked, this, &MainWindow::onRefreshPorts);
    connect(ui->connectButton, &QPushButton::clicked, this, &MainWindow::onConnectToggled);
    connect(ui->sendModeButton, &QPushButton::clicked, this, &MainWindow::onSendMode);
    connect(ui->requestTelemetryButton, &QPushButton::clicked, this, &MainWindow::onRequestTelemetry);
    connect(ui->timeSyncButton, &QPushButton::clicked, this, &MainWindow::onTimeSync);
    connect(ui->calibAccelButton, &QPushButton::clicked, this, &MainWindow::onCalibAccel);
    connect(ui->calibGyroButton, &QPushButton::clicked, this, &MainWindow::onCalibGyro);
    connect(ui->resetButton, &QPushButton::clicked, this, &MainWindow::onReset);

    connect(m_serial, &QSerialPort::readyRead, this, [this]() {
        m_parser->addData(m_serial->readAll());
    });
    connect(m_serial, &QSerialPort::errorOccurred, this, &MainWindow::onSerialErrorOccurred);
    connect(m_parser, &FrameParser::frameReady, this, &MainWindow::onFrameReady);
}

MainWindow::~MainWindow()
{
    if (m_serial->isOpen()) m_serial->close();
    delete ui;
}

void MainWindow::onRefreshPorts()
{
    const QString previouslySelected = ui->portCombo->currentData().toString();

    ui->portCombo->clear();
    const auto ports = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : ports) {
        QString text = info.portName();
        if (!info.description().isEmpty()) {
            text += QStringLiteral(" (%1)").arg(info.description());
        }
        ui->portCombo->addItem(text, info.portName());
    }

    if (ports.isEmpty()) {
        appendLog(QStringLiteral("Доступные COM-порты не найдены."));
        return;
    }

    const int idx = ui->portCombo->findData(previouslySelected);
    if (idx >= 0) ui->portCombo->setCurrentIndex(idx);
}

void MainWindow::onConnectToggled()
{
    if (m_serial->isOpen()) {
        m_serial->close();
        m_parser->reset();
        setConnected(false);
        appendLog(QStringLiteral("Отключено."));
        return;
    }

    const QString portName = ui->portCombo->currentData().toString();
    if (portName.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Подключение"),
                              QStringLiteral("Выберите COM-порт."));
        return;
    }

    bool baudOk = false;
    const qint32 baudRate = ui->baudCombo->currentText().toInt(&baudOk);
    if (!baudOk || baudRate <= 0) {
        QMessageBox::warning(this, QStringLiteral("Подключение"),
                              QStringLiteral("Некорректная скорость порта."));
        return;
    }

    m_serial->setPortName(portName);
    m_serial->setBaudRate(baudRate);
    m_serial->setDataBits(QSerialPort::Data8);
    m_serial->setParity(QSerialPort::NoParity);
    m_serial->setStopBits(QSerialPort::OneStop);
    m_serial->setFlowControl(QSerialPort::NoFlowControl);

    if (!m_serial->open(QIODevice::ReadWrite)) {
        QMessageBox::critical(this, QStringLiteral("Подключение"),
                               QStringLiteral("Не удалось открыть порт %1: %2")
                                   .arg(portName, m_serial->errorString()));
        return;
    }

    m_parser->reset();
    setConnected(true);
    appendLog(QStringLiteral("Подключено к %1 на скорости %2 бод.").arg(portName).arg(baudRate));
}

void MainWindow::setConnected(bool connected)
{
    ui->connectButton->setText(connected ? QStringLiteral("Отключить") : QStringLiteral("Подключить"));
    ui->statusLabel->setText(connected ? QStringLiteral("Подключено") : QStringLiteral("Не подключено"));
    ui->statusLabel->setStyleSheet(connected ? QStringLiteral("color: #2a2; font-weight: bold;")
                                              : QStringLiteral("color: #a33; font-weight: bold;"));

    ui->portCombo->setEnabled(!connected);
    ui->baudCombo->setEnabled(!connected);
    ui->refreshButton->setEnabled(!connected);

    ui->sendModeButton->setEnabled(connected);
    ui->requestTelemetryButton->setEnabled(connected);
    ui->timeSyncButton->setEnabled(connected);
    ui->calibAccelButton->setEnabled(connected);
    ui->calibGyroButton->setEnabled(connected);
    ui->resetButton->setEnabled(connected);
}

void MainWindow::appendLog(const QString &text)
{
    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz"));
    ui->log->appendPlainText(QStringLiteral("[%1] %2").arg(timestamp, text));
}

void MainWindow::sendFrame(const QByteArray &frame, const QString &description)
{
    if (!m_serial->isOpen()) {
        appendLog(QStringLiteral("Ошибка: нет подключения к устройству."));
        return;
    }

    const qint64 written = m_serial->write(frame);
    if (written != frame.size()) {
        appendLog(QStringLiteral("Ошибка отправки: %1").arg(m_serial->errorString()));
        return;
    }

    appendLog(QStringLiteral("-> %1").arg(description));
}

void MainWindow::updateTelemetry(const ImuProtocol::TelemetryData &t)
{
    ui->gyroXValue->setText(QString::number(t.gyroXDps, 'f', 3));
    ui->gyroYValue->setText(QString::number(t.gyroYDps, 'f', 3));
    ui->gyroZValue->setText(QString::number(t.gyroZDps, 'f', 3));
    ui->accelXValue->setText(QString::number(t.accelXG, 'f', 3));
    ui->accelYValue->setText(QString::number(t.accelYG, 'f', 3));
    ui->accelZValue->setText(QString::number(t.accelZG, 'f', 3));
    ui->rollValue->setText(QString::number(t.rollDeg, 'f', 2));
    ui->pitchValue->setText(QString::number(t.pitchDeg, 'f', 2));
    ui->yawValue->setText(QString::number(t.yawDeg, 'f', 2));
    ui->timestampValue->setText(QString::number(t.timestampMs));
}

void MainWindow::onFrameReady(quint8 type, const QByteArray &payload)
{
    using namespace ImuProtocol;

    switch (type) {
    case TypeTelemetry: {
        TelemetryData telemetry;
        if (TelemetryData::fromPayload(payload, &telemetry)) {
            updateTelemetry(telemetry);
        } else {
            appendLog(QStringLiteral("Ошибка разбора кадра телеметрии (неверная длина)."));
        }
        break;
    }
    case TypeAckNack: {
        AckNack ack;
        if (AckNack::fromPayload(payload, &ack)) {
            appendLog(QStringLiteral("Подтверждение команды «%1»: %2")
                          .arg(typeName(ack.ackType),
                               ack.status == StatusOk ? QStringLiteral("ОК") : QStringLiteral("ОШИБКА")));
        }
        break;
    }
    case TypeTimeSync: {
        TimeSyncResponse resp;
        if (TimeSyncResponse::fromPayload(payload, &resp)) {
            const qint64 rtt = QDateTime::currentMSecsSinceEpoch() - m_lastTimeSyncSentMs;
            appendLog(QStringLiteral("Ответ синхронизации времени: тик устройства=%1 мс, задержка=%2 мс")
                          .arg(resp.deviceTickMs).arg(rtt));
        }
        break;
    }
    case TypeCalibAccel:
    case TypeCalibGyro: {
        CalibResponse calib;
        if (CalibResponse::fromPayload(payload, &calib)) {
            const QString unit = (type == TypeCalibAccel) ? QStringLiteral("g") : QStringLiteral("°/с");
            appendLog(QStringLiteral("%1: %2, смещения X=%3 Y=%4 Z=%5 %6")
                          .arg(typeName(type),
                               calib.status == StatusOk ? QStringLiteral("ОК") : QStringLiteral("ОШИБКА"))
                          .arg(calib.biasX, 0, 'f', 4)
                          .arg(calib.biasY, 0, 'f', 4)
                          .arg(calib.biasZ, 0, 'f', 4)
                          .arg(unit));
        }
        break;
    }
    case TypeAsciiMsg: {
        appendLog(QStringLiteral("Устройство: %1").arg(QString::fromLatin1(payload).trimmed()));
        break;
    }
    default:
        appendLog(QStringLiteral("Получен кадр неизвестного/неожиданного типа 0x%1")
                      .arg(type, 2, 16, QChar('0')));
        break;
    }
}

void MainWindow::onSerialErrorOccurred(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError) return;

    appendLog(QStringLiteral("Ошибка порта: %1").arg(m_serial->errorString()));

    if (error == QSerialPort::ResourceError && m_serial->isOpen()) {
        m_serial->close();
        setConnected(false);
    }
}

void MainWindow::onSendMode()
{
    const quint8 mode = static_cast<quint8>(ui->modeCombo->currentData().toUInt());
    sendFrame(ImuProtocol::buildModeConfig(mode),
              QStringLiteral("Установка режима передачи: %1").arg(ui->modeCombo->currentText()));
}

void MainWindow::onCalibAccel()
{
    const auto answer = QMessageBox::question(
        this, QStringLiteral("Калибровка акселерометра"),
        QStringLiteral("Запустить калибровку акселерометра?\nУстройство должно оставаться "
                        "неподвижным на время калибровки."));
    if (answer != QMessageBox::Yes) return;

    sendFrame(ImuProtocol::buildFrame(ImuProtocol::TypeCalibAccel),
              QStringLiteral("Запрос калибровки акселерометра"));
}

void MainWindow::onCalibGyro()
{
    const auto answer = QMessageBox::question(
        this, QStringLiteral("Калибровка гироскопа"),
        QStringLiteral("Запустить калибровку гироскопа?\nУстройство должно оставаться "
                        "неподвижным на время калибровки."));
    if (answer != QMessageBox::Yes) return;

    sendFrame(ImuProtocol::buildFrame(ImuProtocol::TypeCalibGyro),
              QStringLiteral("Запрос калибровки гироскопа"));
}

void MainWindow::onTimeSync()
{
    m_lastTimeSyncSentMs = QDateTime::currentMSecsSinceEpoch();
    sendFrame(ImuProtocol::buildTimeSyncRequest(static_cast<quint64>(m_lastTimeSyncSentMs)),
              QStringLiteral("Запрос синхронизации времени"));
}

void MainWindow::onRequestTelemetry()
{
    sendFrame(ImuProtocol::buildFrame(ImuProtocol::TypeTelemetry),
              QStringLiteral("Запрос телеметрии (режим «по запросу»)"));
}

void MainWindow::onReset()
{
    const auto answer = QMessageBox::warning(
        this, QStringLiteral("Перезагрузка устройства"),
        QStringLiteral("Вы уверены, что хотите перезагрузить устройство?\nСоединение будет разорвано."),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes) return;

    sendFrame(ImuProtocol::buildResetRequest(), QStringLiteral("Команда перезагрузки устройства"));
}
