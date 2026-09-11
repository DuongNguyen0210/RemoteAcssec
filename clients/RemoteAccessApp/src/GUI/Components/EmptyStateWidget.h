#ifndef EMPTYSTATEWIDGET_H
#define EMPTYSTATEWIDGET_H

#include <QWidget>
#include <QString>

class QLabel;
class QPushButton;

class EmptyStateWidget : public QWidget
{
    Q_OBJECT
public:
    explicit EmptyStateWidget(QWidget *parent = nullptr);
    EmptyStateWidget(const QString &title, const QString &description,
                     const QString &actionText = QString(), QWidget *parent = nullptr);

    void setTitle(const QString &title);
    void setDescription(const QString &description);
    void setActionText(const QString &actionText);
    void setActionVisible(bool visible);
    void setIconText(const QString &iconText);

signals:
    void actionClicked();

private:
    void setupUi();

    QLabel *m_iconLabel;
    QLabel *m_titleLabel;
    QLabel *m_descLabel;
    QPushButton *m_actionButton;
};

#endif
