#include "gmp/VtkViewer.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>
#include <QtCore/Qt>

#ifdef GMP_ENABLE_VTK_VIEWER
#include <QVTKOpenGLNativeWidget.h>
#include <vtkActor.h>
#include <vtkCellData.h>
#include <vtkCompositeDataGeometryFilter.h>
#include <vtkDataArray.h>
#include <vtkExodusIIReader.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkInformation.h>
#include <vtkLookupTable.h>
#include <vtkProperty.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkRenderer.h>
#include <vtkScalarBarActor.h>
#include <vtkStreamingDemandDrivenPipeline.h>
#endif

namespace gmp {

VtkViewer::VtkViewer(QWidget* parent) : QWidget(parent) {
  auto* layout = new QVBoxLayout(this);

  auto* header = new QHBoxLayout();
  file_label_ = new QLabel("No file loaded");
  open_btn_ = new QPushButton("Open .e");
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
  connect(array_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &VtkViewer::on_array_changed);
  preset_combo_ = new QComboBox();
  preset_combo_->addItem("Blue-Red");
  preset_combo_->addItem("Grayscale");
  preset_combo_->addItem("Rainbow");
  connect(preset_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &VtkViewer::on_preset_changed);
  repr_combo_ = new QComboBox();
  repr_combo_->addItem("Surface");
  repr_combo_->addItem("Wireframe");
  repr_combo_->addItem("Surface + Edges");
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
    geom_ = vtkSmartPointer<vtkCompositeDataGeometryFilter>::New();
    mapper_ = vtkSmartPointer<vtkPolyDataMapper>::New();
    actor_ = vtkSmartPointer<vtkActor>::New();
    lut_ = vtkSmartPointer<vtkLookupTable>::New();
    lut_->SetNumberOfTableValues(256);
    lut_->Build();
    mapper_->SetLookupTable(lut_);
    scalar_bar_ = vtkSmartPointer<vtkScalarBarActor>::New();
  }

  if (!pipeline_ready_) {
    geom_->SetInputConnection(reader_->GetOutputPort());
    mapper_->SetInputConnection(geom_->GetOutputPort());
    actor_->SetMapper(mapper_);
    renderer_->AddActor(actor_);
    scalar_bar_->SetLookupTable(mapper_->GetLookupTable());
    renderer_->AddViewProp(scalar_bar_);
    pipeline_ready_ = true;
  }

  first_render_ = true;
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
  setup_watcher(path);
  update_pipeline();
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

void VtkViewer::on_reload() {
  if (!current_file_.isEmpty()) {
    set_exodus_file(current_file_);
  }
}

void VtkViewer::on_time_changed(int index) {
#ifdef GMP_ENABLE_VTK_VIEWER
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
  if (!mapper_ || !geom_) {
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

  auto* poly = geom_->GetOutput();
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
  if (!reader_ || !geom_ || !render_window_) {
    return;
  }
  if (current_file_.isEmpty()) {
    return;
  }
  if (!pipeline_ready_) {
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
  if (!geom_) {
    return;
  }
  auto* poly = geom_->GetOutput();
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
      QFileDialog::getOpenFileName(this, "Open Exodus File", current_file_,
                                   "Exodus (*.e)");
  if (!path.isEmpty()) {
    set_exodus_file(path);
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
  if (current_file_.isEmpty() || !reader_) {
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
    update_time_steps_from_reader(true);
    if (array_combo_->count() == 0) {
      populate_arrays();
    }
  }
  refresh_time_only();
#endif
}

void VtkViewer::refresh_time_only() {
#ifdef GMP_ENABLE_VTK_VIEWER
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
  if (!reader_) {
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

}  // namespace gmp
