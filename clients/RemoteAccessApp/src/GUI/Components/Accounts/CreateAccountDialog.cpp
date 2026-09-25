#include "CreateAccountDialog.h"
#include "PasswordField.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QFileDialog>

CreateAccountDialog::CreateAccountDialog(QWidget *parent) : QDialog(parent)
{
    setObjectName("createAccountDialog");
    setWindowTitle("Thêm tài khoản");
    setWindowModality(Qt::WindowModal);
    resize(480, 340);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(12);
    auto *title = new QLabel("Thêm tài khoản", this);
    title->setProperty("role", "accountName");
    layout->addWidget(title);
    auto *nameLabel = new QLabel("Tên tài khoản", this);
    nameLabel->setProperty("role", "fieldLabel");
    m_name = new QLineEdit(this);
    m_name->setObjectName("createChildUsername");
    m_name->setProperty("role", "accountPassword");
    m_name->setMinimumHeight(38);
    m_name->setMaxLength(50);
    m_name->setAccessibleName("Tên tài khoản");
    nameLabel->setBuddy(m_name);
    layout->addWidget(nameLabel);
    layout->addWidget(m_name);
    auto *passwordLabel = new QLabel("Mật khẩu", this);
    passwordLabel->setProperty("role", "fieldLabel");
    auto *password = new PasswordField(this);
    m_password = password->input();
    m_password->setObjectName("createPassword");
    passwordLabel->setBuddy(m_password);
    layout->addWidget(passwordLabel);
    layout->addWidget(password);
    m_status = new QLabel(this);
    m_status->setProperty("role", "accountError");
    m_status->setTextFormat(Qt::PlainText);
    m_status->setWordWrap(true);
    m_status->hide();
    layout->addWidget(m_status);
    auto *actions = new QHBoxLayout;
    actions->addStretch();

    m_cancel = new QPushButton("Hủy", this);
    m_cancel->setProperty("role", "secondaryActionButton");
    m_cancel->setAutoDefault(false);

    m_save = new QPushButton("Thêm tài khoản", this);
    m_save->setObjectName("createAccountSubmit");
    m_save->setProperty("role", "primaryActionButton");
    m_save->setDefault(true);

    m_import = new QPushButton("Thêm danh sách tài khoản", this);
    m_import->setProperty("role", "TertiaryActionButton");
    m_import->setDefault(false);

    for (auto *button : {m_cancel, m_import, m_save}) {
        button->setMinimumHeight(38);
        button->setCursor(Qt::PointingHandCursor);
        actions->addWidget(button);
    }
    layout->addLayout(actions);

    m_filepath = new QLabel(this);
    m_filepath->setStyleSheet("color: #0284c7; font-size: 13px; margin-top: 6px;");
    m_filepath->setWordWrap(true);
    m_filepath->hide();

    layout->addWidget(m_filepath);

    connect(m_cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_save, &QPushButton::clicked, this, [this]() {
        if (!m_save->isEnabled()) return;
        if (m_selectedFilePath.isEmpty() && (m_name->text().trimmed().isEmpty() || m_password->text().trimmed().isEmpty())) {
            showError("Vui lòng nhập đầy đủ thông tin cần thiết hoặc thêm file.");
            return;
        }
        setBusy(true);
        emit registerRequested(m_name->text().trimmed(), m_password->text(), m_selectedFilePath);
    });
    connect(m_import, &QPushButton::clicked, this, &CreateAccountDialog::onImportButtonClicked);

    connect(this, &QDialog::finished, this, [this]() {
        m_password->clear();
        m_selectedFilePath.clear();
    });
    m_name->setFocus();
}

void CreateAccountDialog::onImportButtonClicked()
{
    QString filepath = QFileDialog::getOpenFileName(
        this,
        "Chọn file danh sách tài khoản",
        QString(),
        "File dữ liệu (*.csv *.xlsx *.xls);;Tất cả file (*)"
    );

    if (filepath.isEmpty())
        return;

    m_selectedFilePath = filepath;
    QFileInfo fileInfo(filepath);
    QString fileName = fileInfo.fileName();

    m_filepath->setText(QString("Đã chọn file: <b>%1</b>").arg(fileName));
    m_filepath->setToolTip(filepath);
    m_filepath->show();
}

void CreateAccountDialog::setBusy(bool busy)
{
    m_save->setEnabled(!busy);
    m_cancel->setEnabled(!busy);
    m_name->setEnabled(!busy);
    m_password->parentWidget()->setEnabled(!busy);
    m_save->setText(busy ? "Đang thêm…" : "Thêm tài khoản");
    m_status->hide();
}
void CreateAccountDialog::showError(const QString &message)
{
    setBusy(false);
    m_status->setText(message);
    m_status->show();
}
void CreateAccountDialog::reject()
{
    if (m_save->isEnabled()) QDialog::reject();
}
