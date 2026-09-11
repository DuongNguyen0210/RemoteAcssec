#ifndef ACCOUNTCARDWIDGET_H
#define ACCOUNTCARDWIDGET_H

#include <QFrame>
#include <QString>
#include "Domain/Model/DeviceInfo.h"

class AccountCardWidget : public QFrame
{
    Q_OBJECT
public:
    explicit AccountCardWidget(const DeviceInfo &info, QWidget *parent = nullptr);
    explicit AccountCardWidget(const QString &username, const QString &password,
                               const QString &role, const QString &status,
                               const QString &statusState, QWidget *parent = nullptr);

signals:
    void editRequested(const QString &username);
    void deleteRequested(const QString &username);

private:
    void initUi(const QString &username, const QString &password,
                const QString &role, const QString &status,
                const QString &statusState);

    QString m_username;
};

#endif
