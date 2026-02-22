#include "gmp/MainWindow.h"

#include <QFileDialog>
#include <QDir>
#include <QInputDialog>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QStatusBar>
#include <QSplitter>
#include <QStackedWidget>
#include <QTabBar>
#include <QTabWidget>
#include <QToolBar>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QStyle>
#include <QKeySequence>
#include <QApplication>
#include <QFont>
#include <QPainter>
#include <QPainterPath>
#include <QVariantMap>
#include <QMetaType>

#include <fstream>
#include <QFileInfo>
#include <yaml-cpp/yaml.h>

#include "gmp/GmshPanel.h"
#include "gmp/MoosePanel.h"
#include "gmp/PropertyEditor.h"
#include "gmp/VtkViewer.h"

namespace gmp {

namespace {

enum class IconGlyph {
  NewFile,
  OpenFolder,
  SaveDisk,
  Sync,
  Mesh,
  Run,
  Check,
  Stop,
  Part,
  Material,
  Section,
  Step,
  Function,
  Variable,
  BC,
  Load,
  Output,
  Interaction,
  Job,
  Result,
};

QIcon MakeIcon(IconGlyph glyph, int size = 18) {
  QPixmap pix(size, size);
  pix.fill(Qt::transparent);
  QPainter p(&pix);
  p.setRenderHint(QPainter::Antialiasing, true);
  QPen pen(QColor("#2b2b2b"));
  pen.setWidthF(1.6);
  p.setPen(pen);

  const int s = size;
  const int m = 3;
  const QRect r(m, m, s - 2 * m, s - 2 * m);

  switch (glyph) {
    case IconGlyph::NewFile: {
      p.drawRect(r);
      p.drawLine(s / 2, m + 2, s / 2, s - m - 2);
      p.drawLine(m + 2, s / 2, s - m - 2, s / 2);
      break;
    }
    case IconGlyph::OpenFolder: {
      QRect folder(m, m + 4, s - 2 * m, s - m - 6);
      p.drawRect(folder);
      p.drawLine(m + 2, m + 4, s / 2, m + 4);
      p.drawLine(m + 2, m + 4, m + 6, m + 1);
      break;
    }
    case IconGlyph::SaveDisk: {
      p.drawRect(r);
      p.drawLine(m + 3, m + 5, s - m - 3, m + 5);
      p.drawRect(QRect(m + 4, m + 8, s - 2 * m - 8, 5));
      break;
    }
    case IconGlyph::Sync: {
      p.drawArc(r, 40 * 16, 220 * 16);
      p.drawArc(r, 260 * 16, 220 * 16);
      p.drawLine(s - m - 2, s / 2, s - m - 6, s / 2 - 3);
      p.drawLine(s - m - 2, s / 2, s - m - 6, s / 2 + 3);
      break;
    }
    case IconGlyph::Mesh: {
      for (int i = 0; i < 3; ++i) {
        int x = m + i * (r.width() / 2);
        p.drawLine(x, m, x, s - m);
        int y = m + i * (r.height() / 2);
        p.drawLine(m, y, s - m, y);
      }
      break;
    }
    case IconGlyph::Run: {
      QPolygon poly;
      poly << QPoint(m + 2, m + 1) << QPoint(s - m - 2, s / 2)
           << QPoint(m + 2, s - m - 1);
      p.setBrush(QColor("#2b2b2b"));
      p.drawPolygon(poly);
      break;
    }
    case IconGlyph::Check: {
      p.drawLine(m + 2, s / 2, s / 2 - 1, s - m - 2);
      p.drawLine(s / 2 - 1, s - m - 2, s - m - 2, m + 3);
      break;
    }
    case IconGlyph::Stop: {
      p.setBrush(QColor("#2b2b2b"));
      p.drawRect(QRect(m + 3, m + 3, s - 2 * m - 6, s - 2 * m - 6));
      break;
    }
    case IconGlyph::Part: {
      QRect back(m + 3, m + 1, s - 2 * m - 6, s - 2 * m - 6);
      QRect front(m, m + 4, s - 2 * m - 6, s - 2 * m - 6);
      p.drawRect(back);
      p.drawRect(front);
      p.drawLine(front.topLeft(), back.topLeft());
      p.drawLine(front.topRight(), back.topRight());
      p.drawLine(front.bottomLeft(), back.bottomLeft());
      break;
    }
    case IconGlyph::Material: {
      p.setBrush(QColor("#2b2b2b"));
      p.drawEllipse(r.adjusted(2, 2, -2, -2));
      break;
    }
    case IconGlyph::Section: {
      p.drawLine(m + 2, m + 4, s - m - 2, m + 4);
      p.drawLine(m + 2, s / 2, s - m - 2, s / 2);
      p.drawLine(m + 2, s - m - 4, s - m - 2, s - m - 4);
      break;
    }
    case IconGlyph::Step: {
      QPolygon poly;
      poly << QPoint(m + 2, m + 1) << QPoint(s - m - 2, s / 2)
           << QPoint(m + 2, s - m - 1);
      p.drawPolygon(poly);
      break;
    }
    case IconGlyph::Function: {
      QPainterPath path;
      path.moveTo(m + 1, s - m - 2);
      path.cubicTo(s / 3, m + 1, s / 2, s - m - 2, s - m - 1, m + 2);
      p.drawPath(path);
      break;
    }
    case IconGlyph::Variable: {
      p.drawLine(m + 2, m + 2, s - m - 2, s - m - 2);
      p.drawLine(m + 2, s - m - 2, s - m - 2, m + 2);
      break;
    }
    case IconGlyph::BC: {
      p.drawRect(r);
      p.drawLine(m, m, s - m, m);
      break;
    }
    case IconGlyph::Load: {
      p.drawLine(s / 2, m + 2, s / 2, s - m - 2);
      p.drawLine(s / 2, m + 2, s / 2 - 3, m + 6);
      p.drawLine(s / 2, m + 2, s / 2 + 3, m + 6);
      break;
    }
    case IconGlyph::Output: {
      p.drawRect(r);
      p.drawLine(s / 2, m + 2, s / 2, s - m - 6);
      p.drawLine(s / 2, s - m - 6, s / 2 - 3, s - m - 9);
      p.drawLine(s / 2, s - m - 6, s / 2 + 3, s - m - 9);
      break;
    }
    case IconGlyph::Interaction: {
      p.drawLine(m + 2, s / 2, s - m - 2, s / 2);
      p.drawLine(m + 2, s / 2, m + 6, s / 2 - 3);
      p.drawLine(m + 2, s / 2, m + 6, s / 2 + 3);
      p.drawLine(s - m - 2, s / 2, s - m - 6, s / 2 - 3);
      p.drawLine(s - m - 2, s / 2, s - m - 6, s / 2 + 3);
      break;
    }
    case IconGlyph::Job: {
      p.drawRect(r);
      p.drawLine(m + 2, m + 2, s - m - 2, s - m - 2);
      p.drawLine(m + 2, s - m - 2, s - m - 2, m + 2);
      break;
    }
    case IconGlyph::Result: {
      p.drawRect(r);
      p.drawLine(m + 2, s - m - 3, s / 2, s / 2);
      p.drawLine(s / 2, s / 2, s - m - 2, m + 3);
      break;
    }
  }

  return QIcon(pix);
}

}  // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
  setWindowTitle("GMP-ISE");
  resize(1400, 900);

