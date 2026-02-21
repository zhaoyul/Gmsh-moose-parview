#include "gmp/VtkViewer.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QHBoxLayout>
#include <QLabel>
#include <QListView>
#include <QPushButton>
#include <QSpinBox>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>
#include <QStringList>
#include <QtCore/Qt>
#include <unordered_map>

#ifdef GMP_ENABLE_VTK_VIEWER
#include <QVTKOpenGLNativeWidget.h>
#include <vtkActor.h>
#include <vtkCellData.h>
#include <vtkCompositeDataGeometryFilter.h>
#include <vtkDataArray.h>
#include <vtkDataObject.h>
#include <vtkDataSetSurfaceFilter.h>
#include <vtkExodusIIReader.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkInformation.h>
#include <vtkLookupTable.h>
#include <vtkMeshQuality.h>
#include <vtkPlane.h>
#include <vtkCutter.h>
#include <vtkThreshold.h>
#include <vtkProperty.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkIntArray.h>
#include <vtkUnstructuredGrid.h>
#include <vtkPoints.h>
#include <vtkCellType.h>
#include <vtkVertexGlyphFilter.h>
#include <vtkRenderer.h>
#include <vtkScalarBarActor.h>
#include <vtkWindowToImageFilter.h>
#include <vtkPNGWriter.h>
#include <vtkStreamingDemandDrivenPipeline.h>
#endif

#ifdef GMP_ENABLE_GMSH_GUI
#include <gmsh.h>
#endif

namespace gmp {

#ifdef GMP_ENABLE_VTK_VIEWER
namespace {

int VtkCellFromDimAndNodes(int dim, int num_primary) {
  if (dim == 0) {
    return VTK_VERTEX;
  }
  if (dim == 1) {
    return VTK_LINE;
  }
  if (dim == 2) {
    if (num_primary >= 4) {
      return VTK_QUAD;
    }
    return VTK_TRIANGLE;
  }
  if (dim == 3) {
    if (num_primary == 4) {
      return VTK_TETRA;
    }
    if (num_primary == 5) {
      return VTK_PYRAMID;
    }
    if (num_primary == 6) {
      return VTK_WEDGE;
    }
    if (num_primary == 8) {
      return VTK_HEXAHEDRON;
    }
  }
  return VTK_EMPTY_CELL;
}

#ifdef GMP_ENABLE_GMSH_GUI
vtkSmartPointer<vtkUnstructuredGrid> BuildGridFromGmsh(
    const QString& path) {
  try {
    if (!gmsh::isInitialized()) {
      gmsh::initialize(0, nullptr, false, false);
    }
    gmsh::option::setNumber("General.Terminal", 0);
    gmsh::clear();
    gmsh::open(path.toStdString());

    std::vector<std::size_t> node_tags;
    std::vector<double> coords;
    std::vector<double> params;
    gmsh::model::mesh::getNodes(node_tags, coords, params);
    if (node_tags.empty() || coords.size() < node_tags.size() * 3) {
      return nullptr;
    }

    auto points = vtkSmartPointer<vtkPoints>::New();
    points->SetNumberOfPoints(static_cast<vtkIdType>(node_tags.size()));
    std::unordered_map<std::size_t, vtkIdType> id_map;
    id_map.reserve(node_tags.size());

    auto node_tags_arr = vtkSmartPointer<vtkIntArray>::New();
    node_tags_arr->SetName("node_tag");
    node_tags_arr->SetNumberOfValues(
        static_cast<vtkIdType>(node_tags.size()));
    for (size_t i = 0; i < node_tags.size(); ++i) {
      id_map[node_tags[i]] = static_cast<vtkIdType>(i);
      points->SetPoint(static_cast<vtkIdType>(i), coords[3 * i],
                       coords[3 * i + 1], coords[3 * i + 2]);
      node_tags_arr->SetValue(static_cast<vtkIdType>(i),
                              static_cast<int>(node_tags[i]));
    }

    auto grid = vtkSmartPointer<vtkUnstructuredGrid>::New();
    grid->SetPoints(points);
    grid->GetPointData()->AddArray(node_tags_arr);

    std::unordered_map<std::size_t, int> elem_phys;
    std::unordered_map<std::size_t, int> elem_phys_dim;
    std::vector<std::pair<int, int>> phys_groups;
    gmsh::model::getPhysicalGroups(phys_groups);
    for (const auto& pg : phys_groups) {
      std::vector<int> entities;
      gmsh::model::getEntitiesForPhysicalGroup(pg.first, pg.second, entities);
      for (const auto ent : entities) {
        std::vector<int> etypes;
        std::vector<std::vector<std::size_t>> etags;
        std::vector<std::vector<std::size_t>> enodes;
        gmsh::model::mesh::getElements(etypes, etags, enodes, pg.first, ent);
        for (const auto& list : etags) {
          for (const auto tag : list) {
            elem_phys[tag] = pg.second;
            elem_phys_dim[tag] = pg.first;
          }
        }
      }
    }

    auto phys_id_arr = vtkSmartPointer<vtkIntArray>::New();
    phys_id_arr->SetName("phys_id");
    auto phys_dim_arr = vtkSmartPointer<vtkIntArray>::New();
    phys_dim_arr->SetName("phys_dim");
    auto elem_type_arr = vtkSmartPointer<vtkIntArray>::New();
    elem_type_arr->SetName("elem_type");
    auto elem_tag_arr = vtkSmartPointer<vtkIntArray>::New();
    elem_tag_arr->SetName("elem_tag");

    std::vector<int> element_types;
    std::vector<std::vector<std::size_t>> element_tags;
    std::vector<std::vector<std::size_t>> element_node_tags;
    gmsh::model::mesh::getElements(element_types, element_tags,
                                   element_node_tags);

    for (size_t k = 0; k < element_types.size(); ++k) {
      int dim = 0;
      int order = 0;
      int num_nodes = 0;
      int num_primary = 0;
      std::string name;
      std::vector<double> local;
      gmsh::model::mesh::getElementProperties(element_types[k], name, dim,
                                              order, num_nodes, local,
                                              num_primary);
      if (num_nodes <= 0 || num_primary <= 0) {
        continue;
      }
      const int cell_type = VtkCellFromDimAndNodes(dim, num_primary);
      if (cell_type == VTK_EMPTY_CELL) {
        continue;
      }

      const auto& nodes = element_node_tags[k];
      const size_t elem_count = nodes.size() / static_cast<size_t>(num_nodes);
      std::vector<vtkIdType> ids(static_cast<size_t>(num_primary));
      for (size_t e = 0; e < elem_count; ++e) {
        const size_t base = e * static_cast<size_t>(num_nodes);
        for (int j = 0; j < num_primary; ++j) {
          const std::size_t tag = nodes[base + static_cast<size_t>(j)];
          auto it = id_map.find(tag);
          ids[static_cast<size_t>(j)] =
              it == id_map.end() ? 0 : it->second;
        }
        grid->InsertNextCell(cell_type, num_primary, ids.data());
        const std::size_t elem_tag = element_tags[k][e];
        const auto phys_it = elem_phys.find(elem_tag);
        const int phys_id =
            phys_it == elem_phys.end() ? 0 : phys_it->second;
        const int phys_dim =
            phys_it == elem_phys.end() ? dim : elem_phys_dim[elem_tag];
        phys_id_arr->InsertNextValue(phys_id);
        phys_dim_arr->InsertNextValue(phys_dim);
        elem_type_arr->InsertNextValue(element_types[k]);
        elem_tag_arr->InsertNextValue(static_cast<int>(elem_tag));
      }
    }

    grid->GetCellData()->AddArray(phys_id_arr);
    grid->GetCellData()->AddArray(phys_dim_arr);
    grid->GetCellData()->AddArray(elem_type_arr);
    grid->GetCellData()->AddArray(elem_tag_arr);
    grid->GetCellData()->SetScalars(phys_id_arr);

    return grid;
  } catch (...) {
    return nullptr;
  }
}
#endif

class ComboPopupFixer : public QObject {
 public:
  explicit ComboPopupFixer(QComboBox* combo) : QObject(combo), combo_(combo) {}

