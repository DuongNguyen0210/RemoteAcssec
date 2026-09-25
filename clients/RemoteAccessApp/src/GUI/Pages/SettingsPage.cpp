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
#include <QSpinBox>
#include <QVBoxLayout>

namespace {

QLabel *label(const QString &text, const QString &objectName, QWidget *parent)
{
    QLabel *lbl = new QLabel(text, parent);
    lbl->setProperty("role", objectName);
    return lbl;
}

QLineEdit *lineEdit(const QString &text, QWidget *parent)
{
    QLineEdit *input = new QLineEdit(text, parent);
    input->setProperty("role", "settingsInput");
    input->setFixedHeight(38);
    return input;
}

QFrame *settingsCard(const QString &title, const QString &subtitle, QWidget *parent)
{
    QFrame *card = new QFrame(parent);
    card->setProperty("role", "settingsCard");
    card->setAttribute(Qt::WA_StyledBackground, true);

    QVBoxLayout *layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(14);
    layout->addWidget(label(title, "sectionTitle", card));
    layout->addWidget(label(subtitle, "sectionSubtitle", card));
    return card;
}

void addField(QGridLayout *grid, int row, const QString &caption, QWidget *field, QWidget *parent)
{
    QLabel *captionLabel = label(caption, "fieldLabel", parent);
    grid->addWidget(captionLabel, row, 0);
    grid->addWidget(field, row, 1);
}

QCheckBox *checkBox(const QString &text, bool checked, QWidget *parent)
{
    QCheckBox *box = new QCheckBox(text, parent);
    box->setProperty("role", "settingsCheckBox");
    box->setChecked(checked);
    box->setCursor(Qt::PointingHandCursor);
    return box;
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

    QVBoxLayout *header = new QVBoxLayout();
    header->setContentsMargins(0, 0, 0, 8);
    header->setSpacing(4);
    header->addWidget(label("Cài đặt", "pageTitle", this));
    header->addWidget(label("Thiết lập tài khoản và kết nối.", "pageSubtitle", this));
    mainLayout->addLayout(header);

    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setProperty("role", "scrollArea");
    scrollArea->setWidgetResizable(true);

    QWidget *scrollContent = new QWidget(scrollArea);
    scrollContent->setProperty("role", "scrollContent");
    scrollContent->setAttribute(Qt::WA_StyledBackground, true);
    QVBoxLayout *content = new QVBoxLayout(scrollContent);
    content->setContentsMargins(0, 0, 0, 0);
    content->setSpacing(16);

    QFrame *profileCard = settingsCard("Hồ sơ quản trị", "Thông tin người quản trị.", scrollContent);
    QGridLayout *profileGrid = new QGridLayout();
    profileGrid->setHorizontalSpacing(16);
    profileGrid->setVerticalSpacing(12);
    addField(profileGrid, 0, "Tên hiển thị", lineEdit("Quản trị viên", profileCard), profileCard);
    addField(profileGrid, 1, "Email", lineEdit("admin@company.com", profileCard), profileCard);
    addField(profileGrid, 2, "Đơn vị", lineEdit("Quản trị RemoteAccess", profileCard), profileCard);
    qobject_cast<QVBoxLayout *>(profileCard->layout())->addLayout(profileGrid);
    content->addWidget(profileCard);

    QFrame *securityCard = settingsCard("Bảo mật", "Quyền truy cập và thời gian chờ.", scrollContent);
    QGridLayout *securityGrid = new QGridLayout();
    securityGrid->setHorizontalSpacing(16);
    securityGrid->setVerticalSpacing(12);

    QSpinBox *timeout = new QSpinBox(securityCard);
    timeout->setProperty("role", "settingsInput");
    timeout->setRange(5, 240);
    timeout->setSuffix(" phút");
    timeout->setValue(30);
    timeout->setFixedHeight(38);
    addField(securityGrid, 0, "Thời gian chờ", timeout, securityCard);

    QComboBox *approval = new QComboBox(securityCard);
    approval->setProperty("role", "settingsInput");
    approval->addItems({"Hỏi trước khi điều khiển", "Chỉ xem", "Cho phép điều khiển"});
    approval->setFixedHeight(38);
    addField(securityGrid, 1, "Quyền điều khiển", approval, securityCard);

    qobject_cast<QVBoxLayout *>(securityCard->layout())->addLayout(securityGrid);
    qobject_cast<QVBoxLayout *>(securityCard->layout())->addWidget(checkBox("Yêu cầu xác thực hai bước", true, securityCard));
    qobject_cast<QVBoxLayout *>(securityCard->layout())->addWidget(checkBox("Lưu nhật ký phiên kết nối", true, securityCard));
    qobject_cast<QVBoxLayout *>(securityCard->layout())->addWidget(checkBox("Báo khi có kết nối không cần xác nhận", false, securityCard));
    content->addWidget(securityCard);

    QFrame *connectionCard = settingsCard("Kết nối", "Chất lượng hình ảnh và máy chủ.", scrollContent);
    QGridLayout *connectionGrid = new QGridLayout();
    connectionGrid->setHorizontalSpacing(16);
    connectionGrid->setVerticalSpacing(12);
    QComboBox *quality = new QComboBox(connectionCard);
    quality->setProperty("role", "settingsInput");
    quality->addItems({"Cân bằng", "Chất lượng cao", "Tiết kiệm băng thông"});
    quality->setFixedHeight(38);
    addField(connectionGrid, 0, "Chất lượng hình ảnh", quality, connectionCard);
    addField(connectionGrid, 1, "Máy chủ relay", lineEdit("relay.company.local", connectionCard), connectionCard);
    qobject_cast<QVBoxLayout *>(connectionCard->layout())->addLayout(connectionGrid);

    QPushButton *saveButton = new QPushButton("Lưu thay đổi", connectionCard);
    saveButton->setProperty("role", "primaryActionButton");
    saveButton->setCursor(Qt::PointingHandCursor);
    saveButton->setFixedHeight(40);
    qobject_cast<QVBoxLayout *>(connectionCard->layout())->addWidget(saveButton);
    content->addWidget(connectionCard);

    content->addStretch();
    scrollArea->setWidget(scrollContent);
    mainLayout->addWidget(scrollArea, 1);
    for (auto *button : findChildren<QPushButton*>()) {
        button->setEnabled(false);
        button->setText(button->text() + " · Sắp có");
        button->setToolTip("Sắp có");
    }

    for (auto *input : findChildren<QLineEdit*>()) input->setEnabled(false);
    for (auto *input : findChildren<QComboBox*>()) input->setEnabled(false);
    for (auto *input : findChildren<QSpinBox*>()) input->setEnabled(false);
    for (auto *input : findChildren<QCheckBox*>()) input->setEnabled(false);

}
