#ifndef LOGSPAGE_H
#define LOGSPAGE_H

#include <QWidget>
#include <QString>
class QVBoxLayout;
class QLabel;

class LogsPage : public QWidget
{
    Q_OBJECT
public:
    explicit LogsPage(QWidget *parent = nullptr);
    void addEntry(const QString &event, const QString &detail, bool warning = false, quint64 sessionId = 0);

private:
    void setupUi();
    QVBoxLayout *m_entries = nullptr;
    QLabel *m_empty = nullptr;
    int m_entryCount = 0;
};

#endif