  build_menu();
  build_toolbar();
  apply_theme();

  auto* central = new QWidget(this);
  auto* main_layout = new QVBoxLayout(central);
  main_layout->setContentsMargins(8, 8, 8, 8);
  main_layout->setSpacing(6);

  module_tabs_ = new QTabBar(central);
  module_tabs_->addTab("Part");
  module_tabs_->addTab("Property");
  module_tabs_->addTab("Assembly");
  module_tabs_->addTab("Step");
  module_tabs_->addTab("Interaction");
  module_tabs_->addTab("Load");
  module_tabs_->addTab("Mesh");
  module_tabs_->addTab("Job");
  module_tabs_->addTab("Visualization");
  main_layout->addWidget(module_tabs_);

  auto* vertical_split = new QSplitter(Qt::Vertical, central);
  vertical_split->setChildrenCollapsible(false);
  main_layout->addWidget(vertical_split, 1);

  auto* main_split = new QSplitter(Qt::Horizontal, vertical_split);
  main_split->setChildrenCollapsible(false);
  vertical_split->addWidget(main_split);

  model_tree_ = new QTreeWidget(main_split);
  model_tree_->setHeaderLabel("Model Tree");
  model_tree_->setMinimumWidth(220);
  build_model_tree();

  auto* center_tabs = new QTabWidget(main_split);
  viewer_ = new VtkViewer(center_tabs);
  center_tabs->addTab(viewer_, "Viewport");
  center_tabs->addTab(new QLabel("Plot view (placeholder)", center_tabs),
                      "Plot");
  center_tabs->addTab(new QLabel("Table view (placeholder)", center_tabs),
                      "Table");

  property_stack_ = new QStackedWidget(main_split);
  property_stack_->setMinimumWidth(340);

  property_editor_ = new PropertyEditor(property_stack_);
  auto* mesh_page = new GmshPanel(property_stack_);
  auto* job_page = new MoosePanel(property_stack_);
  moose_panel_ = job_page;
  gmsh_panel_ = mesh_page;

  property_stack_->addWidget(property_editor_);
  property_stack_->addWidget(mesh_page);
  property_stack_->addWidget(job_page);

  console_ = new QPlainTextEdit(vertical_split);
  console_->setReadOnly(true);
  console_->setMinimumHeight(120);
  console_->setPlaceholderText("Job/Message Console");
  vertical_split->addWidget(console_);

  vertical_split->setStretchFactor(0, 1);
  vertical_split->setStretchFactor(1, 0);
  main_split->setStretchFactor(0, 0);
  main_split->setStretchFactor(1, 1);
  main_split->setStretchFactor(2, 0);

  connect(module_tabs_, &QTabBar::currentChanged, this, [this](int index) {
    if (index == 6) {
      property_stack_->setCurrentIndex(1);
    } else if (index == 7) {
      property_stack_->setCurrentIndex(2);
    } else {
      property_stack_->setCurrentIndex(0);
    }
  });
  module_tabs_->setCurrentIndex(6);
  property_stack_->setCurrentIndex(1);

  connect(mesh_page, &GmshPanel::mesh_written, job_page,
          &MoosePanel::set_mesh_path);
  connect(mesh_page, &GmshPanel::boundary_groups, job_page,
          &MoosePanel::set_boundary_groups);
  connect(mesh_page, &GmshPanel::mesh_written, viewer_,
          &VtkViewer::set_mesh_file);
  connect(mesh_page, &GmshPanel::physical_group_selected, viewer_,
          &VtkViewer::set_mesh_group_filter);
  connect(mesh_page, &GmshPanel::mesh_written, this,
          [this](const QString& path) {
            upsert_mesh_item(path);
            statusBar()->showMessage("Mesh generated.", 2000);
          });
  connect(job_page, &MoosePanel::exodus_ready, viewer_,
          &VtkViewer::set_exodus_file);
  connect(job_page, &MoosePanel::exodus_history, viewer_,
          &VtkViewer::set_exodus_history);
  connect(job_page, &MoosePanel::job_started, this,
          [this](const QVariantMap& info) {
            auto* root = find_root_item("Jobs");
            if (!root) {
              return;
            }
            const QString input_path = info.value("input").toString();
            const QString base = QFileInfo(input_path).baseName();
            const QString name =
                base.isEmpty()
                    ? QString("job_%1").arg(root->childCount() + 1)
                    : base;
            active_job_item_ = add_child_item(root, name, "Jobs", info);
            statusBar()->showMessage("Job running...", 2000);
          });
  connect(job_page, &MoosePanel::job_finished, this,
          [this](const QVariantMap& info) {
            if (!active_job_item_) {
              return;
            }
            QVariantMap params =
                active_job_item_->data(0, PropertyEditor::kParamsRole).toMap();
            for (auto it = info.begin(); it != info.end(); ++it) {
              params.insert(it.key(), it.value());
            }
            active_job_item_->setData(0, PropertyEditor::kParamsRole, params);
            const QString exodus = info.value("exodus").toString();
            if (!exodus.isEmpty()) {
              upsert_result_item(exodus, active_job_item_->text(0));
            }
            active_job_item_ = nullptr;
            statusBar()->showMessage("Job finished.", 2000);
          });
  connect(job_page, &MoosePanel::exodus_ready, this,
          [this](const QString& path) { upsert_result_item(path, ""); });

