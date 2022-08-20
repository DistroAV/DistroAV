/********************************************************************************
** Form generated from reading UI file 'SettingsDialog.ui'
**
** Created by: Qt User Interface Compiler version 5.12.8
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_SETTINGSDIALOG_H
#define UI_SETTINGSDIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QVBoxLayout>

QT_BEGIN_NAMESPACE

class Ui_SettingsDialog
{
public:
    QVBoxLayout *verticalLayout;
    QGroupBox *settingsGroupBox;
    QGridLayout *gridLayout;
    QFormLayout *formLayout;
    QLabel *label;
    QLineEdit *ndiExtraIpsLineEdit;
    QGroupBox *outputsGroupBox;
    QGridLayout *gridLayout_2;
    QGroupBox *programOutputGroupBox;
    QFormLayout *formLayout_2;
    QLabel *programOutputSenderNameLabel;
    QLineEdit *programOutputSenderNameLineEdit;
    QGroupBox *previewOutputGroupBox;
    QFormLayout *formLayout_3;
    QLabel *previewOutputSenderNameLabel;
    QLineEdit *previewOutputSenderNameLineEdit;
    QDialogButtonBox *buttonBox;

    void setupUi(QDialog *SettingsDialog)
    {
        if (SettingsDialog->objectName().isEmpty())
            SettingsDialog->setObjectName(QString::fromUtf8("SettingsDialog"));
        SettingsDialog->resize(699, 336);
        verticalLayout = new QVBoxLayout(SettingsDialog);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        settingsGroupBox = new QGroupBox(SettingsDialog);
        settingsGroupBox->setObjectName(QString::fromUtf8("settingsGroupBox"));
        gridLayout = new QGridLayout(settingsGroupBox);
        gridLayout->setObjectName(QString::fromUtf8("gridLayout"));
        formLayout = new QFormLayout();
        formLayout->setObjectName(QString::fromUtf8("formLayout"));
        label = new QLabel(settingsGroupBox);
        label->setObjectName(QString::fromUtf8("label"));

        formLayout->setWidget(0, QFormLayout::LabelRole, label);

        ndiExtraIpsLineEdit = new QLineEdit(settingsGroupBox);
        ndiExtraIpsLineEdit->setObjectName(QString::fromUtf8("ndiExtraIpsLineEdit"));

        formLayout->setWidget(0, QFormLayout::FieldRole, ndiExtraIpsLineEdit);


        gridLayout->addLayout(formLayout, 0, 0, 1, 1);


        verticalLayout->addWidget(settingsGroupBox);

        outputsGroupBox = new QGroupBox(SettingsDialog);
        outputsGroupBox->setObjectName(QString::fromUtf8("outputsGroupBox"));
        gridLayout_2 = new QGridLayout(outputsGroupBox);
        gridLayout_2->setObjectName(QString::fromUtf8("gridLayout_2"));
        programOutputGroupBox = new QGroupBox(outputsGroupBox);
        programOutputGroupBox->setObjectName(QString::fromUtf8("programOutputGroupBox"));
        programOutputGroupBox->setCheckable(true);
        programOutputGroupBox->setChecked(false);
        formLayout_2 = new QFormLayout(programOutputGroupBox);
        formLayout_2->setObjectName(QString::fromUtf8("formLayout_2"));
        programOutputSenderNameLabel = new QLabel(programOutputGroupBox);
        programOutputSenderNameLabel->setObjectName(QString::fromUtf8("programOutputSenderNameLabel"));

        formLayout_2->setWidget(0, QFormLayout::LabelRole, programOutputSenderNameLabel);

        programOutputSenderNameLineEdit = new QLineEdit(programOutputGroupBox);
        programOutputSenderNameLineEdit->setObjectName(QString::fromUtf8("programOutputSenderNameLineEdit"));

        formLayout_2->setWidget(0, QFormLayout::FieldRole, programOutputSenderNameLineEdit);


        gridLayout_2->addWidget(programOutputGroupBox, 0, 0, 1, 1);

        previewOutputGroupBox = new QGroupBox(outputsGroupBox);
        previewOutputGroupBox->setObjectName(QString::fromUtf8("previewOutputGroupBox"));
        previewOutputGroupBox->setCheckable(true);
        previewOutputGroupBox->setChecked(false);
        formLayout_3 = new QFormLayout(previewOutputGroupBox);
        formLayout_3->setObjectName(QString::fromUtf8("formLayout_3"));
        previewOutputSenderNameLabel = new QLabel(previewOutputGroupBox);
        previewOutputSenderNameLabel->setObjectName(QString::fromUtf8("previewOutputSenderNameLabel"));

        formLayout_3->setWidget(0, QFormLayout::LabelRole, previewOutputSenderNameLabel);

        previewOutputSenderNameLineEdit = new QLineEdit(previewOutputGroupBox);
        previewOutputSenderNameLineEdit->setObjectName(QString::fromUtf8("previewOutputSenderNameLineEdit"));

        formLayout_3->setWidget(0, QFormLayout::FieldRole, previewOutputSenderNameLineEdit);


        gridLayout_2->addWidget(previewOutputGroupBox, 1, 0, 1, 1);


        verticalLayout->addWidget(outputsGroupBox);

        buttonBox = new QDialogButtonBox(SettingsDialog);
        buttonBox->setObjectName(QString::fromUtf8("buttonBox"));
        buttonBox->setOrientation(Qt::Horizontal);
        buttonBox->setStandardButtons(QDialogButtonBox::Apply|QDialogButtonBox::Cancel|QDialogButtonBox::Ok);

        verticalLayout->addWidget(buttonBox);


        retranslateUi(SettingsDialog);
        QObject::connect(buttonBox, SIGNAL(accepted()), SettingsDialog, SLOT(accept()));
        QObject::connect(buttonBox, SIGNAL(rejected()), SettingsDialog, SLOT(reject()));

        QMetaObject::connectSlotsByName(SettingsDialog);
    } // setupUi

    void retranslateUi(QDialog *SettingsDialog)
    {
        SettingsDialog->setWindowTitle(QApplication::translate("SettingsDialog", "OBSNdi.SettingsDialog.Title", nullptr));
        settingsGroupBox->setTitle(QApplication::translate("SettingsDialog", "Settings.SettingsGroupBox.Title", nullptr));
        label->setText(QApplication::translate("SettingsDialog", "Settings.SettingsGroupBox.NdiExtraIps", nullptr));
        outputsGroupBox->setTitle(QApplication::translate("SettingsDialog", "Settings.OutputsGroupBox.Title", nullptr));
        programOutputGroupBox->setTitle(QApplication::translate("SettingsDialog", "Settings.ProgramOutputGroupBox.Title", nullptr));
        programOutputSenderNameLabel->setText(QApplication::translate("SettingsDialog", "Settings.ProgramOutputGroupBox.SenderName", nullptr));
        previewOutputGroupBox->setTitle(QApplication::translate("SettingsDialog", "Settings.PreviewOutputGroupBox.Title", nullptr));
        previewOutputSenderNameLabel->setText(QApplication::translate("SettingsDialog", "Settings.PreviewOutputGroupBox.SenderName", nullptr));
    } // retranslateUi

};

namespace Ui {
    class SettingsDialog: public Ui_SettingsDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_SETTINGSDIALOG_H
