#pragma once

#include <QWidget>
#include <QString>
#include <QDateTime>

class QLabel;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QSlider;
class QPushButton;
class QTimer;
class QSpinBox;
class QFileSystemWatcher;

#ifdef GMP_ENABLE_VTK_VIEWER
class QVTKOpenGLNativeWidget;
class vtkGenericOpenGLRenderWindow;
class vtkRenderer;
class vtkExodusIIReader;
class vtkCompositeDataGeometryFilter;
class vtkDataSetSurfaceFilter;
class vtkUnstructuredGrid;
class vtkVertexGlyphFilter;
class vtkThreshold;
class vtkPlane;
class vtkCutter;
class vtkPolyDataMapper;
class vtkActor;
class vtkScalarBarActor;
class vtkLookupTable;

#include <vtkSmartPointer.h>
#include <vector>
#endif

namespace gmp {

class VtkViewer : public QWidget {
  Q_OBJECT
 public:
  explicit VtkViewer(QWidget* parent = nullptr);
  ~VtkViewer() override = default;

 public slots:
  void set_exodus_file(const QString& path);
  void set_exodus_history(const QStringList& paths);
  bool save_screenshot(const QString& path);
  void set_mesh_file(const QString& path);

 private slots:
  void on_time_changed(int index);
  void on_array_changed(int index);
  void on_reload();
  void on_open_file();
  void on_apply_range();
  void on_preset_changed(int index);
  void on_repr_changed(int index);
  void on_auto_refresh_toggled(bool enabled);
  void on_auto_refresh_tick();

 private:
  void init_vtk();
  void update_pipeline();
  void refresh_time_only();
  void refresh_from_disk();
  void update_time_steps_from_reader(bool keep_index);
  void populate_arrays();
  void apply_representation();
  void apply_lookup_table();
  void set_refresh_enabled(bool enabled);
  void setup_watcher(const QString& file_path);
  void schedule_reload();
  void load_file(const QString& path);
  void update_nodes_visibility();
  void update_mesh_pipeline();
  void update_mesh_controls();

  QString current_file_;
  QLabel* file_label_ = nullptr;
  QPushButton* open_btn_ = nullptr;
  QComboBox* array_combo_ = nullptr;
  QComboBox* preset_combo_ = nullptr;
  QComboBox* repr_combo_ = nullptr;
  QCheckBox* auto_range_ = nullptr;
  QCheckBox* auto_refresh_ = nullptr;
  QCheckBox* show_nodes_ = nullptr;
  QCheckBox* show_quality_ = nullptr;
  QLabel* mesh_legend_ = nullptr;
  QComboBox* mesh_group_ = nullptr;
  QComboBox* mesh_dim_ = nullptr;
  QCheckBox* slice_enable_ = nullptr;
  QComboBox* slice_axis_ = nullptr;
  QSlider* slice_slider_ = nullptr;
  QSpinBox* refresh_ms_ = nullptr;
  QTimer* refresh_timer_ = nullptr;
  QTimer* debounce_timer_ = nullptr;
  QFileSystemWatcher* watcher_ = nullptr;
  qint64 last_file_size_ = -1;
  QDateTime last_file_mtime_;
  bool pending_reload_ = false;
  QDoubleSpinBox* range_min_ = nullptr;
  QDoubleSpinBox* range_max_ = nullptr;
  QSlider* time_slider_ = nullptr;
  QLabel* time_label_ = nullptr;
  QPushButton* reload_btn_ = nullptr;
  QLabel* output_label_ = nullptr;
  QComboBox* output_combo_ = nullptr;
  QPushButton* output_pick_ = nullptr;

#ifdef GMP_ENABLE_VTK_VIEWER
  QVTKOpenGLNativeWidget* vtk_widget_ = nullptr;
  std::vector<double> time_steps_;

  enum class DataMode { None, Exodus, Mesh };
  DataMode mode_ = DataMode::None;

  struct MeshGroup {
    int dim = 0;
    int id = 0;
    QString name;
  };
  std::vector<MeshGroup> mesh_groups_;

  vtkSmartPointer<vtkGenericOpenGLRenderWindow> render_window_;
  vtkSmartPointer<vtkRenderer> renderer_;
  vtkSmartPointer<vtkExodusIIReader> reader_;
  vtkSmartPointer<vtkCompositeDataGeometryFilter> geom_;
  vtkSmartPointer<vtkUnstructuredGrid> mesh_grid_;
  vtkSmartPointer<vtkDataSetSurfaceFilter> mesh_geom_;
  vtkSmartPointer<vtkThreshold> mesh_dim_threshold_;
  vtkSmartPointer<vtkThreshold> mesh_group_threshold_;
  vtkSmartPointer<vtkPlane> mesh_slice_plane_;
  vtkSmartPointer<vtkCutter> mesh_slice_cutter_;
  vtkSmartPointer<vtkPolyDataMapper> mapper_;
  vtkSmartPointer<vtkActor> actor_;
  vtkSmartPointer<vtkVertexGlyphFilter> nodes_filter_;
  vtkSmartPointer<vtkPolyDataMapper> nodes_mapper_;
  vtkSmartPointer<vtkActor> nodes_actor_;
  vtkSmartPointer<vtkScalarBarActor> scalar_bar_;
  vtkSmartPointer<vtkLookupTable> lut_;
  bool first_render_ = true;
  bool pipeline_ready_ = false;
  bool actor_added_ = false;
  bool mesh_quality_ready_ = false;
  double mesh_bounds_[6] = {0, 0, 0, 0, 0, 0};
#endif
};

}  // namespace gmp
