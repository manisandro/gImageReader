#ifndef UI_OUTPUTEDITORTEXT_HH
#define UI_OUTPUTEDITORTEXT_HH

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

class UI_OutputEditorText
{
public:
	QAction* actionOutputModeAppend;
	QAction* actionOutputModeCursor;
	QAction* actionOutputModeReplace;
	QAction* actionOutputClear;
	QAction* actionOutputRedo;
	QAction* actionOutputReplace;
	QAction* actionOutputSave;
	QAction* actionOutputPostprocTitle1;
	QAction* actionOutputPostprocKeepEndMark;
	QAction* actionOutputPostprocKeepQuote;
	QAction* actionOutputPostprocTitle2;
	QAction* actionOutputPostprocJoinHyphen;
	QAction* actionOutputPostprocCollapseSpaces;
	QAction* actionOutputPostprocKeepParagraphs;
	QAction* actionOutputPostprocTitle3;
	QAction* actionOutputPostprocDrawWhitespace;
	QAction* actionOutputUndo;
	QMenu* menuOutputMode;
	QMenu* menuOutputPostproc;
	QToolBar* toolBarOutput;
	QToolButton* toolButtonOutputMode;
	QToolButton* toolButtonOutputPostproc;

	QLineEdit *lineEditOutputSearch;
	QLineEdit *lineEditOutputReplace;
	QToolButton *toolButtonOutputFindNext;
	QToolButton *toolButtonOutputReplace;
	QToolButton *toolButtonOutputReplaceAll;
	QToolButton *toolButtonOutputFindPrev;
	QPushButton *pushButtonOutputReplacementList;
	QCheckBox *checkBoxOutputSearchMatchCase;
	OutputTextEdit *plainTextEditOutput;
	QFrame *frameOutputSearch;

