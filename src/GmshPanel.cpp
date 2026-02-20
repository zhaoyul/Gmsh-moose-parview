#include "gmp/GmshPanel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QString>
#include <QVBoxLayout>
#include <vector>

#ifdef GMP_ENABLE_GMSH_GUI
#include <gmsh.h>
#endif

namespace gmp {

GmshPanel::GmshPanel(QWidget* parent) : QWidget(parent) {
  auto* layout = new QVBoxLayout(this);

  auto* title = new QLabel("Gmsh Panel");
  layout->addWidget(title);

  auto* geo_box = new QGroupBox("Geometry");
  auto* geo_form = new QFormLayout(geo_box);

  size_x_ = new QDoubleSpinBox();
  size_x_->setRange(0.01, 1000.0);
  size_x_->setValue(1.0);
  size_y_ = new QDoubleSpinBox();
  size_y_->setRange(0.01, 1000.0);
  size_y_->setValue(1.0);
  size_z_ = new QDoubleSpinBox();
  size_z_->setRange(0.01, 1000.0);
  size_z_->setValue(1.0);

  geo_form->addRow("Size X", size_x_);
  geo_form->addRow("Size Y", size_y_);
  geo_form->addRow("Size Z", size_z_);
  layout->addWidget(geo_box);

  auto* mesh_box = new QGroupBox("Mesh");
  auto* mesh_form = new QFormLayout(mesh_box);

  mesh_size_ = new QDoubleSpinBox();
  mesh_size_->setRange(0.01, 1000.0);
  mesh_size_->setValue(0.2);
  mesh_size_->setSingleStep(0.05);
  mesh_form->addRow("Mesh Size", mesh_size_);

  elem_order_ = new QComboBox();
  elem_order_->addItem("Linear (1)", 1);
  elem_order_->addItem("Quadratic (2)", 2);
  mesh_form->addRow("Element Order", elem_order_);

  msh_version_ = new QComboBox();
  msh_version_->addItem("MSH 2.2", 2);
  msh_version_->addItem("MSH 4.1", 4);
  mesh_form->addRow("MSH Version", msh_version_);

  optimize_ = new QCheckBox("Optimize Mesh");
  optimize_->setChecked(true);
  mesh_form->addRow("", optimize_);

  layout->addWidget(mesh_box);

  auto* form = new QFormLayout();
  output_path_ = new QLineEdit();
  output_path_->setPlaceholderText("Output mesh path (*.msh)");
  output_path_->setText(QDir::currentPath() + "/out/box.msh");

  auto* pick_btn = new QPushButton("Pick Output");
  connect(pick_btn, &QPushButton::clicked, this, &GmshPanel::on_pick_output);

  auto* path_row = new QHBoxLayout();
  path_row->addWidget(output_path_);
  path_row->addWidget(pick_btn);

  auto* path_container = new QWidget();
  path_container->setLayout(path_row);
  form->addRow("Mesh Output", path_container);

  layout->addLayout(form);

  auto* generate_btn = new QPushButton("Generate Sample Mesh");
  connect(generate_btn, &QPushButton::clicked, this, &GmshPanel::on_generate);
  layout->addWidget(generate_btn);

  log_ = new QPlainTextEdit();
  log_->setReadOnly(true);
  layout->addWidget(log_, 1);

  append_log("Gmsh panel ready.");
#ifndef GMP_ENABLE_GMSH_GUI
  append_log("Gmsh is disabled. Rebuild with -DGMP_ENABLE_GMSH_GUI=ON.");
#endif
}

GmshPanel::~GmshPanel() {
#ifdef GMP_ENABLE_GMSH_GUI
  if (gmsh_ready_) {
    gmsh::finalize();
  }
#endif
}

void GmshPanel::on_pick_output() {
  const QString path =
      QFileDialog::getSaveFileName(this, "Select mesh output", output_path_->text(),
                                   "Gmsh Mesh (*.msh)");
  if (!path.isEmpty()) {
    output_path_->setText(path);
  }
}

void GmshPanel::on_generate() {
#ifndef GMP_ENABLE_GMSH_GUI
  append_log("Gmsh is not enabled in this build.");
  return;
#else
  try {
    if (!gmsh_ready_) {
      gmsh::initialize();
      gmsh_ready_ = true;
      append_log("Gmsh initialized.");
    }

    gmsh::option::setNumber("General.Terminal", 0);
    gmsh::logger::start();

    const double dx = size_x_->value();
    const double dy = size_y_->value();
    const double dz = size_z_->value();
    const double lc = mesh_size_->value();
    const int order = elem_order_->currentData().toInt();
    const int msh_version = msh_version_->currentData().toInt();

    gmsh::option::setNumber("Mesh.CharacteristicLengthMin", lc);
    gmsh::option::setNumber("Mesh.CharacteristicLengthMax", lc);
    gmsh::option::setNumber("Mesh.ElementOrder", order);
    gmsh::option::setNumber("Mesh.Optimize", optimize_->isChecked() ? 1 : 0);
    gmsh::option::setNumber("Mesh.MshFileVersion", msh_version == 2 ? 2.2 : 4.1);

    gmsh::clear();
    gmsh::model::add("box_model");
    const int box = gmsh::model::occ::addBox(0, 0, 0, dx, dy, dz);
    gmsh::model::occ::synchronize();

    const int phys = gmsh::model::addPhysicalGroup(3, {box});
    gmsh::model::setPhysicalName(3, phys, "solid");
    std::vector<std::pair<int, int>> faces;
    gmsh::model::getEntities(faces, 2);
    if (!faces.empty()) {
      std::vector<int> face_tags;
      face_tags.reserve(faces.size());
      for (const auto& f : faces) {
        face_tags.push_back(f.second);
      }
      const int bnd = gmsh::model::addPhysicalGroup(2, face_tags);
      gmsh::model::setPhysicalName(2, bnd, "boundary");
    }

    gmsh::model::mesh::generate(3);

    const QString out_path = output_path_->text();
    QDir().mkpath(QFileInfo(out_path).absolutePath());
    gmsh::write(out_path.toStdString());

    std::vector<std::pair<int, int>> phys_groups;
    gmsh::model::getPhysicalGroups(phys_groups);
    QStringList boundary_names;
    for (const auto& p : phys_groups) {
      if (p.first != 2) {
        continue;
      }
      std::string name;
      gmsh::model::getPhysicalName(p.first, p.second, name);
      if (name.empty()) {
        name = "boundary_" + std::to_string(p.second);
      }
      boundary_names << QString::fromStdString(name);
    }
    if (!boundary_names.isEmpty()) {
      emit boundary_groups(boundary_names);
    }

    std::vector<std::string> log;
    gmsh::logger::get(log);
    gmsh::logger::stop();
    for (const auto& line : log) {
      append_log(QString::fromStdString(line));
    }

    append_log("Mesh written: " + out_path);
    emit mesh_written(out_path);

    std::vector<std::size_t> node_tags;
    std::vector<double> node_coords;
    std::vector<double> node_params;
    gmsh::model::mesh::getNodes(node_tags, node_coords, node_params);

    std::vector<int> element_types;
    std::vector<std::vector<std::size_t>> element_tags;
    std::vector<std::vector<std::size_t>> element_nodes;
    gmsh::model::mesh::getElements(element_types, element_tags, element_nodes);
    std::size_t elem_count = 0;
    for (const auto& tags : element_tags) {
      elem_count += tags.size();
    }

    append_log(QString("Nodes: %1, Elements: %2").arg(node_tags.size()).arg(elem_count));
  } catch (const std::exception& ex) {
    append_log(QString("Gmsh error: %1").arg(ex.what()));
  }
#endif
}

void GmshPanel::append_log(const QString& text) {
  if (log_) {
    log_->appendPlainText(text);
  }
}

}  // namespace gmp
