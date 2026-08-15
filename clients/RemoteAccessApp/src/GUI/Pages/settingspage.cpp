#include "settingspage.h"

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
    lbl->setObjectName(objectName);
    return lbl;
}

QLineEdit *lineEdit(const QString &text, QWidget *parent)
{
    QLineEdit *input = new QLineEdit(text, parent);
    input->setObjectName("settingsInput");
    input->setFixedHeight(38);
    return input;
}

QFrame *settingsCard(const QString &title, const QString &subtitle, QWidget *parent)
{
    QFrame *card = new QFrame(parent);
    card->setObjectName("settingsCard");
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
    box->setObjectName("settingsCheckBox");
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
    header->addWidget(label("Settings", "pageTitle", this));
    header->addWidget(label("Configure administrator profile, security policy, and remote connection defaults.", "pageSubtitle", this));
    mainLayout->addLayout(header);

    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setObjectName("contentScrollArea");
    scrollArea->setWidgetResizable(true);

    QWidget *scrollContent = new QWidget(scrollArea);
    scrollContent->setObjectName("contentScrollContent");
    scrollContent->setAttribute(Qt::WA_StyledBackground, true);
    QVBoxLayout *content = new QVBoxLayout(scrollContent);
    content->setContentsMargins(0, 0, 0, 0);
    content->setSpacing(16);

    QFrame *profileCard = settingsCard("Administrator Profile", "Visible identity and contact details for audit records.", scrollContent);
    QGridLayout *profileGrid = new QGridLayout();
    profileGrid->setHorizontalSpacing(16);
    profileGrid->setVerticalSpacing(12);
    addField(profileGrid, 0, "Display name", lineEdit("IT Administrator", profileCard), profileCard);
    addField(profileGrid, 1, "Email", lineEdit("admin@company.com", profileCard), profileCard);
    addField(profileGrid, 2, "Organization", lineEdit("RemoteAccess Operations", profileCard), profileCard);
    qobject_cast<QVBoxLayout *>(profileCard->layout())->addLayout(profileGrid);
    content->addWidget(profileCard);

    QFrame *securityCard = settingsCard("Security Policy", "Defaults for remote sessions and console access.", scrollContent);
    QGridLayout *securityGrid = new QGridLayout();
    securityGrid->setHorizontalSpacing(16);
    securityGrid->setVerticalSpacing(12);

    QSpinBox *timeout = new QSpinBox(securityCard);
    timeout->setObjectName("settingsInput");
    timeout->setRange(5, 240);
    timeout->setSuffix(" min");
    timeout->setValue(30);
    timeout->setFixedHeight(38);
    addField(securityGrid, 0, "Idle timeout", timeout, securityCard);

    QComboBox *approval = new QComboBox(securityCard);
    approval->setObjectName("settingsInput");
    approval->addItems({"Ask before control", "Allow view only", "Allow full control"});
    approval->setFixedHeight(38);
    addField(securityGrid, 1, "Session approval", approval, securityCard);

    qobject_cast<QVBoxLayout *>(securityCard->layout())->addLayout(securityGrid);
    qobject_cast<QVBoxLayout *>(securityCard->layout())->addWidget(checkBox("Require two-factor authentication for administrator actions", true, securityCard));
    qobject_cast<QVBoxLayout *>(securityCard->layout())->addWidget(checkBox("Record session metadata for audit logs", true, securityCard));
    qobject_cast<QVBoxLayout *>(securityCard->layout())->addWidget(checkBox("Notify when a device starts an unattended session", false, securityCard));
    content->addWidget(securityCard);

    QFrame *connectionCard = settingsCard("Connection Defaults", "Network and quality preferences for new remote sessions.", scrollContent);
    QGridLayout *connectionGrid = new QGridLayout();
    connectionGrid->setHorizontalSpacing(16);
    connectionGrid->setVerticalSpacing(12);
    QComboBox *quality = new QComboBox(connectionCard);
    quality->setObjectName("settingsInput");
    quality->addItems({"Balanced", "High quality", "Low bandwidth"});
    quality->setFixedHeight(38);
    addField(connectionGrid, 0, "Stream quality", quality, connectionCard);
    addField(connectionGrid, 1, "Relay server", lineEdit("relay.company.local", connectionCard), connectionCard);
    qobject_cast<QVBoxLayout *>(connectionCard->layout())->addLayout(connectionGrid);

    QPushButton *saveButton = new QPushButton("Save Changes", connectionCard);
    saveButton->setObjectName("primaryActionButton");
    saveButton->setCursor(Qt::PointingHandCursor);
    saveButton->setFixedHeight(40);
    qobject_cast<QVBoxLayout *>(connectionCard->layout())->addWidget(saveButton);
    content->addWidget(connectionCard);

    content->addStretch();
    scrollArea->setWidget(scrollContent);
    mainLayout->addWidget(scrollArea, 1);
}