 protected:
  bool eventFilter(QObject* obj, QEvent* event) override {
    if (!combo_) {
      return QObject::eventFilter(obj, event);
    }
    if (event->type() == QEvent::Show || event->type() == QEvent::ShowToParent) {
      auto* w = qobject_cast<QWidget*>(obj);
      if (w) {
        const QPoint pos = combo_->mapToGlobal(QPoint(0, combo_->height()));
        w->move(pos);
      }
    }
    return QObject::eventFilter(obj, event);
  }

 private:
  QComboBox* combo_ = nullptr;
};

void AttachComboPopupFix(QComboBox* combo) {
  if (!combo) {
    return;
  }
  auto* view = new QListView(combo);
  view->setUniformItemSizes(true);
  combo->setView(view);
  auto* popup = combo->view()->window();
  if (popup) {
    popup->installEventFilter(new ComboPopupFixer(combo));
  } else {
    combo->view()->installEventFilter(new ComboPopupFixer(combo));
  }
}

}  // namespace
#endif

VtkViewer::VtkViewer(QWidget* parent) : QWidget(parent) {
  auto* layout = new QVBoxLayout(this);

  auto* header = new QHBoxLayout();
  file_label_ = new QLabel("No file loaded");
  open_btn_ = new QPushButton("Open");
  reload_btn_ = new QPushButton("Reload");
  connect(open_btn_, &QPushButton::clicked, this, &VtkViewer::on_open_file);
  connect(reload_btn_, &QPushButton::clicked, this, &VtkViewer::on_reload);
  header->addWidget(file_label_, 1);
  header->addWidget(open_btn_);
  header->addWidget(reload_btn_);
  layout->addLayout(header);

  auto* output_row = new QHBoxLayout();
  output_label_ = new QLabel("Outputs");
  output_combo_ = new QComboBox();
  output_combo_->setMinimumWidth(240);
  AttachComboPopupFix(output_combo_);
  output_pick_ = new QPushButton("Load Selected");
  connect(output_pick_, &QPushButton::clicked, this, [this]() {
    const QString path = output_combo_->currentData().toString();
    if (!path.isEmpty()) {
      set_exodus_file(path);
    }
  });
  output_row->addWidget(output_label_);
  output_row->addWidget(output_combo_, 1);
  output_row->addWidget(output_pick_);
  layout->addLayout(output_row);

  auto* scalar_row = new QHBoxLayout();
  array_combo_ = new QComboBox();
  array_combo_->setMinimumWidth(220);
  AttachComboPopupFix(array_combo_);
  connect(array_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &VtkViewer::on_array_changed);
  preset_combo_ = new QComboBox();
  preset_combo_->addItem("Blue-Red");
  preset_combo_->addItem("Grayscale");
  preset_combo_->addItem("Rainbow");
  AttachComboPopupFix(preset_combo_);
  connect(preset_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &VtkViewer::on_preset_changed);
  repr_combo_ = new QComboBox();
  repr_combo_->addItem("Surface");
  repr_combo_->addItem("Wireframe");
  repr_combo_->addItem("Surface + Edges");
  AttachComboPopupFix(repr_combo_);
  connect(repr_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &VtkViewer::on_repr_changed);

  auto_range_ = new QCheckBox("Auto Range");
  auto_range_->setChecked(true);
  connect(auto_range_, &QCheckBox::toggled, this, &VtkViewer::on_apply_range);
  range_min_ = new QDoubleSpinBox();
  range_max_ = new QDoubleSpinBox();
  range_min_->setDecimals(6);
  range_max_->setDecimals(6);
  range_min_->setRange(-1e12, 1e12);
  range_max_->setRange(-1e12, 1e12);
  range_min_->setEnabled(false);
  range_max_->setEnabled(false);
  connect(range_min_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, &VtkViewer::on_apply_range);
  connect(range_max_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, &VtkViewer::on_apply_range);

  scalar_row->addWidget(new QLabel("Scalar"));
  scalar_row->addWidget(array_combo_, 2);
  scalar_row->addWidget(new QLabel("Preset"));
  scalar_row->addWidget(preset_combo_);
  scalar_row->addWidget(new QLabel("Repr"));
  scalar_row->addWidget(repr_combo_);
  scalar_row->addWidget(auto_range_);
  scalar_row->addWidget(range_min_);
  scalar_row->addWidget(range_max_);
  layout->addLayout(scalar_row);

  auto* mesh_row = new QHBoxLayout();
  mesh_row->addWidget(new QLabel("Mesh"));
  show_nodes_ = new QCheckBox("Nodes");
  connect(show_nodes_, &QCheckBox::toggled, this,
          [this](bool) { update_nodes_visibility(); });
  show_quality_ = new QCheckBox("Quality");
  connect(show_quality_, &QCheckBox::toggled, this, [this](bool checked) {
    update_pipeline();
    const QString target = checked ? "C:Quality" : "C:phys_id";
    const int idx = array_combo_->findData(target);
    if (idx >= 0) {
      array_combo_->setCurrentIndex(idx);
    }
  });
  mesh_dim_ = new QComboBox();
  mesh_dim_->addItem("All", -1);
  mesh_dim_->addItem("0", 0);
  mesh_dim_->addItem("1", 1);
  mesh_dim_->addItem("2", 2);
  mesh_dim_->addItem("3", 3);
  AttachComboPopupFix(mesh_dim_);
  connect(mesh_dim_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this](int) { update_mesh_pipeline(); });
  mesh_group_ = new QComboBox();
  mesh_group_->setMinimumWidth(180);
  AttachComboPopupFix(mesh_group_);
  connect(mesh_group_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this](int) { update_mesh_pipeline(); });
  mesh_row->addWidget(show_nodes_);
  mesh_row->addWidget(show_quality_);
  mesh_row->addWidget(new QLabel("Dim"));
  mesh_row->addWidget(mesh_dim_);
  mesh_row->addWidget(new QLabel("Group"));
  mesh_row->addWidget(mesh_group_, 1);
  mesh_row->addStretch(1);
  layout->addLayout(mesh_row);

  auto* slice_row = new QHBoxLayout();
  slice_enable_ = new QCheckBox("Slice");
  slice_axis_ = new QComboBox();
  slice_axis_->addItems({"X", "Y", "Z"});
  AttachComboPopupFix(slice_axis_);
  slice_slider_ = new QSlider(Qt::Horizontal);
  slice_slider_->setRange(0, 100);
  slice_slider_->setValue(50);
  connect(slice_enable_, &QCheckBox::toggled, this,
          [this](bool) { update_mesh_pipeline(); });
  connect(slice_axis_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this](int) { update_mesh_pipeline(); });
  connect(slice_slider_, &QSlider::valueChanged, this,
          [this](int) { update_mesh_pipeline(); });
  slice_row->addWidget(slice_enable_);
  slice_row->addWidget(slice_axis_);
  slice_row->addWidget(slice_slider_, 1);
  layout->addLayout(slice_row);

  mesh_legend_ = new QLabel();
  mesh_legend_->setWordWrap(true);
  mesh_legend_->setText("Groups: none");
  layout->addWidget(mesh_legend_);

  auto* refresh_row = new QHBoxLayout();
  auto_refresh_ = new QCheckBox("Auto Refresh");
  refresh_ms_ = new QSpinBox();
  refresh_ms_->setRange(250, 10000);
  refresh_ms_->setSingleStep(250);
  refresh_ms_->setValue(1000);
  refresh_timer_ = new QTimer(this);
  connect(auto_refresh_, &QCheckBox::toggled, this,
          &VtkViewer::on_auto_refresh_toggled);
  connect(refresh_timer_, &QTimer::timeout, this,
          &VtkViewer::on_auto_refresh_tick);
  connect(refresh_ms_, QOverload<int>::of(&QSpinBox::valueChanged), this,
          [this](int) {
            if (refresh_timer_->isActive()) {
              refresh_timer_->start(refresh_ms_->value());
            }
          });
  debounce_timer_ = new QTimer(this);
  debounce_timer_->setSingleShot(true);
  connect(debounce_timer_, &QTimer::timeout, this,
          &VtkViewer::on_auto_refresh_tick);
  refresh_row->addWidget(auto_refresh_);
  refresh_row->addWidget(new QLabel("ms"));
  refresh_row->addWidget(refresh_ms_);
  refresh_row->addStretch(1);
  layout->addLayout(refresh_row);

  auto* time_row = new QHBoxLayout();
  time_slider_ = new QSlider(Qt::Horizontal);
  time_slider_->setRange(0, 0);
  time_label_ = new QLabel("t=0");
  connect(time_slider_, &QSlider::valueChanged, this, &VtkViewer::on_time_changed);
  time_row->addWidget(new QLabel("Time"));
  time_row->addWidget(time_slider_, 1);
  time_row->addWidget(time_label_);
  layout->addLayout(time_row);

