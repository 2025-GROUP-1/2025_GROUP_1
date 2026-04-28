#include "AboutDialog.h"
#include "ui_aboutdialog.h"

#include <QPushButton>

AboutDialog::AboutDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::AboutDialog)
{
    ui->setupUi(this);

    connect(ui->okButton, &QPushButton::clicked,
            this, &AboutDialog::accept);
}

AboutDialog::~AboutDialog()
{
    delete ui;
}