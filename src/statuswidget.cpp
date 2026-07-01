#include "statuswidget.h"

StatusWidget::StatusWidget(QWidget *parent)
    : QWidget(parent)
    , m_ui(new Ui::StatusWidget)
{
    m_ui->setupUi(this);

    connect(m_ui->generateButton, &QPushButton::clicked,
            this, &StatusWidget::generateClicked);
    connect(m_ui->renewButton, &QPushButton::clicked,
            this, &StatusWidget::renewClicked);
}

StatusWidget::~StatusWidget()
{
    delete m_ui;
}

void StatusWidget::showGenerateMode()
{
    m_ui->certStatusLabel->setVisible(false);
    m_ui->renewButton->setVisible(false);
    m_ui->generateButton->setVisible(true);
}

void StatusWidget::showStatusPill(const QString &text, const QString &textColor, const QString &bgColor)
{
    m_ui->generateButton->setVisible(false);
    m_ui->renewButton->setVisible(false);
    m_ui->certStatusLabel->setVisible(true);

    m_ui->certStatusLabel->setText(text);
    m_ui->certStatusLabel->setStyleSheet(
        QStringLiteral("background-color: %1; color: %2; border: 1px solid %2; border-radius: 10px; padding: 0 10px;")
            .arg(bgColor, textColor));
}

void StatusWidget::showExpiredMode()
{
    m_ui->certStatusLabel->setVisible(false);
    m_ui->generateButton->setVisible(false);
    m_ui->renewButton->setVisible(true);
}

void StatusWidget::setGenerateEnabled(bool enabled)
{
    m_ui->generateButton->setEnabled(enabled);
}

void StatusWidget::setGenerateButtonText(const QString &text)
{
    m_ui->generateButton->setText(text);
}