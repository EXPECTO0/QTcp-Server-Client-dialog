#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    TCP_Socket = new QTcpSocket();

    connect(TCP_Socket, &QTcpSocket::readyRead, this, &MainWindow::readSocket);
    connect(TCP_Socket, &QTcpSocket::disconnected, this, &MainWindow::discardSocket);

    QString IPAddrress = "192.168.111.6";
    TCP_Socket->connectToHost(QHostAddress(IPAddrress), 43000);
    if(TCP_Socket->waitForConnected(30000))
    {
        ui->statusbar->showMessage("Socket (Client) is connected");
    }
    else
    {
        ui->statusbar->showMessage("Socket (Client) is not connected: " + TCP_Socket->errorString());
    }

    connect(ui->pushButton_Send_Message, &QPushButton::clicked, this, [this]() {
        QString message = ui->textEdit_Message->toPlainText();
        sendTextMessage(message);
        ui->textEdit_Message->clear();  // Clear the message input after sending
    });

    connect(ui->pushButton_Send_File, &QPushButton::clicked, this, &MainWindow::on_pushButton_Send_File_clicked);

}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::readSocket()
{
    QByteArray DataBuffer;

    QDataStream socketstream(TCP_Socket);
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
        ui->textBrowser_Inbox->append("Server: \n" + receivedMessage + "\n");

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
    TCP_Socket->deleteLater();
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

void MainWindow::sendTextMessage(const QString &message)
{
    if (TCP_Socket && TCP_Socket->isOpen())
    {
        QByteArray messageData = "text:" + message.toUtf8();

        QDataStream socketStream(TCP_Socket);
        socketStream.setVersion(QDataStream::Qt_5_15);

        socketStream << messageData;
    }
}

void MainWindow::on_pushButton_Send_File_clicked()
{
    if(TCP_Socket)
    {
        if(TCP_Socket->isOpen())
        {
            // Browse file and send to server
            QString FilePath = QFileDialog::getOpenFileName(this, "Select File", QCoreApplication::applicationDirPath(), "File (*.jpg *.txt *.png *.bmp)");

            Send_File(TCP_Socket, FilePath);
        }
    }
}