#ifdef GMP_ENABLE_VTK_VIEWER
  vtk_widget_ = new QVTKOpenGLNativeWidget(this);
  vtk_widget_->setMinimumSize(640, 480);
  layout->addWidget(vtk_widget_, 1);
  QTimer::singleShot(0, this, [this]() { init_vtk(); });
#else
  auto* label =
      new QLabel("VTK Viewer Disabled\n(Rebuild with GMP_ENABLE_VTK_VIEWER=ON)");
  label->setAlignment(Qt::AlignCenter);
  layout->addWidget(label, 1);
  reload_btn_->setEnabled(false);
  open_btn_->setEnabled(false);
  array_combo_->setEnabled(false);
  preset_combo_->setEnabled(false);
  repr_combo_->setEnabled(false);
  auto_range_->setEnabled(false);
  range_min_->setEnabled(false);
  range_max_->setEnabled(false);
  output_combo_->setEnabled(false);
  output_pick_->setEnabled(false);
  auto_refresh_->setEnabled(false);
  refresh_ms_->setEnabled(false);
  time_slider_->setEnabled(false);
  show_nodes_->setEnabled(false);
  show_quality_->setEnabled(false);
  mesh_dim_->setEnabled(false);
  mesh_group_->setEnabled(false);
  slice_enable_->setEnabled(false);
  slice_axis_->setEnabled(false);
  slice_slider_->setEnabled(false);
  mesh_legend_->setEnabled(false);
#endif
}

