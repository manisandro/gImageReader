#ifndef UI_OUTPUTEDITORTEXT_HH
#define UI_OUTPUTEDITORTEXT_HH

#include "common.hh"
#include "OutputTextEdit.hh"
#include "SearchReplaceFrame.hh"
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidgetAction>

class UI_OutputEditorText {
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

	OutputTextEdit* plainTextEditOutput;
	SearchReplaceFrame* searchFrame;

	void setupUi(QWidget* widget) {
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

		searchFrame = new SearchReplaceFrame(widget);
		searchFrame->setVisible(false);
		widget->layout()->addWidget(searchFrame);

		plainTextEditOutput = new OutputTextEdit(widget);
		widget->layout()->addWidget(plainTextEditOutput);
	}
};

#endif // UI_OUTPUTEDITORTEXT_HH
