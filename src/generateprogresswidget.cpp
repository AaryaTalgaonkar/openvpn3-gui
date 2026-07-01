#include "generateprogresswidget.h"

GenerateProgressWidget::GenerateProgressWidget(QWidget *parent)
    : QWidget(parent)
    , m_ui(new Ui::GenerateProgress)
{
    m_ui->setupUi(this);
}

GenerateProgressWidget::~GenerateProgressWidget()
{
    delete m_ui;
}

void GenerateProgressWidget::showProgress(bool show)
{
    m_ui->downloadProgress->setVisible(show);
    m_ui->downloadStatusLabel->setVisible(show);
    if (!show) {
        m_ui->downloadProgress->setValue(0);
        m_ui->downloadStatusLabel->clear();
    }
}

void GenerateProgressWidget::setValue(int value)
{
    m_ui->downloadProgress->setValue(value);
}

void GenerateProgressWidget::setStatusText(const QString &text)
{
    m_ui->downloadStatusLabel->setText(text);
}

void GenerateProgressWidget::reset()
{
    m_ui->downloadProgress->setValue(0);
    m_ui->downloadStatusLabel->clear();
    m_ui->downloadProgress->setVisible(false);
    m_ui->downloadStatusLabel->setVisible(false);
}