void VtkViewer::set_exodus_file(const QString& path) {
  current_file_ = path;
  if (!file_label_) {
    return;
  }
  file_label_->setText(path.isEmpty() ? "No file loaded" : path);
#ifdef GMP_ENABLE_VTK_VIEWER
  if (path.isEmpty()) {
    return;
  }

  if (!reader_) {
    reader_ = vtkSmartPointer<vtkExodusIIReader>::New();
  }
  if (!geom_) {
    geom_ = vtkSmartPointer<vtkCompositeDataGeometryFilter>::New();
  }
  if (!mapper_) {
    mapper_ = vtkSmartPointer<vtkPolyDataMapper>::New();
  }
  if (!actor_) {
    actor_ = vtkSmartPointer<vtkActor>::New();
  }
  if (!lut_) {
    lut_ = vtkSmartPointer<vtkLookupTable>::New();
    lut_->SetNumberOfTableValues(256);
    lut_->Build();
  }
  if (!scalar_bar_) {
    scalar_bar_ = vtkSmartPointer<vtkScalarBarActor>::New();
  }
  mapper_->SetLookupTable(lut_);
  geom_->SetInputConnection(reader_->GetOutputPort());
  mapper_->SetInputConnection(geom_->GetOutputPort());
  actor_->SetMapper(mapper_);
  if (renderer_ && !actor_added_) {
    renderer_->AddActor(actor_);
    scalar_bar_->SetLookupTable(mapper_->GetLookupTable());
    renderer_->AddViewProp(scalar_bar_);
    actor_added_ = true;
  }
  pipeline_ready_ = true;

  first_render_ = true;
  mode_ = DataMode::Exodus;
  reader_->SetFileName(path.toUtf8().constData());
  reader_->UpdateInformation();
  reader_->SetAllArrayStatus(vtkExodusIIReader::NODAL, 1);
  reader_->SetAllArrayStatus(vtkExodusIIReader::ELEM_BLOCK, 1);
  reader_->SetAllArrayStatus(vtkExodusIIReader::GLOBAL, 1);
  const int n_points = reader_->GetNumberOfPointResultArrays();
  for (int i = 0; i < n_points; ++i) {
    reader_->SetPointResultArrayStatus(
        reader_->GetPointResultArrayName(i), 1);
  }
  const int n_elems = reader_->GetNumberOfElementResultArrays();
  for (int i = 0; i < n_elems; ++i) {
    reader_->SetElementResultArrayStatus(
        reader_->GetElementResultArrayName(i), 1);
  }
  update_time_steps_from_reader(false);
  update_mesh_controls();
  setup_watcher(path);
  update_pipeline();
#else
  Q_UNUSED(path);
#endif
}

void VtkViewer::set_mesh_file(const QString& path) {
  current_file_ = path;
  if (!file_label_) {
    return;
  }
  file_label_->setText(path.isEmpty() ? "No file loaded" : path);
#ifdef GMP_ENABLE_VTK_VIEWER
  if (path.isEmpty()) {
    return;
  }
  if (!renderer_) {
    return;
  }
  if (!mapper_) {
    mapper_ = vtkSmartPointer<vtkPolyDataMapper>::New();
    actor_ = vtkSmartPointer<vtkActor>::New();
    lut_ = vtkSmartPointer<vtkLookupTable>::New();
    lut_->SetNumberOfTableValues(256);
    lut_->Build();
    mapper_->SetLookupTable(lut_);
    scalar_bar_ = vtkSmartPointer<vtkScalarBarActor>::New();
  }
  if (!mesh_geom_) {
    mesh_geom_ = vtkSmartPointer<vtkDataSetSurfaceFilter>::New();
  }

#ifdef GMP_ENABLE_GMSH_GUI
  mesh_quality_ready_ = false;
  mesh_groups_.clear();
  mesh_grid_ = BuildGridFromGmsh(path);
  if (!mesh_grid_) {
    file_label_->setText("Failed to load mesh");
    return;
  }
  mesh_grid_->GetBounds(mesh_bounds_);
  try {
    std::vector<std::pair<int, int>> groups;
    gmsh::model::getPhysicalGroups(groups);
    for (const auto& pg : groups) {
      std::string name;
      gmsh::model::getPhysicalName(pg.first, pg.second, name);
      mesh_groups_.push_back(
          {pg.first, pg.second, QString::fromStdString(name)});
    }
  } catch (...) {
    // Ignore physical group name errors.
  }
  mesh_geom_->SetInputData(mesh_grid_);
#else
  file_label_->setText("Mesh preview requires libgmsh");
  return;
#endif

  mapper_->SetInputConnection(mesh_geom_->GetOutputPort());
  actor_->SetMapper(mapper_);
  if (renderer_ && !actor_added_) {
    renderer_->AddActor(actor_);
    scalar_bar_->SetLookupTable(mapper_->GetLookupTable());
    renderer_->AddViewProp(scalar_bar_);
    actor_added_ = true;
  }
  pipeline_ready_ = true;
  first_render_ = true;
  mode_ = DataMode::Mesh;
  time_steps_.clear();
  time_slider_->setEnabled(false);
  time_slider_->setRange(0, 0);
  time_label_->setText("t=0");
  if (repr_combo_ && repr_combo_->currentIndex() == 0) {
    repr_combo_->setCurrentIndex(2);
  }
  update_mesh_controls();

  setup_watcher(path);
  update_pipeline();
  update_nodes_visibility();
#else
  Q_UNUSED(path);
#endif
}

void VtkViewer::set_exodus_history(const QStringList& paths) {
  output_combo_->clear();
  for (const auto& p : paths) {
    output_combo_->addItem(QFileInfo(p).fileName(), p);
  }
  if (!paths.isEmpty()) {
    output_combo_->setCurrentIndex(0);
  }
}

bool VtkViewer::save_screenshot(const QString& path) {
  if (path.isEmpty()) {
    return false;
  }
#ifdef GMP_ENABLE_VTK_VIEWER
  if (render_window_) {
    auto w2i = vtkSmartPointer<vtkWindowToImageFilter>::New();
    w2i->SetInput(render_window_);
    w2i->ReadFrontBufferOff();
    w2i->Update();
    auto writer = vtkSmartPointer<vtkPNGWriter>::New();
    writer->SetFileName(path.toUtf8().constData());
    writer->SetInputConnection(w2i->GetOutputPort());
    writer->Write();
    return QFileInfo::exists(path);
  }
#endif
  const QPixmap pix = grab();
  return pix.save(path);
}

