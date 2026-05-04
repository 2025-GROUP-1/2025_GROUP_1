// dialog for editing a single part's properties (name, colour, visibility, filters)

#ifndef OPTIONDIALOG_H
#define OPTIONDIALOG_H

#include <QDialog>
#include <QColor>

#include "ModelPart.h"

namespace Ui {
    class OptionDialog;
}

class OptionDialog : public QDialog {
    Q_OBJECT

public:
    explicit OptionDialog(QWidget* parent = nullptr);
    ~OptionDialog();

    // fills the dialog fields from the part's current state
    void loadFromModelPart(ModelPart* part);

    // writes the dialog values back into the part
    void saveToModelPart(ModelPart* part);

private slots:
    void on_colourButton_clicked();

private:
    // updates the little colour preview square
    void updateSwatch();

    Ui::OptionDialog* ui;
    QColor            m_colour;
};

#endif
