/**
 * @file optiondialog.cpp
 * @brief Implements the per-part property dialog, including colour picking and state transfer.
 */

#include "optiondialog.h"
#include "ui_optiondialog.h"

#include <QColorDialog>

OptionDialog::OptionDialog(QWidget* parent)
    : QDialog(parent)
    , ui(new Ui::OptionDialog)
    , m_colour(255, 255, 255)
{
    ui->setupUi(this);
    updateSwatch();
}

OptionDialog::~OptionDialog() {
    delete ui;
}

void OptionDialog::loadFromModelPart(ModelPart* part) {
    if (!part)
        return;

    ui->nameEdit->setText(part->data(0).toString());
    ui->visibleCheck->setChecked(part->getVisible());
    ui->shrinkCheck->setChecked(part->getShrinkEnabled());
    ui->clipCheck->setChecked(part->getClipEnabled());

    m_colour = part->getColour();
    updateSwatch();
}

void OptionDialog::saveToModelPart(ModelPart* part) {
    if (!part)
        return;

    part->setData(0, QVariant(ui->nameEdit->text()));
    part->setColour(m_colour);
    part->setVisible(ui->visibleCheck->isChecked());
    part->setShrinkFilter(ui->shrinkCheck->isChecked());
    part->setClipFilter(ui->clipCheck->isChecked());
}

// opens the system colour picker
void OptionDialog::on_colourButton_clicked() {
    QColor chosen = QColorDialog::getColor(m_colour, this, tr("Select Colour"));
    if (chosen.isValid()) {
        m_colour = chosen;
        updateSwatch();
    }
}

// paints the swatch label to match the selected colour
void OptionDialog::updateSwatch() {
    ui->colourSwatch->setStyleSheet(
        QString("background-color: %1; border: 1px solid #444;")
        .arg(m_colour.name()));
}
