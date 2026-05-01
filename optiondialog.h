#ifndef OPTIONDIALOG_H
#define OPTIONDIALOG_H

#include <QDialog>
#include "ModelPart.h"

namespace Ui {
class OptionDialog;
}

/**
 * @brief Modal dialog for viewing and editing the properties of a single ModelPart.
 *
 * Presents editable controls for the part's display name, RGB colour (three
 * spin boxes, 0–255 each), and visibility checkbox.
 *
 * Typical usage:
 * @code
 *   OptionDialog dialog(this);
 *   dialog.loadFromModelPart(selectedPart);   // populate fields
 *   if (dialog.exec() == QDialog::Accepted) {
 *       dialog.saveToModelPart(selectedPart); // apply changes
 *   }
 * @endcode
 *
 * @see ModelPart, MainWindow::handleButton2()
 */
class OptionDialog : public QDialog {
    Q_OBJECT

public:
    /**
     * @brief Constructs the dialog and initialises the generated UI.
     * @param parent Optional parent widget.
     */
    explicit OptionDialog(QWidget* parent = nullptr);

    /**
     * @brief Destructor. Deletes the generated UI object.
     */
    ~OptionDialog();

    /**
     * @brief Populates all dialog fields from the given ModelPart.
     *
     * Sets the name line edit to ModelPart::data(0), the R/G/B spin boxes to
     * the stored colour components, and the visibility checkbox to
     * ModelPart::getVisible().  Call this before exec().
     *
     * @param part The part whose properties are displayed; does nothing if \c nullptr.
     * @see saveToModelPart()
     */
    void loadFromModelPart(ModelPart* part);

    /**
     * @brief Writes the dialog's current field values back to the given ModelPart.
     *
     * Saves the edited name (via ModelPart::setData()), colour (via
     * ModelPart::setColour()), and visibility (via ModelPart::setVisible()).
     * Call this after exec() returns QDialog::Accepted.
     *
     * @param part The part to update; does nothing if \c nullptr.
     * @see loadFromModelPart()
     */
    void saveToModelPart(ModelPart* part);

private:
    Ui::OptionDialog* ui; ///< Generated UI class that owns all dialog widgets.
};

#endif // OPTIONDIALOG_H
