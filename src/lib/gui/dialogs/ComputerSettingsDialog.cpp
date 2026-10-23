/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2012 Synergy App Ltd
 * SPDX-FileCopyrightText: (C) 2008 Volker Lanz <vl@fidra.de>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ComputerSettingsDialog.h"
#include "ui_ComputerSettingsDialog.h"

#include "gui/config/Computer.h"
#include "validators/AliasValidator.h"
#include "validators/ComputerLineNameValidator.h"
#include "validators/ValidationError.h"

#include <QMessageBox>

#include <common/Settings.h>

using enum ComputerConfig::SwitchCorner;
using enum ComputerConfig::Fix;

namespace {
void setupModifierCombo(QComboBox *combo, int target, const QString &inheritLabel = {})
{
  combo->clear();
  if (!inheritLabel.isEmpty())
    combo->addItem(inheritLabel, static_cast<int>(KeyboardModifier::DefaultMod));
  const QStringList labels = {QObject::tr("Shift"), QObject::tr("Ctrl"),   QObject::tr("Alt"),
                              QObject::tr("Meta"),  QObject::tr("Super"),  QObject::tr("Alt Gr"),
                              QObject::tr("None"),  QObject::tr("Hangul"), QObject::tr("Hanja")};
  for (int value = 0; value < labels.size(); value++)
    combo->addItem(labels.at(value), value);
  if (combo->findData(target) == -1)
    combo->addItem(Computer::modifierTargetToOption(target), target);
  combo->setCurrentIndex(combo->findData(target));
}
} // namespace

ComputerSettingsDialog::~ComputerSettingsDialog() = default;

ComputerSettingsDialog::ComputerSettingsDialog(QWidget *parent, Computer *computer, const ComputerList *computers)
    : QDialog(parent, Qt::WindowTitleHint | Qt::WindowSystemMenuHint),
      ui{std::make_unique<Ui::ComputerSettingsDialog>()},
      m_computer(computer)
{

  ui->setupUi(this);
  ui->buttonBox->button(QDialogButtonBox::Cancel)->setFocus();

  ui->lineNameEdit->setText(m_computer->name());

  const auto valNameError = new validators::ValidationError(this, ui->lblNameError);
  const auto valName = new validators::ComputerLineNameValidator(ui->lineNameEdit, valNameError, computers);
  ui->lineNameEdit->setValidator(valName);

  const auto valAliasError = new validators::ValidationError(this, ui->lblAliasError);
  const auto valAlias = new validators::AliasValidator(ui->lineAddAlias, valAliasError);
  ui->lineAddAlias->setValidator(valAlias);

  for (int i = 0; i < m_computer->aliases().count(); i++)
    new QListWidgetItem(m_computer->aliases()[i], ui->listAliases);

  setupModifierCombo(ui->comboShift, m_computer->modifier(static_cast<int>(KeyboardModifier::Shift)));
  setupModifierCombo(ui->comboCtrl, m_computer->modifier(static_cast<int>(KeyboardModifier::Ctrl)));
  setupModifierCombo(ui->comboAlt, m_computer->modifier(static_cast<int>(KeyboardModifier::Alt)));
  setupModifierCombo(ui->comboMeta, m_computer->modifier(static_cast<int>(KeyboardModifier::Meta)));
  setupModifierCombo(ui->comboSuper, m_computer->modifier(static_cast<int>(KeyboardModifier::Super)));
  setupModifierCombo(ui->comboAltGr, m_computer->modifier(static_cast<int>(KeyboardModifier::AltGr)));
  setupModifierCombo(
      ui->comboMetaLeft, m_computer->sideModifierOverride(KeyboardModifierSide::MetaLeft), tr("Inherit Meta")
  );
  setupModifierCombo(
      ui->comboMetaRight, m_computer->sideModifierOverride(KeyboardModifierSide::MetaRight), tr("Inherit Meta")
  );
  setupModifierCombo(
      ui->comboSuperLeft, m_computer->sideModifierOverride(KeyboardModifierSide::SuperLeft), tr("Inherit Super")
  );
  setupModifierCombo(
      ui->comboSuperRight, m_computer->sideModifierOverride(KeyboardModifierSide::SuperRight), tr("Inherit Super")
  );

  ui->chkDeadTopLeft->setChecked(m_computer->switchCorner(static_cast<int>(TopLeft)));
  ui->chkDeadTopRight->setChecked(m_computer->switchCorner(static_cast<int>(TopRight)));
  ui->chkDeadBottomLeft->setChecked(m_computer->switchCorner(static_cast<int>(BottomLeft)));
  ui->chkDeadBottomRight->setChecked(m_computer->switchCorner(static_cast<int>(BottomRight)));
  ui->sbSwitchCornerSize->setValue(m_computer->switchCornerSize());

  ui->chkWeakX11Focus->setChecked(Settings::value(Settings::Computer::WeakX11Focus.arg(m_computer->name())).toBool());

  ui->chkFixCapsLock->setChecked(m_computer->fix(CapsLock));
  ui->chkFixNumLock->setChecked(m_computer->fix(NumLock));
  ui->chkFixScrollLock->setChecked(m_computer->fix(ScrollLock));
  ui->chkFixXTest->setChecked(m_computer->fix(XTest));

  connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &ComputerSettingsDialog::accept);
  connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &ComputerSettingsDialog::reject);
  connect(ui->btnAddAlias, &QPushButton::clicked, this, &ComputerSettingsDialog::addAlias);
  connect(ui->btnRemoveAlias, &QPushButton::clicked, this, &ComputerSettingsDialog::removeAlias);
  connect(ui->lineAddAlias, &QLineEdit::textChanged, this, &ComputerSettingsDialog::checkNewAliasName);
  connect(ui->listAliases, &QListWidget::itemSelectionChanged, this, &ComputerSettingsDialog::aliasSelected);
}