void VtkViewer::on_reload() {
  if (!current_file_.isEmpty()) {
    load_file(current_file_);
  }
}

void VtkViewer::on_time_changed(int index) {
#ifdef GMP_ENABLE_VTK_VIEWER
  if (mode_ != DataMode::Exodus) {
    return;
  }
  if (current_file_.isEmpty()) {
    return;
  }
  if (time_steps_.empty()) {
    return;
  }
  if (index < 0 || index >= static_cast<int>(time_steps_.size())) {
    return;
  }
  time_label_->setText(QString("t=%1").arg(time_steps_[index]));
  if (array_combo_->count() == 0) {
    update_pipeline();
  } else {
    refresh_time_only();
  }
#else
  Q_UNUSED(index);
#endif
}

void VtkViewer::on_array_changed(int index) {
#ifdef GMP_ENABLE_VTK_VIEWER
  if (!mapper_) {
    return;
  }
  if (index < 0) {
    return;
  }
  const QString key = array_combo_->currentData().toString();
  if (key.isEmpty()) {
    return;
  }
  const bool is_point = key.startsWith("P:");
  const QString name = key.mid(2);

  vtkPolyData* poly = nullptr;
  if (mode_ == DataMode::Exodus && geom_) {
    poly = geom_->GetOutput();
  } else if (mode_ == DataMode::Mesh && mapper_) {
    poly = mapper_->GetInput();
    if (!poly && mesh_geom_) {
      mesh_geom_->Update();
      poly = mesh_geom_->GetOutput();
    }
  }
  if (!poly) {
    return;
  }

  vtkDataArray* array = nullptr;
  if (is_point) {
    mapper_->SetScalarModeToUsePointFieldData();
    array = poly->GetPointData()->GetArray(name.toUtf8().constData());
  } else {
    mapper_->SetScalarModeToUseCellFieldData();
    array = poly->GetCellData()->GetArray(name.toUtf8().constData());
  }

  if (array) {
    double range[2] = {0.0, 1.0};
    array->GetRange(range);
    mapper_->SelectColorArray(name.toUtf8().constData());
    mapper_->ScalarVisibilityOn();
    if (auto_range_->isChecked()) {
      range_min_->setValue(range[0]);
      range_max_->setValue(range[1]);
    }
    apply_lookup_table();
    on_apply_range();
    if (scalar_bar_) {
      scalar_bar_->SetTitle(name.toUtf8().constData());
      scalar_bar_->SetLookupTable(mapper_->GetLookupTable());
    }
  } else {
    mapper_->ScalarVisibilityOff();
  }
  if (render_window_) {
    render_window_->Render();
  }
#else
  Q_UNUSED(index);
#endif
}

void VtkViewer::init_vtk() {
#ifdef GMP_ENABLE_VTK_VIEWER
  if (!vtk_widget_) {
    return;
  }
  render_window_ = vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New();
  renderer_ = vtkSmartPointer<vtkRenderer>::New();
  render_window_->AddRenderer(renderer_);
  vtk_widget_->setRenderWindow(render_window_);

  renderer_->SetBackground(0.12, 0.12, 0.12);
#endif
}

void VtkViewer::update_pipeline() {
#ifdef GMP_ENABLE_VTK_VIEWER
  if (!render_window_) {
    return;
  }
  if (current_file_.isEmpty()) {
    return;
  }
  if (!pipeline_ready_) {
    return;
  }
  if (mode_ == DataMode::Exodus && reader_ && geom_) {
    if (!time_steps_.empty()) {
      vtkInformation* info = reader_->GetOutputInformation(0);
      if (info) {
        const int idx = time_slider_->value();
        const double t = time_steps_[idx];
        info->Set(vtkStreamingDemandDrivenPipeline::UPDATE_TIME_STEP(), t);
      }
    }
    reader_->Update();
    geom_->Update();
  } else if (mode_ == DataMode::Mesh) {
    update_mesh_pipeline();
  }
  populate_arrays();
  if (first_render_) {
    renderer_->ResetCamera();
    first_render_ = false;
  }
  render_window_->Render();
#endif
}

void VtkViewer::populate_arrays() {
#ifdef GMP_ENABLE_VTK_VIEWER
  vtkPolyData* poly = nullptr;
  if (mode_ == DataMode::Exodus && geom_) {
    poly = geom_->GetOutput();
  } else if (mode_ == DataMode::Mesh && mapper_) {
    poly = mapper_->GetInput();
    if (!poly && mesh_geom_) {
      mesh_geom_->Update();
      poly = mesh_geom_->GetOutput();
    }
  }
  if (!poly) {
    return;
  }

  const QString current = array_combo_->currentData().toString();
  array_combo_->blockSignals(true);
  array_combo_->clear();

  auto* pd = poly->GetPointData();
  if (pd) {
    for (int i = 0; i < pd->GetNumberOfArrays(); ++i) {
      const char* name = pd->GetArrayName(i);
      if (name) {
        array_combo_->addItem(QString("Point: %1").arg(name),
                              QString("P:%1").arg(name));
      }
    }
  }

  auto* cd = poly->GetCellData();
  if (cd) {
    for (int i = 0; i < cd->GetNumberOfArrays(); ++i) {
      const char* name = cd->GetArrayName(i);
      if (name) {
        array_combo_->addItem(QString("Cell: %1").arg(name),
                              QString("C:%1").arg(name));
      }
    }
  }

  array_combo_->blockSignals(false);

  int idx = array_combo_->findData(current);
  if (idx < 0 && mode_ == DataMode::Mesh) {
    const QString preferred =
        (show_quality_ && show_quality_->isChecked()) ? "C:Quality" : "C:phys_id";
    idx = array_combo_->findData(preferred);
  }
  if (idx < 0 && array_combo_->count() > 0) {
    idx = 0;
    for (int i = 0; i < array_combo_->count(); ++i) {
      const QString label = array_combo_->itemText(i);
      if (!label.contains("ObjectId", Qt::CaseInsensitive)) {
        idx = i;
        break;
      }
    }
  }
  if (idx >= 0) {
    array_combo_->setCurrentIndex(idx);
    on_array_changed(idx);
  } else {
    mapper_->ScalarVisibilityOff();
  }
#endif
}

