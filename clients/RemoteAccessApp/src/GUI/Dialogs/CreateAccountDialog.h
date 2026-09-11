#ifndef CREATEACCOUNTDIALOG_H
#define CREATEACCOUNTDIALOG_H

#include <QDialog>
#include <QString>

class QLineEdit;
class QLabel;

class CreateAccountDialog : public QDialog
{
    Q_OBJECT
public:
    explicit CreateAccountDialog(QWidget *parent = nullptr);
    ~CreateAccountDialog();

    void showError(const QString &msg);
    void showSuccess(const QString &msg);

signals:
    void registerRequested(const QString &username, const QString &password);

private slots:
    void onRegisterClicked();

private:
    void setupUI();

    QLineEdit *m_usernameInput;
    QLineEdit *m_passwordInput;
    QLabel *m_statusLabel;
};

#endif
