#include "SettingsPage.h"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace {

QLabel *label(const QString &text, const QString &objectName, QWidget *parent)
{
    QLabel *lbl = new QLabel(text, parent);
    lbl->setProperty("role", objectName);
    return lbl;
}

QLineEdit *lineEdit(const QString &placeholder, QWidget *parent, bool isPassword = false)
{
    QLineEdit *input = new QLineEdit(parent);
    input->setProperty("role", "settingsInput");
    input->setPlaceholderText(placeholder);
    input->setFixedHeight(38);
    input->setMaxLength(255);
    if (isPassword) {
        input->setEchoMode(QLineEdit::Password);
    }
    return input;
}

QFrame *settingsCard(const QString &title, const QString &subtitle, QWidget *parent)
{
    QFrame *card = new QFrame(parent);
    card->setProperty("role", "settingsCard");
    card->setAttribute(Qt::WA_StyledBackground, true);

    QVBoxLayout *layout = new QVBoxLayout(card);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(12);
    layout->addWidget(label(title, "sectionTitle", card));
    layout->addWidget(label(subtitle, "sectionSubtitle", card));
    return card;
}

void addField(QGridLayout *grid, int row, const QString &caption, QWidget *field, QWidget *parent)
{
    QLabel *captionLabel = label(caption, "fieldLabel", parent);
    captionLabel->setMinimumWidth(140);
    grid->addWidget(captionLabel, row, 0);
    grid->addWidget(field, row, 1);
}

}

SettingsPage::SettingsPage(QWidget *parent)
    : QWidget{parent}
{
    setupUi();
}

void SettingsPage::setupUi()
{
    setObjectName("settingsPage");
    setAttribute(Qt::WA_StyledBackground, true);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(24, 24, 24, 24);
    mainLayout->setSpacing(16);

    // Header
    QVBoxLayout *header = new QVBoxLayout();
    header->setContentsMargins(0, 0, 0, 8);
    header->setSpacing(4);
    header->addWidget(label("Tài khoản của tôi", "pageTitle", this));
    header->addWidget(label("Thông tin đăng nhập và bảo vệ tài khoản của bạn.", "pageSubtitle", this));

    m_feedbackLabel = new QLabel(this);
    m_feedbackLabel->setVisible(false);
    m_feedbackLabel->setWordWrap(true);
    header->addWidget(m_feedbackLabel);

    mainLayout->addLayout(header);

    // Scroll Area
    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setProperty("role", "scrollArea");
    scrollArea->setWidgetResizable(true);

    QWidget *scrollContent = new QWidget(scrollArea);
    scrollContent->setProperty("role", "scrollContent");
    scrollContent->setAttribute(Qt::WA_StyledBackground, true);

    QVBoxLayout *content = new QVBoxLayout(scrollContent);
    content->setContentsMargins(0, 0, 0, 0);
    content->setSpacing(16);

    // ==========================================
    // Card 1: Tài khoản & Đổi mật khẩu
    // ==========================================
    QFrame *profileCard = settingsCard("Tài khoản Quản trị", "Thông tin đăng nhập và đổi mật khẩu.", scrollContent);
    QGridLayout *profileGrid = new QGridLayout();
    profileGrid->setHorizontalSpacing(16);
    profileGrid->setVerticalSpacing(12);

    m_usernameLabel = label("Đang tải...", "fieldValue", profileCard);
    addField(profileGrid, 0, "Tên tài khoản:", m_usernameLabel, profileCard);

    m_oldPasswordInput = lineEdit("Nhập mật khẩu hiện tại", profileCard, true);
    m_newPasswordInput = lineEdit("Mật khẩu mới (tối thiểu 6 ký tự)", profileCard, true);
    m_confirmPasswordInput = lineEdit("Nhập lại mật khẩu mới", profileCard, true);

    addField(profileGrid, 1, "Mật khẩu hiện tại:", m_oldPasswordInput, profileCard);
    addField(profileGrid, 2, "Mật khẩu mới:", m_newPasswordInput, profileCard);
    addField(profileGrid, 3, "Xác nhận mật khẩu:", m_confirmPasswordInput, profileCard);

    qobject_cast<QVBoxLayout *>(profileCard->layout())->addLayout(profileGrid);

    QHBoxLayout *pwdBtnLayout = new QHBoxLayout();
    pwdBtnLayout->addStretch();
    m_btnChangePassword = new QPushButton("Cập nhật mật khẩu", profileCard);
    m_btnChangePassword->setProperty("role", "primaryActionButton");
    m_btnChangePassword->setCursor(Qt::PointingHandCursor);
    m_btnChangePassword->setFixedHeight(38);
    connect(m_btnChangePassword, &QPushButton::clicked, this, &SettingsPage::onApplyPasswordClicked);
    pwdBtnLayout->addWidget(m_btnChangePassword);

    qobject_cast<QVBoxLayout *>(profileCard->layout())->addLayout(pwdBtnLayout);
    content->addWidget(profileCard);

    content->addStretch();
    scrollArea->setWidget(scrollContent);
    mainLayout->addWidget(scrollArea, 1);
}

void SettingsPage::setProfileInfo(const QString &username, const QString &email)
{
    if (m_usernameLabel) m_usernameLabel->setText(username);
    Q_UNUSED(email);
}

void SettingsPage::clearPasswordInputs()
{
    if (m_oldPasswordInput) m_oldPasswordInput->clear();
    if (m_newPasswordInput) m_newPasswordInput->clear();
    if (m_confirmPasswordInput) m_confirmPasswordInput->clear();
}

void SettingsPage::showFeedback(const QString &message, bool isError)
{
    if (!m_feedbackLabel) return;
    m_feedbackLabel->setText(message);
    m_feedbackLabel->setStyleSheet(isError 
        ? QStringLiteral("color: #e53935; font-weight: 500; font-size: 13px; padding: 6px 0;")
        : QStringLiteral("color: #43a047; font-weight: 500; font-size: 13px; padding: 6px 0;"));
    m_feedbackLabel->setVisible(true);
}

void SettingsPage::onApplyPasswordClicked()
{
    emit changePasswordRequested(
        m_oldPasswordInput ? m_oldPasswordInput->text() : QString(),
        m_newPasswordInput ? m_newPasswordInput->text() : QString(),
        m_confirmPasswordInput ? m_confirmPasswordInput->text() : QString()
    );
}

void SettingsPage::setPasswordChangePending(bool pending)
{
    for (auto *input : {m_oldPasswordInput, m_newPasswordInput, m_confirmPasswordInput})
        input->setEnabled(!pending);
    m_btnChangePassword->setEnabled(!pending);
    m_btnChangePassword->setText(pending ? "Đang cập nhật…" : "Cập nhật mật khẩu");
}
