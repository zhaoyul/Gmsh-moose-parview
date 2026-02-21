#pragma once

#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;
class QTreeWidgetItem;

namespace gmp {

class PropertyEditor : public QWidget {
  Q_OBJECT
 public:
  explicit PropertyEditor(QWidget* parent = nullptr);

  void set_item(QTreeWidgetItem* item);

  static constexpr int kKindRole = Qt::UserRole + 1;
  static constexpr int kParamsRole = Qt::UserRole + 2;

 private slots:
  void on_name_changed(const QString& value);
  void on_add_param();
  void on_remove_param();
  void on_param_changed(int row, int column);

 private:
  void load_from_item();
  void save_params_to_item();
  bool is_root_item() const;

  QTreeWidgetItem* current_item_ = nullptr;
  QLabel* kind_label_ = nullptr;
  QLineEdit* name_edit_ = nullptr;
  QTableWidget* params_table_ = nullptr;
  QPushButton* add_param_btn_ = nullptr;
  QPushButton* remove_param_btn_ = nullptr;
};

}  // namespace gmp
