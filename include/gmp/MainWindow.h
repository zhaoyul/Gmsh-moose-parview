#pragma once

#include <QMainWindow>
#include <QVariantMap>

class QPlainTextEdit;
class QAction;
class QStackedWidget;
class QTabBar;
class QTreeWidget;
class QTreeWidgetItem;
class QString;

namespace gmp {

class MoosePanel;
class VtkViewer;
class PropertyEditor;
class GmshPanel;

class MainWindow : public QMainWindow {
  Q_OBJECT
 public:
 explicit MainWindow(QWidget* parent = nullptr);

 private:
  void build_menu();
  void build_toolbar();
  void build_model_tree();
  void apply_theme();
  void clear_model_tree_children();
  QTreeWidgetItem* find_root_item(const QString& name) const;
  QTreeWidgetItem* find_child_by_param(QTreeWidgetItem* root,
                                       const QString& key,
                                       const QString& value) const;
  QTreeWidgetItem* add_child_item(QTreeWidgetItem* root,
                                  const QString& name,
                                  const QString& kind,
                                  const QVariantMap& params);
  void upsert_mesh_item(const QString& path);
  void upsert_result_item(const QString& path, const QString& job_name);
  void add_item_under_root(QTreeWidgetItem* root);
  void remove_item(QTreeWidgetItem* item);
  void load_project(const QString& path);
  bool save_project(const QString& path);
  QString build_block_from_root(QTreeWidgetItem* root,
                                const QString& block_name,
                                const QString& default_type,
                                const QStringList& skip_keys) const;
  QString build_variables_block(QTreeWidgetItem* root) const;
  QString build_executioner_block(QTreeWidgetItem* root) const;
  void sync_model_to_input();
  void load_demo_diffusion(bool run);
  void load_demo_thermo(bool run);
  void load_demo_nonlinear_heat(bool run);

  QTabBar* module_tabs_ = nullptr;
  QTreeWidget* model_tree_ = nullptr;
  QStackedWidget* property_stack_ = nullptr;
  PropertyEditor* property_editor_ = nullptr;
  QPlainTextEdit* console_ = nullptr;
  VtkViewer* viewer_ = nullptr;
  QString project_path_;
  QTreeWidgetItem* active_job_item_ = nullptr;
  MoosePanel* moose_panel_ = nullptr;
  GmshPanel* gmsh_panel_ = nullptr;

  QAction* action_new_ = nullptr;
  QAction* action_open_ = nullptr;
  QAction* action_save_ = nullptr;
  QAction* action_save_as_ = nullptr;
  QAction* action_sync_ = nullptr;
  QAction* action_screenshot_ = nullptr;
  QAction* action_mesh_ = nullptr;
  QAction* action_preview_mesh_ = nullptr;
  QAction* action_run_ = nullptr;
  QAction* action_check_ = nullptr;
  QAction* action_stop_ = nullptr;
};

}  // namespace gmp
