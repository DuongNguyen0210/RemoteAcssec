#pragma once
#include <QWidget>
class QLineEdit;

// Presentation-only password input shared by account forms.
class PasswordField : public QWidget {
public:
    explicit PasswordField(QWidget *parent = nullptr);
    QLineEdit *input() const { return m_input; }
private:
    QLineEdit *m_input;
};