	void setupUi(QWidget* widget)
	{
		widget->setLayout(new QVBoxLayout());
		widget->layout()->setContentsMargins(0, 0, 0, 0);
		widget->layout()->setSpacing(0);

		// Output insert mode
		actionOutputModeAppend = new QAction(QIcon(":/icons/ins_append"), gettext("Append to current text"), widget);
		actionOutputModeCursor = new QAction(QIcon(":/icons/ins_cursor"), gettext("Insert at cursor"), widget);
		actionOutputModeReplace = new QAction(QIcon(":/icons/ins_replace"), gettext("Replace current text"), widget);

		menuOutputMode = new QMenu(widget);
		menuOutputMode->addAction(actionOutputModeAppend);
		menuOutputMode->addAction(actionOutputModeCursor);
		menuOutputMode->addAction(actionOutputModeReplace);

		// Output postprocessing
		actionOutputPostprocTitle1 = new QAction(gettext("Keep line break if..."), widget);
		actionOutputPostprocTitle1->setEnabled(false);
		actionOutputPostprocKeepEndMark = new QAction(gettext("Preceded by end mark (.?!)"), widget);
		actionOutputPostprocKeepEndMark->setCheckable(true);
		actionOutputPostprocKeepQuote = new QAction(gettext("Preceded or succeeded by quote"), widget);
		actionOutputPostprocKeepQuote->setCheckable(true);
		actionOutputPostprocTitle2 = new QAction(gettext("Other options"), widget);
		actionOutputPostprocTitle2->setEnabled(false);
		actionOutputPostprocJoinHyphen = new QAction(gettext("Join hyphenated words"), widget);
		actionOutputPostprocJoinHyphen->setCheckable(true);
		actionOutputPostprocCollapseSpaces = new QAction(gettext("Collapse whitespace"), widget);
		actionOutputPostprocCollapseSpaces->setCheckable(true);
		actionOutputPostprocKeepParagraphs = new QAction(gettext("Preserve paragraphs"), widget);
		actionOutputPostprocKeepParagraphs->setCheckable(true);
		actionOutputPostprocTitle3 = new QAction(gettext("Visual aids"), widget);
		actionOutputPostprocTitle3->setEnabled(false);
		actionOutputPostprocDrawWhitespace = new QAction(gettext("Draw whitespace"), widget);
		actionOutputPostprocDrawWhitespace->setCheckable(true);

		menuOutputPostproc = new QMenu(widget);
		menuOutputPostproc->addAction(actionOutputPostprocTitle1);
		menuOutputPostproc->addAction(actionOutputPostprocKeepEndMark);
		menuOutputPostproc->addAction(actionOutputPostprocKeepQuote);
		menuOutputPostproc->addAction(actionOutputPostprocTitle2);
		menuOutputPostproc->addAction(actionOutputPostprocJoinHyphen);
		menuOutputPostproc->addAction(actionOutputPostprocCollapseSpaces);
		menuOutputPostproc->addAction(actionOutputPostprocKeepParagraphs);
		menuOutputPostproc->addAction(actionOutputPostprocTitle3);
		menuOutputPostproc->addAction(actionOutputPostprocDrawWhitespace);

		// Output toolbar
		toolButtonOutputMode = new QToolButton(widget);
		toolButtonOutputMode->setIcon(QIcon(":/icons/ins_append"));
		toolButtonOutputMode->setToolTip(gettext("Select insert mode"));
		toolButtonOutputMode->setPopupMode(QToolButton::InstantPopup);
		toolButtonOutputMode->setMenu(menuOutputMode);

		toolButtonOutputPostproc = new QToolButton(widget);
		toolButtonOutputPostproc->setIcon(QIcon(":/icons/stripcrlf"));
		toolButtonOutputPostproc->setText(gettext("Strip Line Breaks"));
		toolButtonOutputPostproc->setToolTip(gettext("Strip line breaks on selected text"));
		toolButtonOutputPostproc->setPopupMode(QToolButton::MenuButtonPopup);
		toolButtonOutputPostproc->setMenu(menuOutputPostproc);

		actionOutputReplace = new QAction(QIcon::fromTheme("edit-find-replace"), gettext("Find and Replace"), widget);
		actionOutputReplace->setToolTip(gettext("Find and replace"));
		actionOutputReplace->setCheckable(true);
		actionOutputUndo = new QAction(QIcon::fromTheme("edit-undo"), gettext("Undo"), widget);
		actionOutputUndo->setToolTip(gettext("Undo"));
		actionOutputUndo->setEnabled(false);
		actionOutputRedo = new QAction(QIcon::fromTheme("edit-redo"), gettext("Redo"), widget);
		actionOutputRedo->setToolTip(gettext("Redo"));
		actionOutputRedo->setEnabled(false);
		actionOutputSave = new QAction(QIcon::fromTheme("document-save-as"), gettext("Save Output"), widget);
		actionOutputSave->setToolTip(gettext("Save output"));
		actionOutputClear = new QAction(QIcon::fromTheme("edit-clear"), gettext("Clear Output"), widget);
		actionOutputClear->setToolTip(gettext("Clear output"));

		toolBarOutput = new QToolBar(widget);
		toolBarOutput->setToolButtonStyle(Qt::ToolButtonIconOnly);
		toolBarOutput->setIconSize(QSize(1, 1) * toolBarOutput->style()->pixelMetric(QStyle::PM_SmallIconSize));
		toolBarOutput->addWidget(toolButtonOutputMode);
		toolBarOutput->addWidget(toolButtonOutputPostproc);
		toolBarOutput->addAction(actionOutputReplace);
		toolBarOutput->addAction(actionOutputUndo);
		toolBarOutput->addAction(actionOutputRedo);
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

		pushButtonOutputReplacementList = new QPushButton(frameOutputSearch);
		pushButtonOutputReplacementList->setText(gettext("Substitutions"));
		pushButtonOutputReplacementList->setIcon(QIcon::fromTheme("document-edit"));
		frameOutputSearchLayout->addWidget(pushButtonOutputReplacementList, 3, 0, 1, 3);

		checkBoxOutputSearchMatchCase = new QCheckBox(frameOutputSearch);
		checkBoxOutputSearchMatchCase->setText(gettext("Match case"));
		frameOutputSearchLayout->addWidget(checkBoxOutputSearchMatchCase, 2, 0, 1, 3);

		widget->layout()->addWidget(frameOutputSearch);

		plainTextEditOutput = new OutputTextEdit(widget);
		widget->layout()->addWidget(plainTextEditOutput);
	}
};

#endif // UI_OUTPUTEDITORTEXT_HH