void ComputerSettingsDialog::accept()
{
  if (ui->lineNameEdit->text().isEmpty()) {
    QMessageBox::warning(
        this, tr("Computer name is empty"),
        tr("The computer name cannot be empty. "
           "Please either fill in a name or cancel the dialog.")
    );
    return;
  }
  if (!ui->lblNameError->text().isEmpty()) {
    return;
  }

  m_computer->setName(ui->lineNameEdit->text());

  m_computer->aliases().clear();

  for (int i = 0; i < ui->listAliases->count(); i++) {
    QString alias(ui->listAliases->item(i)->text());
    if (alias == ui->lineNameEdit->text()) {
      QMessageBox::warning(
          this, tr("Computer name matches alias"),
          tr("The computer name cannot be the same as an alias. "
             "Please either remove the alias or change the computer name.")
      );
      return;
    }
    if (!m_computer->aliases().contains(alias))
      m_computer->addAlias(alias);
  }

  m_computer->setModifier(KeyboardModifier::Shift, ui->comboShift->currentData().toInt());
  m_computer->setModifier(KeyboardModifier::Ctrl, ui->comboCtrl->currentData().toInt());
  m_computer->setModifier(KeyboardModifier::Alt, ui->comboAlt->currentData().toInt());
  m_computer->setModifier(KeyboardModifier::Meta, ui->comboMeta->currentData().toInt());
  m_computer->setModifier(KeyboardModifier::Super, ui->comboSuper->currentData().toInt());
  m_computer->setModifier(KeyboardModifier::AltGr, ui->comboAltGr->currentData().toInt());
  m_computer->setSideModifier(KeyboardModifierSide::MetaLeft, ui->comboMetaLeft->currentData().toInt());
  m_computer->setSideModifier(KeyboardModifierSide::MetaRight, ui->comboMetaRight->currentData().toInt());
  m_computer->setSideModifier(KeyboardModifierSide::SuperLeft, ui->comboSuperLeft->currentData().toInt());
  m_computer->setSideModifier(KeyboardModifierSide::SuperRight, ui->comboSuperRight->currentData().toInt());

  m_computer->setSwitchCorner(TopLeft, ui->chkDeadTopLeft->isChecked());
  m_computer->setSwitchCorner(TopRight, ui->chkDeadTopRight->isChecked());
  m_computer->setSwitchCorner(BottomLeft, ui->chkDeadBottomLeft->isChecked());
  m_computer->setSwitchCorner(BottomRight, ui->chkDeadBottomRight->isChecked());
  m_computer->setSwitchCornerSize(ui->sbSwitchCornerSize->value());

  m_computer->setFix(CapsLock, ui->chkFixCapsLock->isChecked());
  m_computer->setFix(NumLock, ui->chkFixNumLock->isChecked());
  m_computer->setFix(ScrollLock, ui->chkFixScrollLock->isChecked());
  m_computer->setFix(XTest, ui->chkFixXTest->isChecked());

  Settings::setValue(Settings::Computer::WeakX11Focus.arg(m_computer->name()), ui->chkWeakX11Focus->isChecked());

  QDialog::accept();
}

void ComputerSettingsDialog::addAlias()
{
  if (!ui->lineAddAlias->text().isEmpty() &&
      ui->listAliases->findItems(ui->lineAddAlias->text(), Qt::MatchFixedString).isEmpty()) {
    new QListWidgetItem(ui->lineAddAlias->text(), ui->listAliases);
    ui->lineAddAlias->clear();
  }
}

void ComputerSettingsDialog::removeAlias() const
{
  QList<QListWidgetItem *> items = ui->listAliases->selectedItems();
  qDeleteAll(items);
}

void ComputerSettingsDialog::checkNewAliasName(const QString &text)
{
  ui->btnAddAlias->setEnabled(!text.isEmpty() && ui->lblAliasError->text().isEmpty());
}

void ComputerSettingsDialog::aliasSelected()
{
  ui->btnRemoveAlias->setEnabled(!ui->listAliases->selectedItems().isEmpty());
}
