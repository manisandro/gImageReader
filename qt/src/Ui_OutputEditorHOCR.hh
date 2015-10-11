#ifndef UI_OUTPUTEDITORHOCR_HH
#define UI_OUTPUTEDITORHOCR_HH

#include "OutputTextEdit.hh"
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QMenu>
#include <QPushButton>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidgetAction>

class UI_OutputEditorHOCR
{
public:
	QAction* actionOutputClear;
	QAction* actionOutputRedo;
	QAction* actionOutputReplace;
	QAction* actionOutputSave;
	QAction* actionOutputUndo;
	QToolBar* toolBarOutput;

	QLineEdit *lineEditOutputSearch;
	QLineEdit *lineEditOutputReplace;
	QToolButton *toolButtonOutputFindNext;
	QToolButton *toolButtonOutputReplace;
	QToolButton *toolButtonOutputReplaceAll;
	QToolButton *toolButtonOutputFindPrev;
	QCheckBox *checkBoxOutputSearchMatchCase;
	OutputTextEdit *plainTextEditOutput;
	QFrame *frameOutputSearch;

	void setupUi(QWidget* widget)
	{
		widget->setLayout(new QVBoxLayout());
		widget->layout()->setContentsMargins(0, 0, 0, 0);
		widget->layout()->setSpacing(0);

		// Output toolbar
		actionOutputUndo = new QAction(QIcon::fromTheme("edit-undo"), gettext("Undo"), widget);
		actionOutputUndo->setToolTip(gettext("Undo"));
		actionOutputUndo->setEnabled(false);
		actionOutputRedo = new QAction(QIcon::fromTheme("edit-redo"), gettext("Redo"), widget);
		actionOutputRedo->setToolTip(gettext("Redo"));
		actionOutputRedo->setEnabled(false);
		actionOutputReplace = new QAction(QIcon::fromTheme("edit-find-replace"), gettext("Find and Replace"), widget);
		actionOutputReplace->setToolTip(gettext("Find and replace"));
		actionOutputReplace->setCheckable(true);
		actionOutputSave = new QAction(QIcon::fromTheme("document-save-as"), gettext("Save Output"), widget);
		actionOutputSave->setToolTip(gettext("Save output"));
		actionOutputClear = new QAction(QIcon::fromTheme("edit-clear"), gettext("Clear Output"), widget);
		actionOutputClear->setToolTip(gettext("Clear output"));

		toolBarOutput = new QToolBar(widget);
		toolBarOutput->setToolButtonStyle(Qt::ToolButtonIconOnly);
		toolBarOutput->setIconSize(QSize(1, 1) * toolBarOutput->style()->pixelMetric(QStyle::PM_SmallIconSize));
		toolBarOutput->addAction(actionOutputUndo);
		toolBarOutput->addAction(actionOutputRedo);
		toolBarOutput->addAction(actionOutputReplace);
		toolBarOutput->addAction(actionOutputSave);
		toolBarOutput->addAction(actionOutputClear);

		widget->layout()->addWidget(toolBarOutput);

		// Search/Replace field
		frameOutputSearch = new QFrame(widget);
		frameOutputSearch->setFrameShape(QFrame::StyledPanel);
		frameOutputSearch->setFrameShadow(QFrame::Plain);
		QGridLayout* frameOutputSearchLayout = new QGridLayout(frameOutputSearch);
		frameOutputSearchLayout->setSpacing(2);
		frameOutputSearchLayout->setContentsMargins(2, 2, 2, 2);

		lineEditOutputSearch = new QLineEdit(frameOutputSearch);
		lineEditOutputSearch->setPlaceholderText(gettext("Find"));
		frameOutputSearchLayout->addWidget(lineEditOutputSearch, 0, 0, 1, 1);

		lineEditOutputReplace = new QLineEdit(frameOutputSearch);
		lineEditOutputReplace->setPlaceholderText(gettext("Replace"));
		frameOutputSearchLayout->addWidget(lineEditOutputReplace, 1, 0, 1, 1);

		toolButtonOutputFindNext = new QToolButton(frameOutputSearch);
		toolButtonOutputFindNext->setIcon(QIcon::fromTheme("go-down"));
		toolButtonOutputFindNext->setToolTip(gettext("Find next"));
		frameOutputSearchLayout->addWidget(toolButtonOutputFindNext, 0, 1, 1, 1);

		toolButtonOutputFindPrev = new QToolButton(frameOutputSearch);
		toolButtonOutputFindPrev->setIcon(QIcon::fromTheme("go-up"));
		toolButtonOutputFindPrev->setToolTip(gettext("Find previous"));
		frameOutputSearchLayout->addWidget(toolButtonOutputFindPrev, 0, 2, 1, 1);

		toolButtonOutputReplace = new QToolButton(frameOutputSearch);
		toolButtonOutputReplace->setIcon(QIcon::fromTheme("edit-find-replace"));
		toolButtonOutputReplace->setToolTip(gettext("Replace"));
		frameOutputSearchLayout->addWidget(toolButtonOutputReplace, 1, 1, 1, 1);

		toolButtonOutputReplaceAll = new QToolButton(frameOutputSearch);
		toolButtonOutputReplaceAll->setIcon(QIcon::fromTheme("edit-find-replace"));
		toolButtonOutputReplaceAll->setToolTip(gettext("Replace all"));
		frameOutputSearchLayout->addWidget(toolButtonOutputReplaceAll, 1, 2, 1, 1);

		checkBoxOutputSearchMatchCase = new QCheckBox(frameOutputSearch);
		checkBoxOutputSearchMatchCase->setText(gettext("Match case"));
		frameOutputSearchLayout->addWidget(checkBoxOutputSearchMatchCase, 2, 0, 1, 3);

		widget->layout()->addWidget(frameOutputSearch);

		plainTextEditOutput = new OutputTextEdit(widget);
		widget->layout()->addWidget(plainTextEditOutput);
	}
};

#endif // UI_OUTPUTEDITORHOCR_HH