  connect(model_tree_, &QTreeWidget::itemSelectionChanged, this, [this]() {
    auto* item = model_tree_->currentItem();
    property_editor_->set_item(item);
  });

  setCentralWidget(central);
  statusBar()->showMessage("Ready");
}

void MainWindow::build_menu() {
  auto* file_menu = menuBar()->addMenu("&File");
  action_new_ = file_menu->addAction("New Project");
  action_open_ = file_menu->addAction("Open Project...");
  action_save_ = file_menu->addAction("Save Project");
  action_save_as_ = file_menu->addAction("Save Project As...");
  action_screenshot_ = file_menu->addAction("Save Screenshot...");
  action_new_->setShortcut(QKeySequence::New);
  action_open_->setShortcut(QKeySequence::Open);
  action_save_->setShortcut(QKeySequence::Save);
  action_save_as_->setShortcut(QKeySequence::SaveAs);
  action_screenshot_->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_P));

  auto* model_menu = menuBar()->addMenu("&Model");
  action_sync_ = model_menu->addAction("Sync Model -> MOOSE Input");
  action_sync_->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_R));

  auto* mesh_menu = menuBar()->addMenu("&Mesh");
  action_mesh_ = mesh_menu->addAction("Generate Mesh");
  action_preview_mesh_ = mesh_menu->addAction("Preview Mesh...");
  action_mesh_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_M));
  action_preview_mesh_->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_M));

  auto* job_menu = menuBar()->addMenu("&Job");
  action_run_ = job_menu->addAction("Run");
  action_check_ = job_menu->addAction("Check Input");
  action_stop_ = job_menu->addAction("Stop");
  action_run_->setShortcut(QKeySequence(Qt::Key_F5));
  action_check_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_K));
  action_stop_->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F5));

  auto* demo_menu = menuBar()->addMenu("&Demos");
  auto* demo_setup_diff =
      demo_menu->addAction("Setup Transient Diffusion");
  auto* demo_run_diff = demo_menu->addAction("Run Transient Diffusion");
  demo_menu->addSeparator();
  auto* demo_setup_tm =
      demo_menu->addAction("Setup Thermo-Mechanics");
  auto* demo_run_tm = demo_menu->addAction("Run Thermo-Mechanics");
  demo_menu->addSeparator();
  auto* demo_setup_nl =
      demo_menu->addAction("Setup Nonlinear Heat");
  auto* demo_run_nl = demo_menu->addAction("Run Nonlinear Heat");

  connect(action_new_, &QAction::triggered, this, [this]() {
    project_path_.clear();
    clear_model_tree_children();
    property_editor_->set_item(nullptr);
    console_->appendPlainText("New project created.");
    statusBar()->showMessage("New project created.", 2000);
  });
  connect(action_open_, &QAction::triggered, this, [this]() {
    const QString path = QFileDialog::getOpenFileName(
        this, "Open Project", project_path_,
        "GMP Project (*.gmp.yaml *.yaml)");
    if (path.isEmpty()) {
      return;
    }
    load_project(path);
    statusBar()->showMessage("Project loaded.", 2000);
  });
  connect(action_save_, &QAction::triggered, this, [this]() {
    if (project_path_.isEmpty()) {
      const QString path = QFileDialog::getSaveFileName(
          this, "Save Project", project_path_,
          "GMP Project (*.gmp.yaml *.yaml)");
      if (path.isEmpty()) {
        return;
      }
      project_path_ = path;
    }
    if (save_project(project_path_)) {
      console_->appendPlainText("Project saved: " + project_path_);
      statusBar()->showMessage("Project saved.", 2000);
    }
  });
  connect(action_save_as_, &QAction::triggered, this, [this]() {
    const QString path = QFileDialog::getSaveFileName(
        this, "Save Project As", project_path_,
        "GMP Project (*.gmp.yaml *.yaml)");
    if (path.isEmpty()) {
      return;
    }
    project_path_ = path;
    if (save_project(project_path_)) {
      console_->appendPlainText("Project saved: " + project_path_);
      statusBar()->showMessage("Project saved.", 2000);
    }
  });
  connect(action_screenshot_, &QAction::triggered, this, [this]() {
    const QString path = QFileDialog::getSaveFileName(
        this, "Save Screenshot", QDir::homePath(),
        "PNG Image (*.png)");
    if (path.isEmpty()) {
      return;
    }
    if (viewer_ && viewer_->save_screenshot(path)) {
      console_->appendPlainText("Screenshot saved: " + path);
      statusBar()->showMessage("Screenshot saved.", 2000);
    } else {
      statusBar()->showMessage("Failed to save screenshot.", 2000);
    }
  });
  connect(action_sync_, &QAction::triggered, this,
          [this]() { sync_model_to_input(); });
  connect(action_mesh_, &QAction::triggered, this, [this]() {
    if (gmsh_panel_) {
      gmsh_panel_->generate_mesh();
      return;
    }
    statusBar()->showMessage("Mesh panel not ready.", 2000);
  });
  connect(action_preview_mesh_, &QAction::triggered, this, [this]() {
    const QString path = QFileDialog::getOpenFileName(
        this, "Open Gmsh Mesh", QDir::homePath(), "Gmsh Mesh (*.msh)");
    if (path.isEmpty()) {
      return;
    }
    if (viewer_) {
      viewer_->set_mesh_file(path);
      statusBar()->showMessage("Mesh loaded.", 2000);
    }
  });
  connect(action_run_, &QAction::triggered, this, [this]() {
    if (moose_panel_) {
      moose_panel_->run_job();
      return;
    }
    statusBar()->showMessage("Job panel not ready.", 2000);
  });
  connect(action_check_, &QAction::triggered, this, [this]() {
    if (moose_panel_) {
      moose_panel_->check_input();
      return;
    }
    statusBar()->showMessage("Job panel not ready.", 2000);
  });
  connect(action_stop_, &QAction::triggered, this, [this]() {
    if (moose_panel_) {
      moose_panel_->stop_job();
      return;
    }
    statusBar()->showMessage("Job panel not ready.", 2000);
  });
  connect(demo_setup_diff, &QAction::triggered, this,
          [this]() { load_demo_diffusion(false); });
  connect(demo_run_diff, &QAction::triggered, this,
          [this]() { load_demo_diffusion(true); });
  connect(demo_setup_tm, &QAction::triggered, this,
          [this]() { load_demo_thermo(false); });
  connect(demo_run_tm, &QAction::triggered, this,
          [this]() { load_demo_thermo(true); });
  connect(demo_setup_nl, &QAction::triggered, this,
          [this]() { load_demo_nonlinear_heat(false); });
  connect(demo_run_nl, &QAction::triggered, this,
          [this]() { load_demo_nonlinear_heat(true); });
}

