#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    TCP_Server = new QTcpServer();
    if(TCP_Server->listen(QHostAddress::Any,43000))
    {
        connect(TCP_Server, &QTcpServer::newConnection, this, &MainWindow::newConnection);
        ui->statusbar->showMessage("TCP Server Started");
    }
    else
    {
        QMessageBox::information(this, "TCP Server Error", TCP_Server->errorString());
    }

    connect(ui->pushButton_Send_Message, &QPushButton::clicked, this, &MainWindow::sendMessage);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::readSocket()
{
    // Receive File from client
    QTcpSocket *socket = reinterpret_cast<QTcpSocket*>(sender());

    QByteArray DataBuffer;

    QDataStream socketstream(socket);
    socketstream.setVersion(QDataStream::Qt_5_15);

    socketstream.startTransaction();
    socketstream >> DataBuffer; //Get Data from socket stream or from client socket

    if(socketstream.commitTransaction() == false)
    {
        return;
    }

    QString receivedMessage = QString::fromUtf8(DataBuffer);
    if (receivedMessage.startsWith("text:"))
    {
        receivedMessage.remove(0, 5); // Remove "text:" prefix

        qintptr clientDescriptor = socket->socketDescriptor();
        QString messageWithClientId = "Client " + QString::number(clientDescriptor) + ":\n" + receivedMessage + "\n";
        ui->textBrowser_Inbox->append(messageWithClientId);
    }
    else
    {
        // Get file header data
        QString HeaderData = DataBuffer.mid(0,128);
        QString Filename = HeaderData.split(",")[0].split(":")[1];
        QString FileExt = Filename.split(".")[1];
        QString FileSize = HeaderData.split(",")[1].split(":")[1];

        DataBuffer = DataBuffer.mid(128);   //Get File Data Only

        QString SaveFilePath = QCoreApplication::applicationDirPath() + "/" + Filename;
        QFile File(SaveFilePath);
        if(File.open(QIODevice::WriteOnly))
        {
            File.write(DataBuffer);
            File.close();
        }
    }
}

void MainWindow::discardSocket()
{
    //Remove Client from list when client is disconnected
    QTcpSocket *socket = reinterpret_cast<QTcpSocket*>(sender());

    int idx = Client_List.indexOf(socket);
    if(idx > -1)
    {
        ui->textBrowser_Server_Log->append("A client is disconnected from Server");
        Client_List.removeAt(idx);
    }

    ui->comboBox_Client_List->clear();

    foreach (QTcpSocket *sockettemp, Client_List)
    {
        ui->comboBox_Client_List->addItem(QString::number(sockettemp->socketDescriptor()));
    }

    socket->deleteLater();
}

void MainWindow::newConnection()
{
    while(TCP_Server->hasPendingConnections())
    {
        AddToSocketList(TCP_Server->nextPendingConnection());
    }
}

void MainWindow::AddToSocketList(QTcpSocket *socket)
{
    Client_List.append(socket);
    connect(socket, &QTcpSocket::readyRead, this, &MainWindow::readSocket);
    connect(socket, &QTcpSocket::disconnected, this, &MainWindow::discardSocket);
    ui->textBrowser_Server_Log->append("Client is connected with Server : Socket ID: " + QString::number(socket->socketDescriptor()));
    ui->comboBox_Client_List->addItem(QString::number(socket->socketDescriptor()));
}


void MainWindow::Send_File(QTcpSocket *socket, QString filename)
{
    if(socket)
    {
        if(socket->isOpen())
        {
            QFile filedata(filename);
            if(filedata.open(QIODevice::ReadOnly))
            {
                QFileInfo fileinfo(filedata);
                QString FileNameWithExt(fileinfo.fileName());

                QDataStream socketstream(socket);
                socketstream.setVersion(QDataStream::Qt_5_15);

                //This code use send file name also with file data
                QByteArray header;
                header.prepend("filename: " + FileNameWithExt.toUtf8() + ", filesize: " + QString::number(filedata.size()).toUtf8());
                header.resize(128); //Header data length

                // Add FileData
                QByteArray ByteFieldData = filedata.readAll();
                ByteFieldData.prepend(header);

                // Write to socket
                socketstream << ByteFieldData;
            }
            else
            {
                qDebug() << "File not open";
            }
        }
        else
        {
            qDebug() << "Client socket not open";
        }
    }
    else
    {
        qDebug() << "Client socket is invalid";
    }

}

void MainWindow::sendMessage()
{
    QString message = ui->textEdit_Message->toPlainText();

    if (ui->comboBox_Transfer_Type->currentText() == "Broadcast")
    {
        foreach (QTcpSocket *sockettemp, Client_List)
        {
            sendTextMessage(sockettemp, message);
        }
    }
    else if (ui->comboBox_Transfer_Type->currentText() == "receiver")
    {
        QString receiverId = ui->comboBox_Client_List->currentText();
        foreach (QTcpSocket *sockettemp, Client_List)
        {
            if (sockettemp->socketDescriptor() == receiverId.toLongLong())
            {
                sendTextMessage(sockettemp, message);
            }
        }
    }

    // Clear the message input after sending
    ui->textEdit_Message->clear();
}

void MainWindow::sendTextMessage(QTcpSocket *socket, const QString &message)
{
    if (socket && socket->isOpen())
    {
        QByteArray messageData = "text:" + message.toUtf8();

        QDataStream socketStream(socket);
        socketStream.setVersion(QDataStream::Qt_5_15);

        socketStream << messageData;
    }
}

void MainWindow::on_pushButton_Send_File_clicked()
{
    // Send File to the Client
    QString FilePath = QFileDialog::getOpenFileName(this, "Select File", QCoreApplication::applicationDirPath(), "File (*.jpg *.txt *.png *.bmp)");

    // Send File to All Connected Clients
    if(ui->comboBox_Transfer_Type->currentText() == "Broadcast")
    {
        foreach (QTcpSocket *sockettemp, Client_List)
        {
            Send_File(sockettemp, FilePath);
        }
    }
    else if(ui->comboBox_Transfer_Type->currentText() == "receiver")
    {
        // Send File to Selected Client
        QString receiverid = ui->comboBox_Client_List->currentText();
        foreach (QTcpSocket *sockettemp, Client_List)
        {
            if(sockettemp->socketDescriptor() == receiverid.toLongLong())
            {
                Send_File(sockettemp, FilePath);
            }

        }
    }
}