void VtkViewer::on_open_file() {
  const QString path =
      QFileDialog::getOpenFileName(this, "Open Result or Mesh", current_file_,
                                   "Exodus (*.e);;Gmsh Mesh (*.msh)");
  if (!path.isEmpty()) {
    load_file(path);
  }
}

void VtkViewer::on_apply_range() {
#ifdef GMP_ENABLE_VTK_VIEWER
  if (!mapper_) {
    return;
  }
  const bool auto_range = auto_range_->isChecked();
  range_min_->setEnabled(!auto_range);
  range_max_->setEnabled(!auto_range);
  if (auto_range) {
    // Range already updated in on_array_changed.
  } else {
    mapper_->SetScalarRange(range_min_->value(), range_max_->value());
  }
  if (render_window_) {
    render_window_->Render();
  }
#endif
}

void VtkViewer::on_preset_changed(int index) {
  Q_UNUSED(index);
  apply_lookup_table();
}

void VtkViewer::on_repr_changed(int index) {
  Q_UNUSED(index);
  apply_representation();
}

void VtkViewer::on_auto_refresh_toggled(bool enabled) {
  set_refresh_enabled(enabled);
}

void VtkViewer::on_auto_refresh_tick() {
  refresh_from_disk();
}

void VtkViewer::apply_representation() {
#ifdef GMP_ENABLE_VTK_VIEWER
  if (!actor_) {
    return;
  }
  const QString mode = repr_combo_->currentText();
  if (mode == "Wireframe") {
    actor_->GetProperty()->SetRepresentationToWireframe();
    actor_->GetProperty()->SetEdgeVisibility(0);
  } else if (mode == "Surface + Edges") {
    actor_->GetProperty()->SetRepresentationToSurface();
    actor_->GetProperty()->SetEdgeVisibility(1);
  } else {
    actor_->GetProperty()->SetRepresentationToSurface();
    actor_->GetProperty()->SetEdgeVisibility(0);
  }
  if (render_window_) {
    render_window_->Render();
  }
#endif
}

void VtkViewer::apply_lookup_table() {
#ifdef GMP_ENABLE_VTK_VIEWER
  if (!lut_ || !mapper_) {
    return;
  }
  const QString preset = preset_combo_->currentText();
  if (preset == "Grayscale") {
    lut_->SetHueRange(0.0, 0.0);
    lut_->SetSaturationRange(0.0, 0.0);
  } else if (preset == "Rainbow") {
    lut_->SetHueRange(0.666, 0.0);
    lut_->SetSaturationRange(1.0, 1.0);
  } else {  // Blue-Red
    lut_->SetHueRange(0.666, 0.0);
    lut_->SetSaturationRange(1.0, 1.0);
  }
  lut_->Build();
  mapper_->SetLookupTable(lut_);
  if (scalar_bar_) {
    scalar_bar_->SetLookupTable(lut_);
  }
  if (render_window_) {
    render_window_->Render();
  }
#endif
}

void VtkViewer::set_refresh_enabled(bool enabled) {
  if (enabled) {
    refresh_timer_->start(refresh_ms_->value());
  } else {
    refresh_timer_->stop();
  }
}

void VtkViewer::setup_watcher(const QString& file_path) {
  if (!watcher_) {
    watcher_ = new QFileSystemWatcher(this);
    connect(watcher_, &QFileSystemWatcher::fileChanged, this,
            [this](const QString&) { schedule_reload(); });
    connect(watcher_, &QFileSystemWatcher::directoryChanged, this,
            [this](const QString&) { schedule_reload(); });
  }
  watcher_->removePaths(watcher_->files());
  watcher_->removePaths(watcher_->directories());

  if (file_path.isEmpty()) {
    return;
  }
  QFileInfo fi(file_path);
  const QString dir = fi.absolutePath();
  if (!dir.isEmpty()) {
    watcher_->addPath(dir);
  }
  if (fi.exists()) {
    watcher_->addPath(fi.absoluteFilePath());
    last_file_size_ = fi.size();
    last_file_mtime_ = fi.lastModified();
  }
}

void VtkViewer::schedule_reload() {
  pending_reload_ = true;
  if (debounce_timer_) {
    debounce_timer_->start(300);
  }
}

void VtkViewer::refresh_from_disk() {
#ifdef GMP_ENABLE_VTK_VIEWER
  if (current_file_.isEmpty()) {
    return;
  }
  QFileInfo fi(current_file_);
  if (!fi.exists()) {
    return;
  }
  const bool changed =
      (last_file_size_ != fi.size()) || (last_file_mtime_ != fi.lastModified());
  if (changed) {
    last_file_size_ = fi.size();
    last_file_mtime_ = fi.lastModified();
    if (mode_ == DataMode::Exodus) {
      update_time_steps_from_reader(true);
    }
    if (array_combo_->count() == 0) {
      populate_arrays();
    }
    if (mode_ == DataMode::Mesh) {
      set_mesh_file(current_file_);
      return;
    }
  }
  if (mode_ == DataMode::Exodus) {
    refresh_time_only();
  } else {
    update_pipeline();
  }
#endif
}

void VtkViewer::refresh_time_only() {
#ifdef GMP_ENABLE_VTK_VIEWER
  if (mode_ != DataMode::Exodus) {
    return;
  }
  if (!reader_ || !geom_ || !render_window_) {
    return;
  }
  if (current_file_.isEmpty() || !pipeline_ready_) {
    return;
  }
  if (!time_steps_.empty()) {
    vtkInformation* info = reader_->GetOutputInformation(0);
    if (info) {
      const int idx = time_slider_->value();
      const double t = time_steps_[idx];
      info->Set(vtkStreamingDemandDrivenPipeline::UPDATE_TIME_STEP(), t);
    }
  }
  reader_->Update();
  geom_->Update();
  render_window_->Render();
#endif
}

