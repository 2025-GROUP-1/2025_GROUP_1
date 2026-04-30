#include "optiondialog.h"
#include "ui_optiondialog.h"

OptionDialog::OptionDialog(QWidget* parent)
    : QDialog(parent)
    , ui(new Ui::OptionDialog)
{
    ui->setupUi(this);
}

OptionDialog::~OptionDialog() {
    delete ui;
}

void OptionDialog::loadFromModelPart(ModelPart* part) {
    if (part == nullptr) return;

    // Populate the name field
    ui->lineEditName->setText(part->data(0).toString());

    // Populate the colour spinboxes
    ui->spinBoxR->setValue(part->getColourR());
    ui->spinBoxG->setValue(part->getColourG());
    ui->spinBoxB->setValue(part->getColourB());

    // Populate the visibility checkbox
    ui->checkBoxVisible->setChecked(part->getVisible());
}

void OptionDialog::saveToModelPart(ModelPart* part) {
    if (part == nullptr) return;

    // Save name back into itemData column 0
    part->setData(0, QVariant(ui->lineEditName->text()));

    // Save colour
    part->setColour(
        ui->spinBoxR->value(),
        ui->spinBoxG->value(),
        ui->spinBoxB->value()
        );

    // Save visibility
    part->setVisible(ui->checkBoxVisible->isChecked());
}
