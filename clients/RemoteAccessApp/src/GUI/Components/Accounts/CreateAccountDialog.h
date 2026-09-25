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
    void onImportButtonClicked();
signals:
    void registerRequested(const QString &username, const QString &password, const QString &filepath);
private:
    void setBusy(bool busy);
    QLineEdit *m_name;
    QLineEdit *m_password;
    QLabel *m_status;
    QPushButton *m_import;
    QPushButton *m_save;
    QPushButton *m_cancel;
    QLabel* m_filepath;
    QString m_selectedFilePath;
};
