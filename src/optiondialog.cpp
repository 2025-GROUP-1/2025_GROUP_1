#include "optiondialog.h"
#include "ui_optiondialog.h"

#include <QColorDialog>
#include <QPushButton>

OptionDialog::OptionDialog(QWidget* parent)
    : QDialog(parent)
    , ui(new Ui::OptionDialog)
    , m_currentColour(Qt::white)
{
    ui->setupUi(this);

    connect(ui->colourButton, &QPushButton::clicked,
            this, &OptionDialog::onColourButtonClicked);

    updateColourSwatch();
}

OptionDialog::~OptionDialog()
{
    delete ui;
}

void OptionDialog::loadFromModelPart(ModelPart* part)
{
    if (!part) return;

    ui->nameEdit->setText(part->data(0).toString());
    ui->visibleCheck->setChecked(part->getVisible());
    ui->shrinkCheck->setChecked(part->getShrinkEnabled());
    ui->clipCheck->setChecked(part->getClipEnabled());

    m_currentColour = QColor(
        part->getColourR(),
        part->getColourG(),
        part->getColourB()
    );

    updateColourSwatch();
}

void OptionDialog::saveToModelPart(ModelPart* part)
{
    if (!part) return;

    part->setData(0, ui->nameEdit->text());

    bool visible = ui->visibleCheck->isChecked();
    part->setVisible(visible);
    part->setData(1, visible ? "true" : "false");
    part->setShrinkEnabled(ui->shrinkCheck->isChecked());
    part->setClipEnabled(ui->clipCheck->isChecked());

    part->setColour(
        m_currentColour.red(),
        m_currentColour.green(),
        m_currentColour.blue()
    );
}

void OptionDialog::onColourButtonClicked()
{
    QColor chosen = QColorDialog::getColor(
        m_currentColour,
        this,
        tr("Choose part colour")
    );

    if (!chosen.isValid()) return;

    m_currentColour = chosen;
    updateColourSwatch();
}

void OptionDialog::updateColourSwatch()
{
    ui->colourSwatch->setStyleSheet(
        QString("background-color: %1; border: 1px solid black;")
            .arg(m_currentColour.name())
    );
}