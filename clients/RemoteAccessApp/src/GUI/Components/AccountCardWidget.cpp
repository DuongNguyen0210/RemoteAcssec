#include "AccountCardWidget.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>

AccountCardWidget::AccountCardWidget(const AccountInfo &info, QWidget *parent) : QFrame(parent)
{
    setProperty("role", "sessionCard");
    setAttribute(Qt::WA_StyledBackground, true);
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    auto *labels = new QVBoxLayout;
    auto *title = new QLabel(info.childUsername.isEmpty() ? info.username : info.childUsername, this);
    title->setTextFormat(Qt::PlainText);
    title->setProperty("role", "cardTitle");
    auto *username = new QLabel(info.username, this);
    username->setTextFormat(Qt::PlainText);
    username->setProperty("role", "cardSubtitle");
    labels->addWidget(title);
    labels->addWidget(username);
    layout->addLayout(labels, 1);
    auto *remove = new QPushButton(QStringLiteral("Delete account"), this);
    remove->setProperty("role", "dangerActionButton");
    remove->setFixedHeight(36);
    remove->setCursor(Qt::PointingHandCursor);
    layout->addWidget(remove);
    connect(remove, &QPushButton::clicked, this, [this, info]() { emit deleteRequested(info.username); });
}