void VtkViewer::update_time_steps_from_reader(bool keep_index) {
#ifdef GMP_ENABLE_VTK_VIEWER
  if (!reader_ || mode_ != DataMode::Exodus) {
    return;
  }
  const int prev_index = time_slider_->value();
  reader_->UpdateInformation();

  time_steps_.clear();
  vtkInformation* info = reader_->GetOutputInformation(0);
  if (info && info->Has(vtkStreamingDemandDrivenPipeline::TIME_STEPS())) {
    const int len = info->Length(vtkStreamingDemandDrivenPipeline::TIME_STEPS());
    const double* steps =
        info->Get(vtkStreamingDemandDrivenPipeline::TIME_STEPS());
    for (int i = 0; i < len; ++i) {
      time_steps_.push_back(steps[i]);
    }
  }

  if (time_steps_.empty()) {
    time_slider_->setRange(0, 0);
    time_slider_->setEnabled(false);
    time_label_->setText("t=0");
    return;
  }

  time_slider_->setEnabled(true);
  time_slider_->setRange(0, static_cast<int>(time_steps_.size()) - 1);
  const int idx = keep_index && prev_index < static_cast<int>(time_steps_.size())
                      ? prev_index
                      : 0;
  time_slider_->blockSignals(true);
  time_slider_->setValue(idx);
  time_slider_->blockSignals(false);
  time_label_->setText(QString("t=%1").arg(time_steps_[idx]));
#endif
}

void VtkViewer::load_file(const QString& path) {
  if (path.endsWith(".msh", Qt::CaseInsensitive)) {
    set_mesh_file(path);
  } else {
    set_exodus_file(path);
  }
}

void VtkViewer::update_mesh_controls() {
#ifdef GMP_ENABLE_VTK_VIEWER
  const bool mesh_mode = mode_ == DataMode::Mesh;
  if (show_nodes_) {
    show_nodes_->setEnabled(mesh_mode);
  }
  if (show_quality_) {
    show_quality_->setEnabled(mesh_mode);
  }
  if (mesh_dim_) {
    mesh_dim_->setEnabled(mesh_mode);
  }
  if (mesh_group_) {
    mesh_group_->setEnabled(mesh_mode);
  }
  if (slice_enable_) {
    slice_enable_->setEnabled(mesh_mode);
  }
  if (slice_axis_) {
    slice_axis_->setEnabled(mesh_mode && slice_enable_->isChecked());
  }
  if (slice_slider_) {
    slice_slider_->setEnabled(mesh_mode && slice_enable_->isChecked());
  }
  if (mesh_legend_) {
    mesh_legend_->setEnabled(mesh_mode);
    mesh_legend_->setVisible(mesh_mode);
  }

  if (!mesh_mode) {
    if (mesh_legend_) {
      mesh_legend_->setText("Groups: none");
    }
    return;
  }

  if (mesh_group_) {
    mesh_group_->blockSignals(true);
    mesh_group_->clear();
    mesh_group_->addItem("All", -1);
    for (size_t i = 0; i < mesh_groups_.size(); ++i) {
      const auto& g = mesh_groups_[i];
      const QString label = g.name.isEmpty()
                                ? QString("%1:%2").arg(g.dim).arg(g.id)
                                : QString("%1:%2 %3").arg(g.dim).arg(g.id).arg(g.name);
      mesh_group_->addItem(label, static_cast<int>(i));
    }
    mesh_group_->setCurrentIndex(0);
    mesh_group_->blockSignals(false);
  }

  if (mesh_dim_) {
    mesh_dim_->blockSignals(true);
    mesh_dim_->setCurrentIndex(0);
    mesh_dim_->blockSignals(false);
  }

  if (mesh_grid_) {
    mesh_grid_->GetBounds(mesh_bounds_);
    const double dx = mesh_bounds_[1] - mesh_bounds_[0];
    const double dy = mesh_bounds_[3] - mesh_bounds_[2];
    const double dz = mesh_bounds_[5] - mesh_bounds_[4];
    int axis = 0;
    double max_extent = dx;
    if (dy > max_extent) {
      axis = 1;
      max_extent = dy;
    }
    if (dz > max_extent) {
      axis = 2;
    }
    if (slice_axis_) {
      slice_axis_->setCurrentIndex(axis);
    }
    if (slice_slider_) {
      slice_slider_->setValue(50);
    }
  }

  if (mesh_legend_) {
    if (mesh_groups_.empty()) {
      mesh_legend_->setText("Groups: none");
    } else {
      QStringList labels;
      for (const auto& g : mesh_groups_) {
        const QString label =
            g.name.isEmpty() ? QString("%1:%2").arg(g.dim).arg(g.id)
                             : QString("%1:%2 %3").arg(g.dim).arg(g.id).arg(g.name);
        labels << label;
      }
      mesh_legend_->setText(QString("Groups: %1").arg(labels.join(", ")));
    }
  }
#endif
}

