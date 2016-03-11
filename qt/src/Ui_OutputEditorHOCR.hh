#ifndef UI_OUTPUTEDITORHOCR_HH
#define UI_OUTPUTEDITORHOCR_HH

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

class UI_OutputEditorHOCR
{
public:
	QAction* actionOutputOpen;
	QAction* actionOutputClear;
	QAction* actionOutputSaveHOCR;
	QToolButton* toolButtonOutputExport;
	QAction* actionOutputSavePDFTextOverlay;
	QAction* actionOutputSavePDF;
	QToolBar* toolBarOutput;

	QSplitter* splitter;
	QTreeWidget *treeWidgetItems;
	QTableWidget *tableWidgetProperties;
	OutputTextEdit *plainTextEditOutput;

	void setupUi(QWidget* widget)
	{
		widget->setLayout(new QVBoxLayout());
		widget->layout()->setContentsMargins(0, 0, 0, 0);
		widget->layout()->setSpacing(0);

		// Output toolbar
		actionOutputOpen = new QAction(QIcon::fromTheme("document-open"), gettext("Open hOCR file"), widget);
		actionOutputOpen->setToolTip(gettext("Open hOCR file"));
		actionOutputSaveHOCR = new QAction(QIcon::fromTheme("document-save-as"), gettext("Save as hOCR text"), widget);
		actionOutputSaveHOCR->setToolTip(gettext("Save as hOCR text"));
		toolButtonOutputExport = new QToolButton(widget);
		toolButtonOutputExport->setIcon(QIcon::fromTheme("application-pdf"));
		toolButtonOutputExport->setText(gettext("Export to PDF"));
		QMenu* saveMenu = new QMenu(widget);
		toolButtonOutputExport->setToolTip(gettext("Export to PDF"));
		toolButtonOutputExport->setMenu(saveMenu);
		toolButtonOutputExport->setPopupMode(QToolButton::InstantPopup);
		actionOutputSavePDF = saveMenu->addAction(gettext("PDF"));
		actionOutputSavePDFTextOverlay = saveMenu->addAction(gettext("PDF with invisible text overlay"));
		actionOutputClear = new QAction(QIcon::fromTheme("edit-clear"), gettext("Clear output"), widget);
		actionOutputClear->setToolTip(gettext("Clear output"));

		toolBarOutput = new QToolBar(widget);
		toolBarOutput->setToolButtonStyle(Qt::ToolButtonIconOnly);
		toolBarOutput->setIconSize(QSize(1, 1) * toolBarOutput->style()->pixelMetric(QStyle::PM_SmallIconSize));
		toolBarOutput->addAction(actionOutputOpen);
		toolBarOutput->addAction(actionOutputSaveHOCR);
		toolBarOutput->addWidget(toolButtonOutputExport);
		toolBarOutput->addAction(actionOutputClear);

		widget->layout()->addWidget(toolBarOutput);

		splitter = new QSplitter(Qt::Vertical, widget);
		widget->layout()->addWidget(splitter);

		treeWidgetItems = new QTreeWidget(widget);
		treeWidgetItems->setHeaderHidden(true);
		splitter->addWidget(treeWidgetItems);

		QTabWidget* tabWidget = new QTabWidget(widget);

		tableWidgetProperties = new QTableWidget(widget);
		tableWidgetProperties->setColumnCount(2);
		tableWidgetProperties->horizontalHeader()->setVisible(false);
		tableWidgetProperties->verticalHeader()->setVisible(false);
		tableWidgetProperties->horizontalHeader()->setStretchLastSection(true);
		tableWidgetProperties->setEditTriggers(QAbstractItemView::NoEditTriggers);
		tabWidget->addTab(tableWidgetProperties, gettext("Properties"));

		plainTextEditOutput = new OutputTextEdit(widget);
		plainTextEditOutput->setReadOnly(true);
		tabWidget->addTab(plainTextEditOutput, gettext("Source"));

		splitter->addWidget(tabWidget);
	}
};

#endif // UI_OUTPUTEDITORHOCR_HH
