#include "PasswordField.h"
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>

PasswordField::PasswordField(QWidget *parent) : QWidget(parent)
{
    auto *row = new QHBoxLayout(this);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(8);
    m_input = new QLineEdit(this);
    m_input->setProperty("role", "accountPassword");
    m_input->setEchoMode(QLineEdit::Password);
    m_input->setMaxLength(255);
    m_input->setMinimumHeight(38);
    m_input->setAccessibleName("Mật khẩu");
    auto *toggle = new QPushButton("Hiện", this);
    toggle->setObjectName("toggleNewPassword");
    toggle->setProperty("role", "secondaryActionButton");
    toggle->setMinimumSize(72, 38);
    toggle->setCursor(Qt::PointingHandCursor);
    toggle->setCheckable(true);
    toggle->setAutoDefault(false);
    toggle->setAccessibleName("Hiện mật khẩu");
    connect(toggle, &QPushButton::toggled, this, [this, toggle](bool shown) {
        m_input->setEchoMode(shown ? QLineEdit::Normal : QLineEdit::Password);
        toggle->setText(shown ? "Ẩn" : "Hiện");
        toggle->setAccessibleName(shown ? "Ẩn mật khẩu" : "Hiện mật khẩu");
    });
    row->addWidget(m_input, 1);
    row->addWidget(toggle);
    setFocusProxy(m_input);
}