void VtkViewer::update_mesh_pipeline() {
#ifdef GMP_ENABLE_VTK_VIEWER
  if (mode_ != DataMode::Mesh) {
    return;
  }
  if (!mesh_grid_ || !mapper_) {
    return;
  }
  if (!mesh_geom_) {
    mesh_geom_ = vtkSmartPointer<vtkDataSetSurfaceFilter>::New();
  }
  if (!mesh_dim_threshold_) {
    mesh_dim_threshold_ = vtkSmartPointer<vtkThreshold>::New();
  }
  if (!mesh_group_threshold_) {
    mesh_group_threshold_ = vtkSmartPointer<vtkThreshold>::New();
  }
  if (!mesh_slice_plane_) {
    mesh_slice_plane_ = vtkSmartPointer<vtkPlane>::New();
  }
  if (!mesh_slice_cutter_) {
    mesh_slice_cutter_ = vtkSmartPointer<vtkCutter>::New();
  }

  if (!mesh_quality_ready_ && mesh_grid_->GetCellData()) {
    if (mesh_grid_->GetCellData()->HasArray("Quality")) {
      mesh_quality_ready_ = true;
    } else {
      auto quality = vtkSmartPointer<vtkMeshQuality>::New();
      quality->SetInputData(mesh_grid_);
      quality->Update();
      auto* arr = quality->GetOutput()->GetCellData()->GetArray("Quality");
      if (arr) {
        vtkSmartPointer<vtkDataArray> copy;
        copy.TakeReference(arr->NewInstance());
        copy->DeepCopy(arr);
        copy->SetName("Quality");
        mesh_grid_->GetCellData()->AddArray(copy);
        mesh_quality_ready_ = true;
      }
    }
  }

  int dim_filter = mesh_dim_ ? mesh_dim_->currentData().toInt() : -1;
  int group_index = mesh_group_ ? mesh_group_->currentData().toInt() : -1;
  int group_dim = -1;
  int group_id = -1;
  if (group_index >= 0 &&
      group_index < static_cast<int>(mesh_groups_.size())) {
    const auto& g = mesh_groups_[group_index];
    group_dim = g.dim;
    group_id = g.id;
    dim_filter = g.dim;
    if (mesh_dim_) {
      const int dim_idx = mesh_dim_->findData(group_dim);
      if (dim_idx >= 0 && mesh_dim_->currentIndex() != dim_idx) {
        mesh_dim_->blockSignals(true);
        mesh_dim_->setCurrentIndex(dim_idx);
        mesh_dim_->blockSignals(false);
      }
    }
  }

  vtkAlgorithmOutput* current_port = nullptr;
  if (dim_filter >= 0) {
    mesh_dim_threshold_->SetInputData(mesh_grid_);
    mesh_dim_threshold_->SetInputArrayToProcess(
        0, 0, 0, vtkDataObject::FIELD_ASSOCIATION_CELLS, "phys_dim");
    mesh_dim_threshold_->SetLowerThreshold(dim_filter);
    mesh_dim_threshold_->SetUpperThreshold(dim_filter);
    current_port = mesh_dim_threshold_->GetOutputPort();
  }

  if (group_index >= 0 && group_id >= 0) {
    if (current_port) {
      mesh_group_threshold_->SetInputConnection(current_port);
    } else {
      mesh_group_threshold_->SetInputData(mesh_grid_);
    }
    mesh_group_threshold_->SetInputArrayToProcess(
        0, 0, 0, vtkDataObject::FIELD_ASSOCIATION_CELLS, "phys_id");
    mesh_group_threshold_->SetLowerThreshold(group_id);
    mesh_group_threshold_->SetUpperThreshold(group_id);
    current_port = mesh_group_threshold_->GetOutputPort();
  }

  if (current_port) {
    mesh_geom_->SetInputConnection(current_port);
  } else {
    mesh_geom_->SetInputData(mesh_grid_);
  }

  vtkAlgorithmOutput* poly_port = mesh_geom_->GetOutputPort();
  const bool slice_on = slice_enable_ && slice_enable_->isChecked();
  if (slice_axis_) {
    slice_axis_->setEnabled(slice_on);
  }
  if (slice_slider_) {
    slice_slider_->setEnabled(slice_on);
  }
  if (slice_on) {
    mesh_grid_->GetBounds(mesh_bounds_);
    const int axis = slice_axis_ ? slice_axis_->currentIndex() : 0;
    const double minv = mesh_bounds_[2 * axis];
    const double maxv = mesh_bounds_[2 * axis + 1];
    const double range = maxv - minv;
    const bool has_range = range > 1e-12;
    if (slice_slider_) {
      slice_slider_->setEnabled(has_range);
    }
    const double t = slice_slider_ ? slice_slider_->value() / 100.0 : 0.5;
    const double pos = has_range ? minv + t * range : minv;
    double origin[3] = {(mesh_bounds_[0] + mesh_bounds_[1]) * 0.5,
                        (mesh_bounds_[2] + mesh_bounds_[3]) * 0.5,
                        (mesh_bounds_[4] + mesh_bounds_[5]) * 0.5};
    origin[axis] = pos;
    double normal[3] = {0.0, 0.0, 0.0};
    normal[axis] = 1.0;
    mesh_slice_plane_->SetOrigin(origin);
    mesh_slice_plane_->SetNormal(normal);
    mesh_slice_cutter_->SetCutFunction(mesh_slice_plane_);
    mesh_slice_cutter_->SetInputConnection(poly_port);
    poly_port = mesh_slice_cutter_->GetOutputPort();
  }

  mapper_->SetInputConnection(poly_port);
  if (slice_on) {
    mesh_slice_cutter_->Update();
  } else {
    mesh_geom_->Update();
  }

  update_nodes_visibility();
  if (render_window_) {
    render_window_->Render();
  }
#endif
}

void VtkViewer::update_nodes_visibility() {
#ifdef GMP_ENABLE_VTK_VIEWER
  if (!renderer_ || !render_window_) {
    return;
  }
  const bool show =
      show_nodes_ && show_nodes_->isChecked() && mode_ == DataMode::Mesh;
  if (!show) {
    if (nodes_actor_) {
      nodes_actor_->SetVisibility(false);
      render_window_->Render();
    }
    return;
  }
  vtkAlgorithmOutput* port = nullptr;
  if (mapper_ && mapper_->GetInputConnection(0, 0)) {
    port = mapper_->GetInputConnection(0, 0);
  } else if (mesh_geom_) {
    port = mesh_geom_->GetOutputPort();
  }
  if (!port) {
    return;
  }
  if (!nodes_filter_) {
    nodes_filter_ = vtkSmartPointer<vtkVertexGlyphFilter>::New();
    nodes_mapper_ = vtkSmartPointer<vtkPolyDataMapper>::New();
    nodes_actor_ = vtkSmartPointer<vtkActor>::New();
    nodes_mapper_->SetInputConnection(nodes_filter_->GetOutputPort());
    nodes_actor_->SetMapper(nodes_mapper_);
    nodes_actor_->GetProperty()->SetPointSize(4);
    nodes_actor_->GetProperty()->SetColor(0.1, 0.1, 0.1);
    renderer_->AddActor(nodes_actor_);
  }
  nodes_filter_->SetInputConnection(port);
  nodes_filter_->Update();
  nodes_actor_->SetVisibility(true);
  render_window_->Render();
#endif
}

}  // namespace gmp
