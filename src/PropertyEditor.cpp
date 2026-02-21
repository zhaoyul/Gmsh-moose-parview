#include "gmp/PropertyEditor.h"

#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

namespace gmp {

PropertyEditor::PropertyEditor(QWidget* parent) : QWidget(parent) {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(8, 8, 8, 8);
  layout->setSpacing(6);

  auto* form = new QFormLayout();
  kind_label_ = new QLabel("-", this);
  name_edit_ = new QLineEdit(this);
  form->addRow("Kind", kind_label_);
  form->addRow("Name", name_edit_);
  layout->addLayout(form);

  params_table_ = new QTableWidget(this);
  params_table_->setColumnCount(2);
  params_table_->setHorizontalHeaderLabels({"Key", "Value"});
  params_table_->horizontalHeader()->setStretchLastSection(true);
  params_table_->verticalHeader()->setVisible(false);
  params_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
  params_table_->setSelectionMode(QAbstractItemView::SingleSelection);
  layout->addWidget(params_table_, 1);

  auto* buttons = new QHBoxLayout();
  add_param_btn_ = new QPushButton("Add Param", this);
  remove_param_btn_ = new QPushButton("Remove Param", this);
  buttons->addWidget(add_param_btn_);
  buttons->addWidget(remove_param_btn_);
  buttons->addStretch(1);
  layout->addLayout(buttons);

  connect(name_edit_, &QLineEdit::textChanged, this,
          &PropertyEditor::on_name_changed);
  connect(add_param_btn_, &QPushButton::clicked, this,
          &PropertyEditor::on_add_param);
  connect(remove_param_btn_, &QPushButton::clicked, this,
          &PropertyEditor::on_remove_param);
  connect(params_table_, &QTableWidget::cellChanged, this,
          &PropertyEditor::on_param_changed);

  set_item(nullptr);
}

void PropertyEditor::set_item(QTreeWidgetItem* item) {
  current_item_ = item;
  load_from_item();
}

bool PropertyEditor::is_root_item() const {
  return current_item_ && current_item_->parent() == nullptr;
}

void PropertyEditor::load_from_item() {
  params_table_->blockSignals(true);
  name_edit_->blockSignals(true);

  if (!current_item_) {
    kind_label_->setText("-");
    name_edit_->setText("");
    params_table_->setRowCount(0);
    name_edit_->setEnabled(false);
    params_table_->setEnabled(false);
    add_param_btn_->setEnabled(false);
    remove_param_btn_->setEnabled(false);
    name_edit_->blockSignals(false);
    params_table_->blockSignals(false);
    return;
  }

  const QString kind =
      current_item_->data(0, kKindRole).toString().isEmpty()
          ? current_item_->text(0)
          : current_item_->data(0, kKindRole).toString();
  kind_label_->setText(kind);
  name_edit_->setText(current_item_->text(0));

  params_table_->setRowCount(0);
  const QVariantMap params =
      current_item_->data(0, kParamsRole).toMap();
  int row = 0;
  for (auto it = params.begin(); it != params.end(); ++it) {
    params_table_->insertRow(row);
    params_table_->setItem(row, 0, new QTableWidgetItem(it.key()));
    params_table_->setItem(row, 1,
                           new QTableWidgetItem(it.value().toString()));
    ++row;
  }

  const bool editable = !is_root_item();
  name_edit_->setEnabled(editable);
  params_table_->setEnabled(editable);
  add_param_btn_->setEnabled(editable);
  remove_param_btn_->setEnabled(editable);

  name_edit_->blockSignals(false);
  params_table_->blockSignals(false);
}

void PropertyEditor::on_name_changed(const QString& value) {
  if (!current_item_ || is_root_item()) {
    return;
  }
  current_item_->setText(0, value);
}

void PropertyEditor::on_add_param() {
  if (!current_item_ || is_root_item()) {
    return;
  }
  const int row = params_table_->rowCount();
  params_table_->insertRow(row);
  params_table_->setItem(row, 0, new QTableWidgetItem("key"));
  params_table_->setItem(row, 1, new QTableWidgetItem("value"));
  save_params_to_item();
}

void PropertyEditor::on_remove_param() {
  if (!current_item_ || is_root_item()) {
    return;
  }
  const auto ranges = params_table_->selectedRanges();
  if (ranges.isEmpty()) {
    return;
  }
  params_table_->removeRow(ranges.first().topRow());
  save_params_to_item();
}

void PropertyEditor::on_param_changed(int row, int column) {
  Q_UNUSED(row);
  Q_UNUSED(column);
  if (!current_item_ || is_root_item()) {
    return;
  }
  save_params_to_item();
}

void PropertyEditor::save_params_to_item() {
  if (!current_item_) {
    return;
  }
  QVariantMap params;
  for (int row = 0; row < params_table_->rowCount(); ++row) {
    auto* key_item = params_table_->item(row, 0);
    auto* val_item = params_table_->item(row, 1);
    if (!key_item) {
      continue;
    }
    const QString key = key_item->text().trimmed();
    if (key.isEmpty()) {
      continue;
    }
    const QString value = val_item ? val_item->text() : QString();
    params.insert(key, value);
  }
  current_item_->setData(0, kParamsRole, params);
}

}  // namespace gmp
