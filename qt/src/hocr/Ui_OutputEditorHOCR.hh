#ifndef UI_OUTPUTEDITORHOCR_HH
#define UI_OUTPUTEDITORHOCR_HH

#include "common.hh"
#include "OutputTextEdit.hh"
#include "SearchReplaceFrame.hh"
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
	QMenu* menuInsertMode;
	QMenu* menuOpen;
	QToolButton* toolButtonInsertMode;
	QToolButton* toolButtonOutputExport;
	QToolButton* toolButtonOpen;
	QAction* actionInsertModeAppend;
	QAction* actionInsertModeBefore;
	QAction* actionOpenAppend;
	QAction* actionOpenInsertBefore;
	QAction* actionOutputClear;
	QAction* actionOutputSaveHOCR;
	QAction* actionOutputExportText;
	QAction* actionOutputExportPDF;
	QAction* actionOutputExportODT;
	QAction* actionOutputReplace;
	QAction* actionToggleWConf;
	QAction* actionPreview;
	QAction* actionProofread;
	QAction* actionNavigateNext;
	QAction* actionNavigatePrev;
	QAction* actionExpandAll;
	QAction* actionCollapseAll;
	QComboBox* comboBoxNavigate;

	QToolBar* toolBarOutput;
	QToolBar* toolBarNavigate;
	QTabWidget* tabWidgetProps;

	QSplitter* splitter;
	QTreeView* treeViewHOCR;
	QTableWidget* tableWidgetProperties;
	OutputTextEdit* plainTextEditOutput;
	SearchReplaceFrame* searchFrame;

	void setupUi(QWidget* widget) {
		widget->setLayout(new QVBoxLayout());
		widget->layout()->setContentsMargins(0, 0, 0, 0);
		widget->layout()->setSpacing(0);

		// Output toolbar
		actionInsertModeAppend = new QAction(QIcon(":/icons/ins_hocr_append"), gettext("Append new output after last page"), widget);
		actionInsertModeBefore = new QAction(QIcon(":/icons/ins_hocr_before"), gettext("Insert new output before current page"), widget);

		menuInsertMode = new QMenu(widget);
		menuInsertMode->addAction(actionInsertModeAppend);
		menuInsertMode->addAction(actionInsertModeBefore);

		toolButtonInsertMode = new QToolButton(widget);
		toolButtonInsertMode->setIcon(QIcon(":/icons/ins_hocr_append"));
		toolButtonInsertMode->setToolTip(gettext("Select insert mode"));
		toolButtonInsertMode->setPopupMode(QToolButton::InstantPopup);
		toolButtonInsertMode->setMenu(menuInsertMode);

		actionOpenAppend = new QAction(gettext("Append document after last page"), widget);
		actionOpenInsertBefore = new QAction(gettext("Insert document before current page"), widget);

		menuOpen = new QMenu(widget);
		menuOpen->addAction(actionOpenAppend);
		menuOpen->addAction(actionOpenInsertBefore);

		toolButtonOpen = new QToolButton(widget);
		toolButtonOpen->setIcon(QIcon::fromTheme("document-open"));
		toolButtonOpen->setText(gettext("Open hOCR file"));
		toolButtonOpen->setToolTip(gettext("Open hOCR file"));
		toolButtonOpen->setPopupMode(QToolButton::MenuButtonPopup);
		toolButtonOpen->setMenu(menuOpen);

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
		actionOutputExportODT = new QAction(QIcon::fromTheme("x-office-document"), gettext("Export to ODT"), widget);
		actionOutputExportODT->setToolTip(gettext("Export to ODT"));
		exportMenu->addAction(actionOutputExportODT);
		toolButtonOutputExport = new QToolButton(widget);
		toolButtonOutputExport->setIcon(QIcon::fromTheme("document-export"));
		toolButtonOutputExport->setText(gettext("Export"));
		toolButtonOutputExport->setToolTip(gettext("Export"));
		toolButtonOutputExport->setEnabled(false);
		toolButtonOutputExport->setMenu(exportMenu);
		toolButtonOutputExport->setPopupMode(QToolButton::InstantPopup);
		actionOutputClear = new QAction(QIcon::fromTheme("edit-clear"), gettext("Clear output"), widget);
		actionOutputClear->setToolTip(gettext("Clear output"));
		actionOutputReplace = new QAction(QIcon::fromTheme("edit-find-replace"), gettext("Find and Replace"), widget);
		actionOutputReplace->setToolTip(gettext("Find and replace"));
		actionOutputReplace->setCheckable(true);
		actionToggleWConf = new QAction(QIcon(":/icons/wconf"), gettext("Show confidence values"), widget);
		actionToggleWConf->setToolTip(gettext("Show confidence values"));
		actionToggleWConf->setCheckable(true);
		actionProofread = new QAction(QIcon(":/icons/proofread"), gettext("Show proofread widget"), widget);
		actionProofread->setToolTip(gettext("Show proofread widget"));
		actionProofread->setCheckable(true);
		actionPreview = new QAction(QIcon::fromTheme("document-preview"), gettext("Show preview"), widget);
		actionPreview->setToolTip(gettext("Show preview"));
		actionPreview->setCheckable(true);

		toolBarOutput = new QToolBar(widget);
		toolBarOutput->setToolButtonStyle(Qt::ToolButtonIconOnly);
		toolBarOutput->setIconSize(QSize(1, 1) * toolBarOutput->style()->pixelMetric(QStyle::PM_SmallIconSize));
		toolBarOutput->addWidget(toolButtonInsertMode);
		toolBarOutput->addSeparator();
		toolBarOutput->addWidget(toolButtonOpen);
		toolBarOutput->addAction(actionOutputSaveHOCR);
		toolBarOutput->addWidget(toolButtonOutputExport);
		toolBarOutput->addAction(actionOutputClear);
		toolBarOutput->addSeparator();
		toolBarOutput->addAction(actionOutputReplace);
		toolBarOutput->addAction(actionToggleWConf);
		toolBarOutput->addAction(actionProofread);
		toolBarOutput->addAction(actionPreview);

		widget->layout()->addWidget(toolBarOutput);

		searchFrame = new SearchReplaceFrame(widget);
		searchFrame->setVisible(false);
		widget->layout()->addWidget(searchFrame);

		splitter = new QSplitter(Qt::Vertical, widget);
		widget->layout()->addWidget(splitter);

		QWidget* treeContainer = new QWidget(widget);
		treeContainer->setLayout(new QVBoxLayout());
		treeContainer->layout()->setSpacing(0);
		treeContainer->layout()->setContentsMargins(0, 0, 0, 0);
		splitter->addWidget(treeContainer);

		treeViewHOCR = new QTreeView(widget);
		treeViewHOCR->setHeaderHidden(true);
		treeViewHOCR->setSelectionMode(QTreeWidget::ExtendedSelection);
		treeContainer->layout()->addWidget(treeViewHOCR);

		actionNavigateNext = new QAction(QIcon::fromTheme("go-down"), gettext("Next (F3)"), widget);
		actionNavigatePrev = new QAction(QIcon::fromTheme("go-up"), gettext("Previous (Shift+F3)"), widget);
		comboBoxNavigate = new QComboBox();
		comboBoxNavigate->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLength);
		comboBoxNavigate->setMinimumContentsLength(10);
		actionExpandAll = new QAction(QIcon(":/icons/expand"), gettext("Expand all"), widget);
		actionCollapseAll = new QAction(QIcon(":/icons/collapse"), gettext("Collapse all"), widget);

		toolBarNavigate = new QToolBar(widget);
		toolBarNavigate->setToolButtonStyle(Qt::ToolButtonIconOnly);
		toolBarNavigate->setIconSize(QSize(1, 1) * toolBarOutput->style()->pixelMetric(QStyle::PM_SmallIconSize));
		toolBarNavigate->addWidget(comboBoxNavigate);
		toolBarNavigate->addSeparator();
		toolBarNavigate->addAction(actionNavigateNext);
		toolBarNavigate->addAction(actionNavigatePrev);
		toolBarNavigate->addSeparator();
		toolBarNavigate->addAction(actionExpandAll);
		toolBarNavigate->addAction(actionCollapseAll);
		treeContainer->layout()->addWidget(toolBarNavigate);

		tabWidgetProps = new QTabWidget(widget);

		tableWidgetProperties = new QTableWidget(widget);
		tableWidgetProperties->setColumnCount(2);
		tableWidgetProperties->horizontalHeader()->setVisible(false);
		tableWidgetProperties->verticalHeader()->setVisible(false);
		tableWidgetProperties->horizontalHeader()->setStretchLastSection(true);
		tabWidgetProps->addTab(tableWidgetProperties, gettext("Properties"));

		plainTextEditOutput = new OutputTextEdit(widget);
		plainTextEditOutput->setReadOnly(true);
		tabWidgetProps->addTab(plainTextEditOutput, gettext("Source"));

		splitter->addWidget(tabWidgetProps);
	}
};

#endif // UI_OUTPUTEDITORHOCR_HH
