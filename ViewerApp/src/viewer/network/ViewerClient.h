#ifndef VIEWERCLIENT_H
#define VIEWERCLIENT_H

#include <QByteArray>
#include <QObject>

class QString;
class QTcpSocket;

class ViewerClient : public QObject
{
    Q_OBJECT

public:
    explicit ViewerClient(QObject *parent = nullptr);

    void connectToHost(const QString &ip, quint16 port);

signals:
    void connected();
    void disconnected();
    void frameReceived(const QByteArray &imageBuffer);

private slots:
    void onReadyRead();

private:
    QTcpSocket *socket;
    quint32 expectedImageSize;
    QByteArray imageBuffer;
};

#endif
