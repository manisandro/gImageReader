#ifndef UI_OUTPUTEDITORHOCR_HH
#define UI_OUTPUTEDITORHOCR_HH

#include "common.hh"
#include "OutputTextEdit.hh"
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QHeaderView>
#include <QMenu>
#include <QPushButton>
#include <QToolBar>
#include <QToolButton>
#include <QSplitter>
#include <QTableWidget>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidgetAction>

class UI_OutputEditorHOCR {
public:
	QToolButton* toolButtonOutputExport;
	QAction* actionOutputOpen;
	QAction* actionOutputClear;
	QAction* actionOutputSaveHOCR;
	QAction* actionOutputExportText;
	QAction* actionOutputExportPDF;
	QAction* actionToggleWConf;
	QAction* actionPick;
	QToolBar* toolBarOutput;
	QTabWidget* tabWidget;

	QSplitter* splitter;
	QTreeView *treeViewHOCR;
	QTableWidget *tableWidgetProperties;
	OutputTextEdit *plainTextEditOutput;

	void setupUi(QWidget* widget) {
		widget->setLayout(new QVBoxLayout());
		widget->layout()->setContentsMargins(0, 0, 0, 0);
		widget->layout()->setSpacing(0);

		// Output toolbar
		actionOutputOpen = new QAction(QIcon::fromTheme("document-open"), gettext("Open hOCR file"), widget);
		actionOutputOpen->setToolTip(gettext("Open hOCR file"));
		actionOutputSaveHOCR = new QAction(QIcon::fromTheme("document-save-as"), gettext("Save as hOCR text"), widget);
		actionOutputSaveHOCR->setToolTip(gettext("Save as hOCR text"));
		actionOutputSaveHOCR->setEnabled(false);
		QMenu* exportMenu = new QMenu();
		actionOutputExportText = new QAction(QIcon::fromTheme("text-plain"), gettext("Export to plain text"), widget);
		actionOutputExportText->setToolTip(gettext("Export to plain text"));
		exportMenu->addAction(actionOutputExportText);
		actionOutputExportPDF = new QAction(QIcon::fromTheme("application-pdf"), gettext("Export to PDF"), widget);
		actionOutputExportPDF->setToolTip(gettext("Export to PDF"));
		exportMenu->addAction(actionOutputExportPDF);
		toolButtonOutputExport = new QToolButton(widget);
		toolButtonOutputExport->setIcon(QIcon::fromTheme("document-export"));
		toolButtonOutputExport->setText(gettext("Export"));
		toolButtonOutputExport->setToolTip(gettext("Export"));
		toolButtonOutputExport->setEnabled(false);
		toolButtonOutputExport->setMenu(exportMenu);
		toolButtonOutputExport->setPopupMode(QToolButton::InstantPopup);
		actionOutputClear = new QAction(QIcon::fromTheme("edit-clear"), gettext("Clear output"), widget);
		actionOutputClear->setToolTip(gettext("Clear output"));
		actionToggleWConf = new QAction(QIcon(":/icons/wconf"), gettext("Show confidence values"), widget);
		actionToggleWConf->setToolTip(gettext("Show confidence values"));
		actionToggleWConf->setCheckable(true);

		toolBarOutput = new QToolBar(widget);
		toolBarOutput->setToolButtonStyle(Qt::ToolButtonIconOnly);
		toolBarOutput->setIconSize(QSize(1, 1) * toolBarOutput->style()->pixelMetric(QStyle::PM_SmallIconSize));
		toolBarOutput->addAction(actionOutputOpen);
		toolBarOutput->addAction(actionOutputSaveHOCR);
		toolBarOutput->addWidget(toolButtonOutputExport);
		toolBarOutput->addAction(actionOutputClear);
		toolBarOutput->addSeparator();
		toolBarOutput->addAction(actionToggleWConf);

		widget->layout()->addWidget(toolBarOutput);

		splitter = new QSplitter(Qt::Vertical, widget);
		widget->layout()->addWidget(splitter);

		treeViewHOCR = new QTreeView(widget);
		treeViewHOCR->setHeaderHidden(true);
		treeViewHOCR->setSelectionMode(QTreeWidget::ExtendedSelection);
		splitter->addWidget(treeViewHOCR);

		tabWidget = new QTabWidget(widget);

		tableWidgetProperties = new QTableWidget(widget);
		tableWidgetProperties->setColumnCount(2);
		tableWidgetProperties->horizontalHeader()->setVisible(false);
		tableWidgetProperties->verticalHeader()->setVisible(false);
		tableWidgetProperties->horizontalHeader()->setStretchLastSection(true);
		tabWidget->addTab(tableWidgetProperties, gettext("Properties"));

		plainTextEditOutput = new OutputTextEdit(widget);
		plainTextEditOutput->setReadOnly(true);
		tabWidget->addTab(plainTextEditOutput, gettext("Source"));

		splitter->addWidget(tabWidget);
	}
};

#endif // UI_OUTPUTEDITORHOCR_HH
