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

    void loadFromModelPart(ModelPart* part);
    void saveToModelPart(ModelPart* part);

private slots:
    void onColourButtonClicked();

private:
    Ui::OptionDialog* ui;
    QColor m_currentColour;

    void updateColourSwatch();
};

#endif // OPTIONDIALOG_H