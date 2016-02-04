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
	QAction* actionOutputClear;
	QToolButton* toolButtonOutputSave;
	QAction* actionOutputSaveHOCR;
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
		toolButtonOutputSave = new QToolButton(widget);
		toolButtonOutputSave->setIcon(QIcon::fromTheme("document-save-as"));
		toolButtonOutputSave->setText(gettext("Save Output"));
		QMenu* saveMenu = new QMenu(widget);
		toolButtonOutputSave->setToolTip(gettext("Save output"));
		toolButtonOutputSave->setMenu(saveMenu);
		toolButtonOutputSave->setPopupMode(QToolButton::InstantPopup);
		actionOutputSaveHOCR = saveMenu->addAction(gettext("HOCR Text"));
		actionOutputSavePDF = saveMenu->addAction(gettext("PDF"));
		actionOutputSavePDFTextOverlay = saveMenu->addAction(gettext("PDF with invisible text overlay"));
		actionOutputClear = new QAction(QIcon::fromTheme("edit-clear"), gettext("Clear Output"), widget);
		actionOutputClear->setToolTip(gettext("Clear output"));

		toolBarOutput = new QToolBar(widget);
		toolBarOutput->setToolButtonStyle(Qt::ToolButtonIconOnly);
		toolBarOutput->setIconSize(QSize(1, 1) * toolBarOutput->style()->pixelMetric(QStyle::PM_SmallIconSize));
		toolBarOutput->addWidget(toolButtonOutputSave);
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
