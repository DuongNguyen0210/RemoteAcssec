#include "SettingsPage.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QSysInfo>

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
    header->addWidget(label("Cài đặt hệ thống", "pageTitle", this));
    header->addWidget(label("Quản lý tài khoản quản trị, kết nối máy chủ và tùy chọn điều khiển.", "pageSubtitle", this));

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
    m_emailLabel = label("Đang tải...", "fieldValue", profileCard);
    addField(profileGrid, 0, "Tên tài khoản:", m_usernameLabel, profileCard);
    addField(profileGrid, 1, "Email liên kết:", m_emailLabel, profileCard);

    m_oldPasswordInput = lineEdit("Nhập mật khẩu hiện tại", profileCard, true);
    m_newPasswordInput = lineEdit("Mật khẩu mới (tối thiểu 6 ký tự)", profileCard, true);
    m_confirmPasswordInput = lineEdit("Nhập lại mật khẩu mới", profileCard, true);

    addField(profileGrid, 2, "Mật khẩu hiện tại:", m_oldPasswordInput, profileCard);
    addField(profileGrid, 3, "Mật khẩu mới:", m_newPasswordInput, profileCard);
    addField(profileGrid, 4, "Xác nhận mật khẩu:", m_confirmPasswordInput, profileCard);

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

    // ==========================================
    // Card 2: Kết nối Máy chủ (Server Connection)
    // ==========================================
    QFrame *serverCard = settingsCard("Kết nối Máy chủ", "Địa chỉ API của máy chủ điều khiển.", scrollContent);
    QGridLayout *serverGrid = new QGridLayout();
    serverGrid->setHorizontalSpacing(16);
    serverGrid->setVerticalSpacing(12);

    m_serverUrlInput = lineEdit("http://localhost:9090", serverCard);
    addField(serverGrid, 0, "Địa chỉ Server API:", m_serverUrlInput, serverCard);
    qobject_cast<QVBoxLayout *>(serverCard->layout())->addLayout(serverGrid);

    QHBoxLayout *serverBtnLayout = new QHBoxLayout();
    serverBtnLayout->addStretch();

    m_btnResetServer = new QPushButton("Khôi phục mặc định", serverCard);
    m_btnResetServer->setCursor(Qt::PointingHandCursor);
    m_btnResetServer->setFixedHeight(38);
    connect(m_btnResetServer, &QPushButton::clicked, this, &SettingsPage::onResetServerClicked);
    serverBtnLayout->addWidget(m_btnResetServer);

    m_btnSaveServer = new QPushButton("Lưu địa chỉ máy chủ", serverCard);
    m_btnSaveServer->setProperty("role", "primaryActionButton");
    m_btnSaveServer->setCursor(Qt::PointingHandCursor);
    m_btnSaveServer->setFixedHeight(38);
    connect(m_btnSaveServer, &QPushButton::clicked, this, &SettingsPage::onSaveServerClicked);
    serverBtnLayout->addWidget(m_btnSaveServer);

    qobject_cast<QVBoxLayout *>(serverCard->layout())->addLayout(serverBtnLayout);
    content->addWidget(serverCard);

    // ==========================================
    // Card 3: Chất lượng Truyền phát & Hiển thị
    // ==========================================
    QFrame *streamCard = settingsCard("Truyền phát & Màn hình", "Tùy chỉnh nén ảnh JPEG và cách hiển thị màn hình từ xa.", scrollContent);
    QGridLayout *streamGrid = new QGridLayout();
    streamGrid->setHorizontalSpacing(16);
    streamGrid->setVerticalSpacing(12);

    m_qualityCombo = new QComboBox(streamCard);
    m_qualityCombo->setProperty("role", "settingsInput");
    m_qualityCombo->addItems({"Tiết kiệm băng thông (Chất lượng 50%)",
                              "Cân bằng (Chất lượng 70% - Khuyên dùng)",
                              "Chất lượng cao (Chất lượng 85%)"});
    m_qualityCombo->setCurrentIndex(1);
    m_qualityCombo->setFixedHeight(38);
    addField(streamGrid, 0, "Chất lượng hình ảnh:", m_qualityCombo, streamCard);

    m_keepAspectCheck = new QCheckBox("Giữ nguyên tỉ lệ khung hình (Khuyên dùng)", streamCard);
    m_keepAspectCheck->setChecked(true);
    m_keepAspectCheck->setCursor(Qt::PointingHandCursor);
    addField(streamGrid, 1, "Chế độ co giãn:", m_keepAspectCheck, streamCard);

    qobject_cast<QVBoxLayout *>(streamCard->layout())->addLayout(streamGrid);

    QHBoxLayout *streamBtnLayout = new QHBoxLayout();
    streamBtnLayout->addStretch();
    m_btnSaveStream = new QPushButton("Áp dụng tùy chọn", streamCard);
    m_btnSaveStream->setProperty("role", "primaryActionButton");
    m_btnSaveStream->setCursor(Qt::PointingHandCursor);
    m_btnSaveStream->setFixedHeight(38);
    connect(m_btnSaveStream, &QPushButton::clicked, this, &SettingsPage::onSaveStreamClicked);
    streamBtnLayout->addWidget(m_btnSaveStream);

    qobject_cast<QVBoxLayout *>(streamCard->layout())->addLayout(streamBtnLayout);
    content->addWidget(streamCard);

    // ==========================================
    // Card 4: Thông tin Ứng dụng & Môi trường
    // ==========================================
    QFrame *infoCard = settingsCard("Thông tin Ứng dụng", "Đồ án PBL4 - Hệ thống Quản trị & Điều khiển máy tính từ xa.", scrollContent);
    QGridLayout *infoGrid = new QGridLayout();
    infoGrid->setHorizontalSpacing(16);
    infoGrid->setVerticalSpacing(8);

    addField(infoGrid, 0, "Phiên bản:", label("v1.0.0 (Phase 6)", "fieldValue", infoCard), infoCard);
    addField(infoGrid, 1, "Giao thức truyền hình ảnh:", label("RDTP qua Netty Relay Proxy", "fieldValue", infoCard), infoCard);
    addField(infoGrid, 2, "Cơ chế đồng bộ thiết bị:", label("Server-Sent Events (SSE) Realtime", "fieldValue", infoCard), infoCard);
    addField(infoGrid, 3, "Hệ điều hành hiện tại:", label(QSysInfo::prettyProductName(), "fieldValue", infoCard), infoCard);

    qobject_cast<QVBoxLayout *>(infoCard->layout())->addLayout(infoGrid);
    content->addWidget(infoCard);

    content->addStretch();
    scrollArea->setWidget(scrollContent);
    mainLayout->addWidget(scrollArea, 1);
}

void SettingsPage::setProfileInfo(const QString &username, const QString &email)
{
    if (m_usernameLabel) m_usernameLabel->setText(username);
    if (m_emailLabel) m_emailLabel->setText(email);
}

void SettingsPage::setServerUrl(const QString &url)
{
    if (m_serverUrlInput) m_serverUrlInput->setText(url);
}

void SettingsPage::setStreamSettings(int qualityIndex, bool keepAspectRatio)
{
    if (m_qualityCombo && qualityIndex >= 0 && qualityIndex < m_qualityCombo->count()) {
        m_qualityCombo->setCurrentIndex(qualityIndex);
    }
    if (m_keepAspectCheck) {
        m_keepAspectCheck->setChecked(keepAspectRatio);
    }
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

void SettingsPage::onSaveServerClicked()
{
    emit saveServerRequested(m_serverUrlInput ? m_serverUrlInput->text() : QString());
}

void SettingsPage::onResetServerClicked()
{
    emit resetServerRequested();
}

void SettingsPage::onSaveStreamClicked()
{
    emit saveStreamSettingsRequested(
        m_qualityCombo ? m_qualityCombo->currentIndex() : 1,
        m_keepAspectCheck ? m_keepAspectCheck->isChecked() : true
    );
}
