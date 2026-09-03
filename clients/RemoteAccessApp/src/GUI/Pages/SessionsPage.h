#ifndef SESSIONSPAGE_H
#define SESSIONSPAGE_H

#include <QWidget>

class SessionsPage : public QWidget
{
    Q_OBJECT
public:
    explicit SessionsPage(QWidget *parent = nullptr);

private:
    void setupUi();
};

#endif // SESSIONSPAGE_H
