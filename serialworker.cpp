#include "serialworker.h"

#include <QSerialPort>
#include <QDebug>

SerialWorker::SerialWorker(QObject *parent) : QObject(parent)
{
    serial = NULL;
    rx_state = IDLE;
    rx_step = 0;
    buildFrame = new CANFrame;
}

SerialWorker::~SerialWorker()
{
    if (serial != NULL) delete serial;
}

void SerialWorker::setSerialPort(QString portName)
{
    if (serial == NULL) serial = new QSerialPort(this);

    if (serial->isOpen())
    {
        serial->close();
    }
    else
    {
        qDebug() << "Serial port name is " << portName;
        serial->setPortName(portName);
        serial->open(QIODevice::ReadWrite);
        QByteArray output;
        output.append(0xE7);
        output.append(0xE7);
        serial->write(output);
        ///isConnected = true;
        connect(serial, SIGNAL(readyRead()), this, SLOT(readSerialData()));
    }
}

void SerialWorker::readSerialData()
{
    QByteArray data = serial->readAll();
    unsigned char c;
    qDebug() << (tr("Got data from serial. Len = %0").arg(data.length()));
    for (int i = 0; i < data.length(); i++)
    {
        c = data.at(i);
        procRXChar(c);
    }
}

void SerialWorker::procRXChar(unsigned char c)
{
    switch (rx_state)
    {
    case IDLE:
        if (c == 0xF1) rx_state = GET_COMMAND;
        break;
    case GET_COMMAND:
        switch (c)
        {
        case 0: //receiving a can frame
            rx_state = BUILD_CAN_FRAME;
            rx_step = 0;
            break;
        case 1: //we don't accept time sync commands from the firmware
            rx_state = IDLE;
            break;
        case 2: //process a return reply for digital input states.
            rx_state = GET_DIG_INPUTS;
            rx_step = 0;
            break;
        case 3: //process a return reply for analog inputs
            rx_state = GET_ANALOG_INPUTS;
            break;
        case 4: //we set digital outputs we don't accept replies so nothing here.
            rx_state = IDLE;
            break;
        case 5: //we set canbus specs we don't accept replies.
            rx_state = IDLE;
            break;
        }
        break;
    case BUILD_CAN_FRAME:
        switch (rx_step)
        {
        case 0:
            buildFrame->timestamp = c;
            break;
        case 1:
            buildFrame->timestamp |= (uint)(c << 8);
            break;
        case 2:
            buildFrame->timestamp |= (uint)c << 16;
            break;
        case 3:
            buildFrame->timestamp |= (uint)c << 24;
            break;
        case 4:
            buildFrame->ID = c;
            break;
        case 5:
            buildFrame->ID |= c << 8;
            break;
        case 6:
            buildFrame->ID |= c << 16;
            break;
        case 7:
            buildFrame->ID |= c << 24;
            if ((buildFrame->ID & 1 << 31) == 1 << 31)
            {
                buildFrame->ID &= 0x7FFFFFFF;
                buildFrame->extended = true;
            }
            else buildFrame->extended = false;
            break;
        case 8:
            buildFrame->len = c & 0xF;
            if (buildFrame->len > 8) buildFrame->len = 8;
            buildFrame->bus = (c & 0xF0) >> 4;
            break;
        default:
            if (rx_step < buildFrame->len + 9)
            {
                buildFrame->data[rx_step - 9] = c;
            }
            else
            {
                rx_state = IDLE;
                rx_step = 0;
                qDebug() << "emit from serial handler to main form id: " << buildFrame->ID;
                emit receivedFrame(buildFrame);
                buildFrame = new CANFrame;
            }
            break;
        }
        rx_step++;
        break;
    case GET_ANALOG_INPUTS: //get 9 bytes - 2 per analog input plus checksum
        switch (rx_step)
        {
        case 0:
            break;
        }
        rx_step++;
        break;
    case GET_DIG_INPUTS: //get two bytes. One for digital in status and one for checksum.
        switch (rx_step)
        {
        case 0:
            break;
        case 1:
            rx_state = IDLE;
            break;
        }
        rx_step++;
        break;
    }
}