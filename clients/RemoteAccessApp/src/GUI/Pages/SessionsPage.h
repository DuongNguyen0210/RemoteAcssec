#ifndef SESSIONSPAGE_H
#define SESSIONSPAGE_H

#include <QWidget>
#include <QImage>
class QFrame;

class SessionsPage : public QWidget
{
    Q_OBJECT
public:
    explicit SessionsPage(QWidget *parent = nullptr);
    void setFrame(const QImage &image);
    void clearFrame();
    void showSession();
private:
    void setupUi();
    QWidget *m_viewer = nullptr;
    QFrame *m_sessionCard = nullptr;
};
#endif
