#include "gmp/MainWindow.h"

#include <QSplitter>
#include <QTabWidget>

#include "gmp/GmshPanel.h"
#include "gmp/MoosePanel.h"
#include "gmp/VtkViewer.h"

namespace gmp {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
  setWindowTitle("GMP-ISE");
  resize(1200, 800);

  auto* splitter = new QSplitter(Qt::Horizontal, this);
  splitter->setChildrenCollapsible(false);

  auto* tabs = new QTabWidget(splitter);
  auto* gmsh_panel = new GmshPanel(tabs);
  auto* moose_panel = new MoosePanel(tabs);
  tabs->addTab(gmsh_panel, "Gmsh");
  tabs->addTab(moose_panel, "MOOSE");
  splitter->addWidget(tabs);

  auto* viewer = new VtkViewer(splitter);
  splitter->addWidget(viewer);

  connect(gmsh_panel, &GmshPanel::mesh_written, moose_panel,
          &MoosePanel::set_mesh_path);
  connect(gmsh_panel, &GmshPanel::boundary_groups, moose_panel,
          &MoosePanel::set_boundary_groups);
  connect(moose_panel, &MoosePanel::exodus_ready, viewer,
          &VtkViewer::set_exodus_file);
  connect(moose_panel, &MoosePanel::exodus_history, viewer,
          &VtkViewer::set_exodus_history);

  splitter->setStretchFactor(0, 0);
  splitter->setStretchFactor(1, 1);

  setCentralWidget(splitter);
}

}  // namespace gmp
