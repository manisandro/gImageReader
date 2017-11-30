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
	QAction* actionOutputOpen;
	QAction* actionOutputClear;
	QAction* actionOutputSaveHOCR;
	QAction* actionOutputExportPDF;
	QAction* actionToggleWConf;
	QAction* actionSyncPage;
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
		actionOutputExportPDF = new QAction(QIcon::fromTheme("application-pdf"), gettext("Export to PDF"), widget);
		actionOutputExportPDF->setToolTip(gettext("Export to PDF"));
		actionOutputExportPDF->setEnabled(false);
		actionOutputClear = new QAction(QIcon::fromTheme("edit-clear"), gettext("Clear output"), widget);
		actionOutputClear->setToolTip(gettext("Clear output"));
		actionToggleWConf = new QAction(QIcon(":/icons/wconf"), gettext("Show confidence values"), widget);
		actionToggleWConf->setToolTip(gettext("Show confidence values"));
		actionToggleWConf->setCheckable(true);
		actionSyncPage = new QAction(QIcon(":/icons/lock_page"), gettext("Syncronize with displayed page"), widget);
		actionSyncPage->setToolTip(gettext("Syncronize with displayed page"));
		actionSyncPage->setCheckable(true);
		actionPick = new QAction(QIcon(":/icons/pick"), gettext("Select item at position"), widget);
		actionPick->setToolTip(gettext("Select item at position"));
		actionPick->setCheckable(true);

		toolBarOutput = new QToolBar(widget);
		toolBarOutput->setToolButtonStyle(Qt::ToolButtonIconOnly);
		toolBarOutput->setIconSize(QSize(1, 1) * toolBarOutput->style()->pixelMetric(QStyle::PM_SmallIconSize));
		toolBarOutput->addAction(actionOutputOpen);
		toolBarOutput->addAction(actionOutputSaveHOCR);
		toolBarOutput->addAction(actionOutputExportPDF);
		toolBarOutput->addAction(actionOutputClear);
		toolBarOutput->addSeparator();
		toolBarOutput->addAction(actionToggleWConf);
		toolBarOutput->addAction(actionSyncPage);
		toolBarOutput->addAction(actionPick);

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