void MainWindow::build_toolbar() {
  auto* toolbar = addToolBar("Main");
  toolbar->setMovable(false);
  toolbar->setIconSize(QSize(18, 18));

  if (action_new_) {
    action_new_->setIcon(MakeIcon(IconGlyph::NewFile));
    toolbar->addAction(action_new_);
  }
  if (action_open_) {
    action_open_->setIcon(MakeIcon(IconGlyph::OpenFolder));
    toolbar->addAction(action_open_);
  }
  if (action_save_) {
    action_save_->setIcon(MakeIcon(IconGlyph::SaveDisk));
    toolbar->addAction(action_save_);
  }
  if (action_screenshot_) {
    action_screenshot_->setIcon(MakeIcon(IconGlyph::Output));
    toolbar->addAction(action_screenshot_);
  }
  if (action_sync_) {
    action_sync_->setIcon(MakeIcon(IconGlyph::Sync));
    toolbar->addAction(action_sync_);
  }
  toolbar->addSeparator();
  if (action_mesh_) {
    action_mesh_->setIcon(MakeIcon(IconGlyph::Mesh));
    toolbar->addAction(action_mesh_);
  }
  if (action_preview_mesh_) {
    action_preview_mesh_->setIcon(MakeIcon(IconGlyph::OpenFolder));
    toolbar->addAction(action_preview_mesh_);
  }
  if (action_run_) {
    action_run_->setIcon(MakeIcon(IconGlyph::Run));
    toolbar->addAction(action_run_);
  }
  if (action_check_) {
    action_check_->setIcon(MakeIcon(IconGlyph::Check));
    toolbar->addAction(action_check_);
  }
  if (action_stop_) {
    action_stop_->setIcon(MakeIcon(IconGlyph::Stop));
    toolbar->addAction(action_stop_);
  }
}

