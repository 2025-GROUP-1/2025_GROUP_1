#ifndef OPTIONDIALOG_H
#define OPTIONDIALOG_H

#include <QDialog>
#include "ModelPart.h"

namespace Ui {
class OptionDialog;
}

class OptionDialog : public QDialog {
    Q_OBJECT

public:
    explicit OptionDialog(QWidget* parent = nullptr);
    ~OptionDialog();

    // Call this BEFORE exec() to populate fields from a ModelPart
    void loadFromModelPart(ModelPart* part);

    // Call this AFTER exec() returns Accepted to save fields to a ModelPart
    void saveToModelPart(ModelPart* part);

private:
    Ui::OptionDialog* ui;
};

#endif // OPTIONDIALOG_H
