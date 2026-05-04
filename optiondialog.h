/** @file optiondialog.h
 *  @brief Declares OptionDialog — modal editor for part name, colour, visibility, and filters.
 */

#ifndef OPTIONDIALOG_H
#define OPTIONDIALOG_H

#include <QDialog>
#include <QColor>

#include "ModelPart.h"

/** @brief Auto-generated UI namespace — holds the form classes created by Qt Designer. */
namespace Ui {
    class OptionDialog;
}

/** @brief Modal dialog for editing a single part's name, colour, visibility, and filters. */
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
