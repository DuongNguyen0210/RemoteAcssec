#ifndef CONFIRMDIALOG_H
#define CONFIRMDIALOG_H

#include <QDialog>
#include <QString>

class QLabel;
class QPushButton;
class QFrame;
class QKeyEvent;
class QShowEvent;

class ConfirmDialog : public QDialog
{
    Q_OBJECT
public:
    enum class Type {
        Danger,
        Question,
        Warning,
        Info
    };

    explicit ConfirmDialog(QWidget *parent = nullptr);
    ~ConfirmDialog() override = default;

    void setDialogType(Type type);
    void setTitleText(const QString &title);
    void setMessageText(const QString &message);
    void setConfirmButtonText(const QString &text);
    void setCancelButtonText(const QString &text);
    void setCancelButtonVisible(bool visible);

    static bool confirmDelete(QWidget *parent,
                              const QString &title,
                              const QString &message,
                              const QString &confirmText = QStringLiteral("Xóa"),
                              const QString &cancelText = QStringLiteral("Hủy"));

    static bool confirm(QWidget *parent,
                        const QString &title,
                        const QString &message,
                        const QString &confirmText = QStringLiteral("Xác nhận"),
                        const QString &cancelText = QStringLiteral("Hủy"));

    static void showWarning(QWidget *parent,
                            const QString &title,
                            const QString &message,
                            const QString &buttonText = QStringLiteral("Đóng"));

    static void showInfo(QWidget *parent,
                         const QString &title,
                         const QString &message,
                         const QString &buttonText = QStringLiteral("Đóng"));

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    void setupUI();
    void updateVisuals();

    Type m_type{Type::Question};
    QFrame *m_cardFrame{nullptr};
    QLabel *m_iconBadge{nullptr};
    QLabel *m_titleLabel{nullptr};
    QLabel *m_messageLabel{nullptr};
    QPushButton *m_cancelButton{nullptr};
    QPushButton *m_confirmButton{nullptr};
};

#endif