void MainWindow::apply_theme() {
  QFont font = QApplication::font();
#if defined(Q_OS_MAC)
  font.setFamily("Helvetica Neue");
#elif defined(Q_OS_WIN)
  font.setFamily("Segoe UI");
#else
  font.setFamily("Noto Sans");
#endif
  font.setPointSize(12);
  QApplication::setFont(font);

  const QString style = R"(
QMainWindow { background: #e6e6e6; }
QMenuBar {
  background: #d4d4d4;
  border-bottom: 1px solid #b5b5b5;
}
QMenuBar::item { padding: 4px 10px; }
QMenuBar::item:selected { background: #c9c9c9; }
QTabBar::tab {
  background: #d9d9d9;
  border: 1px solid #b5b5b5;
  padding: 6px 14px;
  margin-right: 2px;
}
QTabBar::tab:selected { background: #f2f2f2; }
QTreeWidget, QPlainTextEdit, QLineEdit, QTableWidget, QComboBox, QSpinBox,
QDoubleSpinBox {
  background: #fbfbfb;
  border: 1px solid #b5b5b5;
}
QComboBox QAbstractItemView {
  background: #fbfbfb;
  border: 1px solid #b5b5b5;
  selection-background-color: #cfe1ff;
  selection-color: #111;
  outline: 0;
}
QComboBox QAbstractItemView::item:hover {
  background: #cfe1ff;
  color: #111;
}
QTreeView::item { padding: 4px 6px; }
QTreeView::item:selected { background: #cfe1ff; color: #111; }
QTableWidget::item { padding: 2px 4px; }
QHeaderView::section {
  background: #e0e0e0;
  padding: 4px;
  border: 1px solid #b5b5b5;
}
QGroupBox {
  border: 1px solid #b5b5b5;
  margin-top: 8px;
}
QGroupBox::title {
  subcontrol-origin: margin;
  left: 8px;
  padding: 0 4px;
}
QToolBar {
  background: #d4d4d4;
  border-bottom: 1px solid #b5b5b5;
}
QStatusBar {
  background: #d4d4d4;
  border-top: 1px solid #b5b5b5;
}
QPushButton {
  background: #f2f2f2;
  border: 1px solid #b5b5b5;
  padding: 4px 10px;
  min-height: 24px;
  min-width: 72px;
}
QPushButton:hover { background: #f9f9f9; }
QPushButton:pressed { background: #e0e0e0; }
QToolButton {
  background: transparent;
  padding: 2px 4px;
}
QToolButton:hover { background: #cfcfcf; }
QToolButton:checked { background: #c9c9c9; }
)";
  setStyleSheet(style);
}

void MainWindow::build_model_tree() {
  const QStringList root_nodes = {"Parts", "Materials",  "Sections",  "Steps",
                                  "Functions", "Variables", "BC", "Loads",
                                  "Outputs", "Interactions", "Mesh", "Jobs",
                                  "Results"};
  for (const auto& name : root_nodes) {
    auto* item = new QTreeWidgetItem(model_tree_);
    item->setText(0, name);
    item->setExpanded(true);
    item->setData(0, PropertyEditor::kKindRole, name);
    QIcon icon;
    if (name == "Parts") {
      icon = MakeIcon(IconGlyph::Part);
    } else if (name == "Materials") {
      icon = MakeIcon(IconGlyph::Material);
    } else if (name == "Sections") {
      icon = MakeIcon(IconGlyph::Section);
    } else if (name == "Steps") {
      icon = MakeIcon(IconGlyph::Step);
    } else if (name == "Functions") {
      icon = MakeIcon(IconGlyph::Function);
    } else if (name == "Variables") {
      icon = MakeIcon(IconGlyph::Variable);
    } else if (name == "BC") {
      icon = MakeIcon(IconGlyph::BC);
    } else if (name == "Loads") {
      icon = MakeIcon(IconGlyph::Load);
    } else if (name == "Outputs") {
      icon = MakeIcon(IconGlyph::Output);
    } else if (name == "Interactions") {
      icon = MakeIcon(IconGlyph::Interaction);
    } else if (name == "Mesh") {
      icon = MakeIcon(IconGlyph::Mesh);
    } else if (name == "Jobs") {
      icon = MakeIcon(IconGlyph::Job);
    } else if (name == "Results") {
      icon = MakeIcon(IconGlyph::Result);
    }
    if (!icon.isNull()) {
      item->setIcon(0, icon);
    }
  }

  model_tree_->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(model_tree_, &QTreeWidget::customContextMenuRequested, this,
          [this](const QPoint& pos) {
            auto* item = model_tree_->itemAt(pos);
            if (!item) {
              return;
            }
            QMenu menu(this);
            if (!item->parent()) {
              auto* add_action =
                  menu.addAction(QString("Add %1").arg(item->text(0)));
              connect(add_action, &QAction::triggered, this,
                      [this, item]() { add_item_under_root(item); });
            } else {
              auto* rename_action = menu.addAction("Rename");
              auto* delete_action = menu.addAction("Delete");
              connect(rename_action, &QAction::triggered, this, [this, item]() {
                const QString name = QInputDialog::getText(
                    this, "Rename Item", "Name:", QLineEdit::Normal,
                    item->text(0));
                if (!name.isEmpty()) {
                  item->setText(0, name);
                }
              });
              connect(delete_action, &QAction::triggered, this,
                      [this, item]() { remove_item(item); });
            }
            menu.exec(model_tree_->viewport()->mapToGlobal(pos));
          });
}

void MainWindow::clear_model_tree_children() {
  for (int i = 0; i < model_tree_->topLevelItemCount(); ++i) {
    auto* root = model_tree_->topLevelItem(i);
    if (!root) {
      continue;
    }
    root->takeChildren();
  }
}

QTreeWidgetItem* MainWindow::find_root_item(const QString& name) const {
  for (int i = 0; i < model_tree_->topLevelItemCount(); ++i) {
    auto* root = model_tree_->topLevelItem(i);
    if (root && root->text(0) == name) {
      return root;
    }
  }
  return nullptr;
}

QTreeWidgetItem* MainWindow::find_child_by_param(QTreeWidgetItem* root,
                                                 const QString& key,
                                                 const QString& value) const {
  if (!root) {
    return nullptr;
  }
  for (int i = 0; i < root->childCount(); ++i) {
    auto* child = root->child(i);
    if (!child) {
      continue;
    }
    const QVariantMap params =
        child->data(0, PropertyEditor::kParamsRole).toMap();
    if (params.value(key).toString() == value) {
      return child;
    }
  }
  return nullptr;
}

QTreeWidgetItem* MainWindow::add_child_item(QTreeWidgetItem* root,
                                            const QString& name,
                                            const QString& kind,
                                            const QVariantMap& params) {
  if (!root) {
    return nullptr;
  }
  auto* item = new QTreeWidgetItem(root);
  item->setText(0, name);
  item->setData(0, PropertyEditor::kKindRole, kind);
  item->setData(0, PropertyEditor::kParamsRole, params);
  root->setExpanded(true);
  model_tree_->setCurrentItem(item);
  return item;
}

void MainWindow::upsert_mesh_item(const QString& path) {
  if (path.isEmpty()) {
    return;
  }
  auto* root = find_root_item("Mesh");
  if (!root) {
    return;
  }
  auto* item = find_child_by_param(root, "path", path);
  const QString base = QFileInfo(path).baseName();
  const QString name =
      base.isEmpty() ? QString("mesh_%1").arg(root->childCount() + 1) : base;
  QVariantMap params;
  params.insert("path", path);
  params.insert("source", "gmsh");
  if (!item) {
    add_child_item(root, name, "Mesh", params);
  } else {
    item->setText(0, name);
    item->setData(0, PropertyEditor::kParamsRole, params);
  }
}

void MainWindow::upsert_result_item(const QString& path,
                                    const QString& job_name) {
  if (path.isEmpty()) {
    return;
  }
  auto* root = find_root_item("Results");
  if (!root) {
    return;
  }
  auto* item = find_child_by_param(root, "path", path);
  const QString base = QFileInfo(path).baseName();
  const QString name =
      base.isEmpty() ? QString("result_%1").arg(root->childCount() + 1) : base;
  QVariantMap params;
  params.insert("path", path);
  if (!job_name.isEmpty()) {
    params.insert("job", job_name);
  }
  if (!item) {
    add_child_item(root, name, "Results", params);
  } else {
    item->setText(0, name);
    item->setData(0, PropertyEditor::kParamsRole, params);
  }
}

QString MainWindow::build_block_from_root(QTreeWidgetItem* root,
                                          const QString& block_name,
                                          const QString& default_type,
                                          const QStringList& skip_keys) const {
  if (!root || root->childCount() == 0) {
    return QString();
  }
  QString out;
  out += QString("[%1]\n").arg(block_name);
  for (int i = 0; i < root->childCount(); ++i) {
    auto* child = root->child(i);
    if (!child) {
      continue;
    }
    const QString name = child->text(0);
    out += QString("  [%1]\n").arg(name);
    const QVariantMap params =
        child->data(0, PropertyEditor::kParamsRole).toMap();
    QString type = params.value("type").toString();
    if (type.isEmpty()) {
      type = default_type;
    }
    if (!type.isEmpty()) {
      out += QString("    type = %1\n").arg(type);
    }
    for (auto it = params.begin(); it != params.end(); ++it) {
      if (it.key() == "type") {
        continue;
      }
      if (skip_keys.contains(it.key())) {
        continue;
      }
      out += QString("    %1 = %2\n")
                 .arg(it.key())
                 .arg(it.value().toString());
    }
    out += "  []\n";
  }
  out += "[]\n";
  return out;
}

QString MainWindow::build_variables_block(QTreeWidgetItem* root) const {
  if (!root || root->childCount() == 0) {
    return QString();
  }
  QString out;
  out += "[Variables]\n";
  for (int i = 0; i < root->childCount(); ++i) {
    auto* child = root->child(i);
    if (!child) {
      continue;
    }
    const QString name = child->text(0);
    out += QString("  [%1]\n").arg(name);
    const QVariantMap params =
        child->data(0, PropertyEditor::kParamsRole).toMap();
    const QString order = params.value("order", "FIRST").toString();
    const QString family = params.value("family", "LAGRANGE").toString();
    out += QString("    order = %1\n").arg(order);
    out += QString("    family = %1\n").arg(family);
    for (auto it = params.begin(); it != params.end(); ++it) {
      if (it.key() == "order" || it.key() == "family" ||
          it.key() == "type") {
        continue;
      }
      out += QString("    %1 = %2\n")
                 .arg(it.key())
                 .arg(it.value().toString());
    }
    out += "  []\n";
  }
  out += "[]\n";
  return out;
}

QString MainWindow::build_executioner_block(QTreeWidgetItem* root) const {
  if (!root || root->childCount() == 0) {
    return QString();
  }
  auto* step = root->child(0);
  if (!step) {
    return QString();
  }
  const QVariantMap params =
      step->data(0, PropertyEditor::kParamsRole).toMap();
  QString type = params.value("type").toString();
  if (type.isEmpty()) {
    type = "Transient";
  }
  QString out;
  out += "[Executioner]\n";
  out += QString("  type = %1\n").arg(type);
  for (auto it = params.begin(); it != params.end(); ++it) {
    if (it.key() == "type") {
      continue;
    }
    out += QString("  %1 = %2\n")
               .arg(it.key())
               .arg(it.value().toString());
  }
  out += "[]\n";
  if (root->childCount() > 1) {
    console_->appendPlainText(
        "Warning: multiple Steps found; using the first for [Executioner].");
  }
  return out;
}

void MainWindow::sync_model_to_input() {
  if (!moose_panel_) {
    return;
  }
  const QString functions =
      build_block_from_root(find_root_item("Functions"), "Functions",
                            "ParsedFunction", {});
  const QString variables = build_variables_block(find_root_item("Variables"));
  const QString materials =
      build_block_from_root(find_root_item("Materials"), "Materials",
                            "GenericConstantMaterial", {});
  const QString bcs = build_block_from_root(find_root_item("BC"), "BCs",
                                            "DirichletBC", {});
  const QString kernels =
      build_block_from_root(find_root_item("Loads"), "Kernels", "BodyForce",
                            {"section"});
  const QString outputs =
      build_block_from_root(find_root_item("Outputs"), "Outputs", "Exodus", {});
  const QString executioner =
      build_executioner_block(find_root_item("Steps"));
  moose_panel_->apply_model_blocks(functions, variables, materials, bcs, kernels,
                                   outputs, executioner);
  console_->appendPlainText("Model tree synced to MOOSE input.");
  statusBar()->showMessage("Model synced to MOOSE input.", 2000);
}

void MainWindow::load_demo_diffusion(bool run) {
  if (!moose_panel_) {
    return;
  }
  clear_model_tree_children();

  auto* functions = find_root_item("Functions");
  add_child_item(functions, "ic_u", "Functions",
                 {{"type", "ParsedFunction"},
                  {"expression", "sin(3.14159*x)*sin(3.14159*y)"}});
  add_child_item(functions, "ic_v", "Functions",
                 {{"type", "ParsedFunction"},
                  {"expression", "0.2*cos(3.14159*x)*cos(3.14159*y)"}});
  add_child_item(functions, "source_u", "Functions",
                 {{"type", "ParsedFunction"},
                  {"expression", "exp(-t)*sin(3.14159*x)*sin(3.14159*y)"}});
  add_child_item(functions, "source_v", "Functions",
                 {{"type", "ParsedFunction"},
                  {"expression", "0.1*exp(-0.5*t)*cos(3.14159*x)"}});
  add_child_item(functions, "bc_left", "Functions",
                 {{"type", "ParsedFunction"},
                  {"expression", "1.0+0.1*sin(6.28318*t)"}});
  add_child_item(functions, "bc_right", "Functions",
                 {{"type", "ParsedFunction"}, {"expression", "0.0"}});

  auto* variables = find_root_item("Variables");
  add_child_item(variables, "u", "Variables",
                 {{"order", "FIRST"}, {"family", "LAGRANGE"}});
  add_child_item(variables, "v", "Variables",
                 {{"order", "FIRST"}, {"family", "LAGRANGE"}});

  auto* materials = find_root_item("Materials");
  add_child_item(materials, "diffusion", "Materials",
                 {{"type", "GenericConstantMaterial"},
                  {"prop_names", "diff_u diff_v"},
                  {"prop_values", "1.0 0.25"}});

  auto* bcs = find_root_item("BC");
  add_child_item(bcs, "u_left", "BC",
                 {{"type", "FunctionDirichletBC"},
                  {"variable", "u"},
                  {"boundary", "left"},
                  {"function", "bc_left"}});
  add_child_item(bcs, "u_right", "BC",
                 {{"type", "FunctionDirichletBC"},
                  {"variable", "u"},
                  {"boundary", "right"},
                  {"function", "bc_right"}});
  add_child_item(bcs, "v_left", "BC",
                 {{"type", "DirichletBC"},
                  {"variable", "v"},
                  {"boundary", "left"},
                  {"value", "0"}});
  add_child_item(bcs, "v_right", "BC",
                 {{"type", "DirichletBC"},
                  {"variable", "v"},
                  {"boundary", "right"},
                  {"value", "0"}});

  auto* loads = find_root_item("Loads");
  add_child_item(loads, "u_dt", "Loads",
                 {{"type", "TimeDerivative"}, {"variable", "u"}});
  add_child_item(loads, "u_diff", "Loads",
                 {{"type", "MatDiffusion"},
                  {"variable", "u"},
                  {"diffusivity", "diff_u"}});
  add_child_item(loads, "u_src", "Loads",
                 {{"type", "BodyForce"}, {"variable", "u"},
                  {"function", "source_u"}});
  add_child_item(loads, "v_dt", "Loads",
                 {{"type", "TimeDerivative"}, {"variable", "v"}});
  add_child_item(loads, "v_diff", "Loads",
                 {{"type", "MatDiffusion"},
                  {"variable", "v"},
                  {"diffusivity", "diff_v"}});
  add_child_item(loads, "v_src", "Loads",
                 {{"type", "BodyForce"}, {"variable", "v"},
                  {"function", "source_v"}});

  auto* outputs = find_root_item("Outputs");
  add_child_item(outputs, "exodus", "Outputs",
                 {{"type", "Exodus"}, {"exodus", "true"}, {"csv", "true"}});

  auto* steps = find_root_item("Steps");
  add_child_item(steps, "transient", "Steps",
                 {{"type", "Transient"},
                  {"solve_type", "NEWTON"},
                  {"scheme", "bdf2"},
                  {"dt", "0.01"},
                  {"end_time", "0.2"}});

  moose_panel_->set_template_by_key("generated", true);
  sync_model_to_input();

  statusBar()->showMessage("Demo loaded: Transient Diffusion", 2000);
  console_->appendPlainText("Demo loaded: Transient Diffusion");
  if (run) {
    moose_panel_->run_job();
  }
}

void MainWindow::load_demo_thermo(bool run) {
  if (!moose_panel_) {
    return;
  }
  clear_model_tree_children();

  auto* functions = find_root_item("Functions");
  add_child_item(functions, "heat_src", "Functions",
                 {{"type", "ParsedFunction"},
                  {"expression",
                   "50.0*exp(-t)*sin(3.14159*x)*sin(3.14159*y)"}});

  auto* variables = find_root_item("Variables");
  add_child_item(variables, "T", "Variables",
                 {{"order", "FIRST"},
                  {"family", "LAGRANGE"},
                  {"initial_condition", "300"}});
  add_child_item(variables, "disp_x", "Variables",
                 {{"order", "FIRST"}, {"family", "LAGRANGE"}});
  add_child_item(variables, "disp_y", "Variables",
                 {{"order", "FIRST"}, {"family", "LAGRANGE"}});

  auto* materials = find_root_item("Materials");
  add_child_item(materials, "thcond", "Materials",
                 {{"type", "GenericConstantMaterial"},
                  {"prop_names", "thermal_conductivity"},
                  {"prop_values", "1.0"}});
  add_child_item(materials, "elastic", "Materials",
                 {{"type", "ComputeElasticityTensor"},
                  {"fill_method", "symmetric_isotropic"},
                  {"C_ijkl", "2.1e5 0.8e5"}});
  add_child_item(materials, "strain", "Materials",
                 {{"type", "ComputeSmallStrain"},
                  {"displacements", "disp_x disp_y"},
                  {"eigenstrain_names", "eigenstrain"}});
  add_child_item(materials, "stress", "Materials",
                 {{"type", "ComputeLinearElasticStress"}});
  add_child_item(materials, "thermal_strain", "Materials",
                 {{"type", "ComputeThermalExpansionEigenstrain"},
                  {"thermal_expansion_coeff", "1e-5"},
                  {"temperature", "T"},
                  {"stress_free_temperature", "300"},
                  {"eigenstrain_name", "eigenstrain"}});

  auto* bcs = find_root_item("BC");
  add_child_item(bcs, "temp_left", "BC",
                 {{"type", "DirichletBC"},
                  {"variable", "T"},
                  {"boundary", "left"},
                  {"value", "400"}});
  add_child_item(bcs, "temp_right", "BC",
                 {{"type", "DirichletBC"},
                  {"variable", "T"},
                  {"boundary", "right"},
                  {"value", "300"}});
  add_child_item(bcs, "fix_x", "BC",
                 {{"type", "DirichletBC"},
                  {"variable", "disp_x"},
                  {"boundary", "left"},
                  {"value", "0"}});
  add_child_item(bcs, "fix_y", "BC",
                 {{"type", "DirichletBC"},
                  {"variable", "disp_y"},
                  {"boundary", "bottom"},
                  {"value", "0"}});

  auto* loads = find_root_item("Loads");
  add_child_item(loads, "htcond", "Loads",
                 {{"type", "HeatConduction"}, {"variable", "T"}});
  add_child_item(loads, "TensorMechanics", "Loads",
                 {{"type", "TensorMechanics"},
                  {"displacements", "disp_x disp_y"}});
  add_child_item(loads, "Q_function", "Loads",
                 {{"type", "BodyForce"},
                  {"variable", "T"},
                  {"function", "heat_src"}});

  auto* outputs = find_root_item("Outputs");
  add_child_item(outputs, "exodus", "Outputs",
                 {{"type", "Exodus"}, {"exodus", "true"}, {"csv", "true"}});

  auto* steps = find_root_item("Steps");
  add_child_item(steps, "transient", "Steps",
                 {{"type", "Transient"},
                  {"scheme", "bdf2"},
                  {"dt", "0.05"},
                  {"end_time", "0.5"},
                  {"solve_type", "PJFNK"},
                  {"nl_max_its", "10"},
                  {"l_max_its", "30"},
                  {"nl_abs_tol", "1e-8"},
                  {"l_tol", "1e-4"}});

  moose_panel_->set_template_by_key("tm_generated", true);
  sync_model_to_input();

  statusBar()->showMessage("Demo loaded: Thermo-Mechanics", 2000);
  console_->appendPlainText("Demo loaded: Thermo-Mechanics");
  if (run) {
    moose_panel_->run_job();
  }
}

void MainWindow::load_demo_nonlinear_heat(bool run) {
  if (!moose_panel_) {
    return;
  }
  clear_model_tree_children();

  auto* variables = find_root_item("Variables");
  add_child_item(variables, "T", "Variables",
                 {{"order", "FIRST"},
                  {"family", "LAGRANGE"},
                  {"initial_condition", "300"}});

  auto* materials = find_root_item("Materials");
  add_child_item(materials, "k_T", "Materials",
                 {{"type", "ParsedMaterial"},
                  {"property_name", "thermal_conductivity"},
                  {"coupled_variables", "T"},
                  {"expression", "1 + 0.01*T"}});

  auto* bcs = find_root_item("BC");
  add_child_item(bcs, "temp_left", "BC",
                 {{"type", "DirichletBC"},
                  {"variable", "T"},
                  {"boundary", "left"},
                  {"value", "500"}});
  add_child_item(bcs, "temp_right", "BC",
                 {{"type", "DirichletBC"},
                  {"variable", "T"},
                  {"boundary", "right"},
                  {"value", "300"}});

  auto* loads = find_root_item("Loads");
  add_child_item(loads, "T_dt", "Loads",
                 {{"type", "TimeDerivative"}, {"variable", "T"}});
  add_child_item(loads, "T_cond", "Loads",
                 {{"type", "HeatConduction"}, {"variable", "T"}});

  auto* outputs = find_root_item("Outputs");
  add_child_item(outputs, "exodus", "Outputs",
                 {{"type", "Exodus"}, {"exodus", "true"}, {"csv", "true"}});

  auto* steps = find_root_item("Steps");
  add_child_item(steps, "transient", "Steps",
                 {{"type", "Transient"},
                  {"solve_type", "NEWTON"},
                  {"scheme", "bdf2"},
                  {"dt", "0.02"},
                  {"end_time", "0.5"}});

  moose_panel_->set_template_by_key("heat_generated", true);
  sync_model_to_input();

  statusBar()->showMessage("Demo loaded: Nonlinear Heat", 2000);
  console_->appendPlainText("Demo loaded: Nonlinear Heat");
  if (run) {
    moose_panel_->run_job();
  }
}

void MainWindow::add_item_under_root(QTreeWidgetItem* root) {
  if (!root) {
    return;
  }
  const QString kind = root->text(0);
  const QString base = kind.left(kind.size() - 1).toLower();
  const QString name = QInputDialog::getText(
      this, QString("Add %1").arg(kind), "Name:", QLineEdit::Normal,
      base + "_1");
  if (name.isEmpty()) {
    return;
  }
  auto* item = new QTreeWidgetItem(root);
  item->setText(0, name);
  item->setData(0, PropertyEditor::kKindRole, kind);
  item->setData(0, PropertyEditor::kParamsRole, QVariantMap());
  root->setExpanded(true);
  model_tree_->setCurrentItem(item);
}

void MainWindow::remove_item(QTreeWidgetItem* item) {
  if (!item || !item->parent()) {
    return;
  }
  auto* parent = item->parent();
  parent->removeChild(item);
  delete item;
}

void MainWindow::load_project(const QString& path) {
  try {
    YAML::Node root = YAML::LoadFile(path.toStdString());
    YAML::Node model = root["model"];
    if (!model || !model.IsMap()) {
      QMessageBox::warning(this, "Project Load",
                           "Invalid project file (missing model).");
      return;
    }
    clear_model_tree_children();
    for (const auto& it : model) {
      const QString kind = QString::fromStdString(it.first.as<std::string>());
      auto* root_item = find_root_item(kind);
      if (!root_item) {
        continue;
      }
      const YAML::Node list = it.second;
      if (!list.IsSequence()) {
        continue;
      }
      for (const auto& entry : list) {
        const QString name =
            QString::fromStdString(entry["name"].as<std::string>(""));
        if (name.isEmpty()) {
          continue;
        }
        auto* child = new QTreeWidgetItem(root_item);
        child->setText(0, name);
        child->setData(0, PropertyEditor::kKindRole, kind);
        QVariantMap params;
        const YAML::Node param_node = entry["params"];
        if (param_node && param_node.IsMap()) {
          for (const auto& p : param_node) {
            const QString key =
                QString::fromStdString(p.first.as<std::string>());
            const QString value =
                QString::fromStdString(p.second.as<std::string>());
            params.insert(key, value);
          }
        }
        child->setData(0, PropertyEditor::kParamsRole, params);
      }
    }
    project_path_ = path;
    console_->appendPlainText("Project loaded: " + path);
    YAML::Node gmsh_node = root["gmsh"];
    if (gmsh_node && gmsh_node.IsMap() && gmsh_panel_) {
      QVariantMap gmsh_settings;
      for (const auto& it : gmsh_node) {
        const QString key = QString::fromStdString(it.first.as<std::string>());
        const YAML::Node value = it.second;
        if (!value.IsScalar()) {
          continue;
        }
        const QString raw = QString::fromStdString(value.as<std::string>());
        const QString lower = raw.toLower();
        if (lower == "true" || lower == "false") {
          gmsh_settings.insert(key, lower == "true");
          continue;
        }
        bool ok_int = false;
        int int_val = raw.toInt(&ok_int);
        if (ok_int && !raw.contains('.')
            && !raw.contains('e', Qt::CaseInsensitive)) {
          gmsh_settings.insert(key, int_val);
          continue;
        }
        bool ok_double = false;
        double dbl_val = raw.toDouble(&ok_double);
        if (ok_double) {
          gmsh_settings.insert(key, dbl_val);
          continue;
        }
        gmsh_settings.insert(key, raw);
      }
      gmsh_panel_->apply_gmsh_settings(gmsh_settings);
    }
  } catch (const std::exception& e) {
    QMessageBox::warning(this, "Project Load",
                         QString("Failed to load: %1").arg(e.what()));
  }
}

bool MainWindow::save_project(const QString& path) {
  try {
    YAML::Node root;
    root["version"] = 1;
    YAML::Node model(YAML::NodeType::Map);
    for (int i = 0; i < model_tree_->topLevelItemCount(); ++i) {
      auto* root_item = model_tree_->topLevelItem(i);
      if (!root_item) {
        continue;
      }
      YAML::Node list(YAML::NodeType::Sequence);
      for (int j = 0; j < root_item->childCount(); ++j) {
        auto* child = root_item->child(j);
        if (!child) {
          continue;
        }
        YAML::Node entry;
        entry["name"] = child->text(0).toStdString();
        entry["kind"] = root_item->text(0).toStdString();
        YAML::Node params(YAML::NodeType::Map);
        const QVariantMap map =
            child->data(0, PropertyEditor::kParamsRole).toMap();
        for (auto it = map.begin(); it != map.end(); ++it) {
          params[it.key().toStdString()] =
              it.value().toString().toStdString();
        }
        entry["params"] = params;
        list.push_back(entry);
      }
      model[root_item->text(0).toStdString()] = list;
    }
    root["model"] = model;
    if (gmsh_panel_) {
      YAML::Node gmsh_node(YAML::NodeType::Map);
      const QVariantMap settings = gmsh_panel_->gmsh_settings();
      for (auto it = settings.begin(); it != settings.end(); ++it) {
        const QVariant& val = it.value();
        switch (val.typeId()) {
          case QMetaType::Bool:
            gmsh_node[it.key().toStdString()] = val.toBool();
            break;
          case QMetaType::Int:
            gmsh_node[it.key().toStdString()] = val.toInt();
            break;
          case QMetaType::Double:
            gmsh_node[it.key().toStdString()] = val.toDouble();
            break;
          default:
            gmsh_node[it.key().toStdString()] = val.toString().toStdString();
            break;
        }
      }
      root["gmsh"] = gmsh_node;
    }
    std::ofstream out(path.toStdString());
    out << root;
    out.close();
    return true;
  } catch (const std::exception& e) {
    QMessageBox::warning(this, "Project Save",
                         QString("Failed to save: %1").arg(e.what()));
  }
  return false;
}

}  // namespace gmp
