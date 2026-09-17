#pragma once
#include <QDialog>
#include "Domain/Model/AccountInfo.h"
class QLineEdit;
class QLabel;
class QPushButton;

class EditAccountDialog : public QDialog {
    Q_OBJECT
public:
    explicit EditAccountDialog(const AccountInfo &info, QWidget *parent = nullptr);
    void reject() override;
    void showError(const QString &message);
signals:
    void saveRequested(qint64 id, const QString &childUsername, const QString &newPassword);
private:
    QLineEdit *m_name;
    QLineEdit *m_password;
    QLabel *m_status;
    QPushButton *m_save;
    QPushButton *m_cancel;
};
