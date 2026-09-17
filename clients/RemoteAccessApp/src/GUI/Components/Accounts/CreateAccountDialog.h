#pragma once
#include <QDialog>
class QLineEdit;
class QLabel;
class QPushButton;

class CreateAccountDialog : public QDialog {
    Q_OBJECT
public:
    explicit CreateAccountDialog(QWidget *parent = nullptr);
    void showError(const QString &message);
    void reject() override;
signals:
    void registerRequested(const QString &username, const QString &password);
private:
    void setBusy(bool busy);
    QLineEdit *m_name;
    QLineEdit *m_password;
    QLabel *m_status;
    QPushButton *m_save;
    QPushButton *m_cancel;
};
