#pragma once
#include <QFrame>
#include "Domain/Model/AccountInfo.h"
class AccountCardWidget : public QFrame {
    Q_OBJECT
public:
    explicit AccountCardWidget(const AccountInfo &info, QWidget *parent = nullptr);
signals:
    void deleteRequested(const QString &username);
};
