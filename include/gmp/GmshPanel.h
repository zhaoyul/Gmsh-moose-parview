#pragma once

#include <QWidget>
#include <QString>
#include <QStringList>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLineEdit;
class QPlainTextEdit;

namespace gmp {

class GmshPanel : public QWidget {
  Q_OBJECT
 public:
  explicit GmshPanel(QWidget* parent = nullptr);
  ~GmshPanel() override;

 signals:
  void mesh_written(const QString& path);
  void boundary_groups(const QStringList& names);

 private slots:
  void on_pick_output();
  void on_generate();

 private:
  void append_log(const QString& text);

  QDoubleSpinBox* size_x_ = nullptr;
  QDoubleSpinBox* size_y_ = nullptr;
  QDoubleSpinBox* size_z_ = nullptr;
  QDoubleSpinBox* mesh_size_ = nullptr;
  QComboBox* elem_order_ = nullptr;
  QComboBox* msh_version_ = nullptr;
  QCheckBox* optimize_ = nullptr;

  QLineEdit* output_path_ = nullptr;
  QPlainTextEdit* log_ = nullptr;
  bool gmsh_ready_ = false;
};

}  // namespace gmp
