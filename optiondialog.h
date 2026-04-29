/**
 * @file optiondialog.h
 * @brief Modal dialog for editing per-part properties.
 *
 * Lets the user change the part name, pick a colour through QColorDialog,
 * toggle visibility, and toggle the shrink and clip filters.
 */

#ifndef OPTIONDIALOG_H
#define OPTIONDIALOG_H

#include <QDialog>
#include <QColor>

#include "ModelPart.h"

namespace Ui {
    class OptionDialog;
}

/**
 * @class OptionDialog
 * @brief Dialog used by the right-click "Item Options" action.
 *
 * Workflow: construct dialog, call loadFromModelPart(), exec() the dialog,
 * and on QDialog::Accepted call saveToModelPart() to write changes back.
 */
class OptionDialog : public QDialog {
    Q_OBJECT

public:
    /**
     * @brief Construct the dialog.
     * @param parent Parent widget.
     */
    explicit OptionDialog(QWidget* parent = nullptr);

    /** @brief Destructor. */
    ~OptionDialog();

    /**
     * @brief Populate dialog widgets from a ModelPart's current state.
     * @param part Pointer to the part being edited.
     */
    void loadFromModelPart(ModelPart* part);

    /**
     * @brief Write the dialog values back into a ModelPart.
     * @param part Pointer to the part being edited.
     */
    void saveToModelPart(ModelPart* part);

private slots:
    /** @brief Open QColorDialog when the colour button is pressed. */
    void on_colourButton_clicked();

private:
    /** @brief Repaint the swatch to show the currently selected colour. */
    void updateSwatch();

    Ui::OptionDialog* ui;        ///< Generated UI from the .ui file.
    QColor            m_colour;  ///< Current colour selected in the dialog.
};

#endif