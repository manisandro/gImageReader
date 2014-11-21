#ifndef UI_MAINWINDOW_HH
#define UI_MAINWINDOW_HH

#include "ui_MainWindow.h"
#include <QDoubleSpinBox>
#include <QMenu>
#include <QWidgetAction>

class UI_MainWindow : public Ui_MainWindow
{
public:
	QAction* actionAbout;
	QAction* actionHelp;
	QAction* actionOutputModeAppend;
	QAction* actionOutputModeCursor;
	QAction* actionOutputModeReplace;
	QAction* actionOutputClear;
	QAction* actionOutputRedo;
	QAction* actionOutputReplace;
	QAction* actionOutputSave;
	QAction* actionOutputPostprocTitle1;
	QAction* actionOutputPostprocKeepDot;
	QAction* actionOutputPostprocKeepQuote;
	QAction* actionOutputPostprocTitle2;
	QAction* actionOutputPostprocJoinHyphen;
	QAction* actionOutputPostprocCollapseSpaces;
	QAction* actionOutputUndo;
	QAction* actionPreferences;
	QAction* actionRedetectLanguages;
	QAction* actionSourceClear;
	QAction* actionSourceDelete;
	QAction* actionSourcePaste;
	QAction* actionSourceRecent;
	QAction* actionSourceRemove;
	QAction* actionSourceScreenshot;
	QDoubleSpinBox* spinBoxRotation;
	QSpinBox* spinBoxPage;
	QFrame* frameRotation;
	QFrame* framePage;
	QLabel* labelRotation;
	QLabel* labelPage;
	QMenu* menuAppMenu;
	QMenu* menuAddSource;
	QMenu* menuLanguages;
	QMenu* menuOutputMode;
	QMenu* menuOutputPostproc;
	QToolBar* toolBarOutput;
	QToolBar* toolBarSources;
	QToolButton* toolButtonRecognize;
	QToolButton* toolButtonAppMenu;
	QToolButton* toolButtonOutputMode;
	QToolButton* toolButtonOutputPostproc;
	QToolButton* toolButtonSourceAdd;
	QWidgetAction* actionRotate;
	QWidgetAction* actionPage;


	void setupUi(QMainWindow* MainWindow)
	{
		Ui_MainWindow::setupUi(MainWindow);

		// Do remaining things which are not possible in designer
		toolBarMain->setContextMenuPolicy(Qt::PreventContextMenu);

		// Hide image controls widget
		widgetImageControls->setVisible(false);

		// Rotate spinbox
		frameRotation = new QFrame(MainWindow);
		frameRotation->setFrameShape(QFrame::StyledPanel);
		frameRotation->setFrameShadow(QFrame::Sunken);
		frameRotation->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

		QHBoxLayout* layoutRotation = new QHBoxLayout(frameRotation);
		layoutRotation->setContentsMargins(1, 1, 1, 1);
		layoutRotation->setSpacing(0);

		labelRotation = new QLabel(MainWindow);
		labelRotation->setPixmap(QPixmap(":/icons/angle"));
		layoutRotation->addWidget(labelRotation);

		spinBoxRotation = new QDoubleSpinBox(MainWindow);
		spinBoxRotation->setRange(0.0, 359.9);
		spinBoxRotation->setDecimals(1);
		spinBoxRotation->setSingleStep(0.1);
		spinBoxRotation->setWrapping(true);
		spinBoxRotation->setFrame(false);
		layoutRotation->addWidget(spinBoxRotation);

		actionRotate = new QWidgetAction(MainWindow);
		actionRotate->setDefaultWidget(frameRotation);

		toolBarMain->insertAction(actionImageControls, actionRotate);

		// Page spinbox
		framePage = new QFrame(MainWindow);
		framePage->setFrameShape(QFrame::StyledPanel);
		framePage->setFrameShadow(QFrame::Sunken);
		framePage->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

		QHBoxLayout* layoutPage = new QHBoxLayout(framePage);
		layoutPage->setContentsMargins(1, 1, 1, 1);
		layoutPage->setSpacing(0);

		labelPage = new QLabel(MainWindow);
		labelPage->setPixmap(QPixmap(":/icons/page"));
		layoutPage->addWidget(labelPage);

		spinBoxPage = new QSpinBox(MainWindow);
		spinBoxPage->setRange(1, 1);
		spinBoxPage->setFrame(false);
		layoutPage->addWidget(spinBoxPage);

		actionPage = new QWidgetAction(MainWindow);
		actionPage->setDefaultWidget(framePage);

		toolBarMain->insertAction(actionImageControls, actionPage);
		actionPage->setVisible(false);

		// Recognizer button
		toolButtonRecognize = new QToolButton(MainWindow);
		toolButtonRecognize->setIcon(QIcon::fromTheme("insert-text"));
		toolButtonRecognize->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
		QFont font;
		font.setPointSizeF(font.pointSizeF() * 0.9);
		toolButtonRecognize->setFont(font);
		toolButtonRecognize->setPopupMode(QToolButton::MenuButtonPopup);
		toolBarMain->insertWidget(actionToggleOutputPane, toolButtonRecognize);

		menuLanguages = new QMenu(toolButtonRecognize);
		toolButtonRecognize->setMenu(menuLanguages);

		// Spacer before app menu button
		QWidget* toolBarMainSpacer = new QWidget(toolBarMain);
		toolBarMainSpacer->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);
		toolBarMain->addWidget(toolBarMainSpacer);

		// App menu
		menuAppMenu = new QMenu(MainWindow);

		actionRedetectLanguages = new QAction(QIcon::fromTheme("view-refresh"), gettext("Redetect Languages"), MainWindow);
		menuAppMenu->addAction(actionRedetectLanguages);

		actionPreferences = new QAction(QIcon::fromTheme("configure"), gettext("Preferences"), MainWindow);
		menuAppMenu->addAction(actionPreferences);

		menuAppMenu->addSeparator();

		actionHelp = new QAction(QIcon::fromTheme("help-contents"), gettext("Help"), MainWindow);
		menuAppMenu->addAction(actionHelp);

		actionAbout = new QAction(QIcon::fromTheme("help-about"), gettext("About"), MainWindow);
		menuAppMenu->addAction(actionAbout);

		// App menu button
		toolButtonAppMenu = new QToolButton(MainWindow);
		toolButtonAppMenu->setIcon(QIcon::fromTheme("configure"));
		toolButtonAppMenu->setToolTip(gettext("Preferences"));
		toolButtonAppMenu->setPopupMode(QToolButton::InstantPopup);
		toolButtonAppMenu->setMenu(menuAppMenu);
		toolBarMain->addWidget(toolButtonAppMenu);

		// Sources toolbar
		actionSourceRecent = new QAction(QIcon::fromTheme("document-open-recent"), gettext("Recent"), MainWindow);
		actionSourcePaste = new QAction(QIcon::fromTheme("edit-paste"), gettext("Paste"), MainWindow);
		actionSourceScreenshot = new QAction(QIcon::fromTheme("camera-photo"), gettext("Take Screenshot"), MainWindow);

		menuAddSource = new QMenu(MainWindow);
		menuAddSource->addAction(actionSourceRecent);
		menuAddSource->addSeparator();
		menuAddSource->addAction(actionSourcePaste);
		menuAddSource->addAction(actionSourceScreenshot);

		toolButtonSourceAdd = new QToolButton(MainWindow);
		toolButtonSourceAdd->setIcon(QIcon::fromTheme("document-open"));
		toolButtonSourceAdd->setToolTip(gettext("Add images"));
		toolButtonSourceAdd->setPopupMode(QToolButton::MenuButtonPopup);
		toolButtonSourceAdd->setMenu(menuAddSource);

		actionSourceRemove = new QAction(QIcon::fromTheme("list-remove"), gettext("Remove Image"), MainWindow);
		actionSourceRemove->setToolTip(gettext("Remove image from list"));
		actionSourceRemove->setEnabled(false);
		actionSourceDelete = new QAction(QIcon::fromTheme("user-trash"), gettext("Delete Image"), MainWindow);
		actionSourceDelete->setToolTip(gettext("Delete image"));
		actionSourceDelete->setEnabled(false);
		actionSourceClear = new QAction(QIcon::fromTheme("edit-clear"), gettext("Clear List"), MainWindow);
		actionSourceClear->setToolTip(gettext("Clear list"));
		actionSourceClear->setEnabled(false);

		toolBarSources = new QToolBar(MainWindow);
		toolBarSources->setIconSize(QSize(1, 1) * toolBarSources->style()->pixelMetric(QStyle::PM_SmallIconSize));
		toolBarSources->addWidget(toolButtonSourceAdd);
		toolBarSources->addSeparator();
		toolBarSources->addAction(actionSourceRemove);
		toolBarSources->addAction(actionSourceDelete);
		toolBarSources->addAction(actionSourceClear);
		static_cast<QVBoxLayout*>(tabSources->layout())->insertWidget(0, toolBarSources);

		// Output insert mode
		actionOutputModeAppend = new QAction(QIcon(":/icons/ins_append"), gettext("Append to current text"), MainWindow);
		actionOutputModeCursor = new QAction(QIcon(":/icons/ins_cursor"), gettext("Insert at cursor"), MainWindow);
		actionOutputModeReplace = new QAction(QIcon(":/icons/ins_replace"), gettext("Replace current text"), MainWindow);

		menuOutputMode = new QMenu(MainWindow);
		menuOutputMode->addAction(actionOutputModeAppend);
		menuOutputMode->addAction(actionOutputModeCursor);
		menuOutputMode->addAction(actionOutputModeReplace);

		// Output postprocessing
		actionOutputPostprocTitle1 = new QAction(gettext("Keep line break if..."), MainWindow);
		actionOutputPostprocTitle1->setEnabled(false);
		actionOutputPostprocKeepDot = new QAction(gettext("Preceeded by dot"), MainWindow);
		actionOutputPostprocKeepDot->setCheckable(true);
		actionOutputPostprocKeepQuote = new QAction(gettext("Preceeded or succeeded by quote"), MainWindow);
		actionOutputPostprocKeepQuote->setCheckable(true);
		actionOutputPostprocTitle2 = new QAction(gettext("Other options"), MainWindow);
		actionOutputPostprocTitle2->setEnabled(false);
		actionOutputPostprocJoinHyphen = new QAction(gettext("Join hyphenated words"), MainWindow);
		actionOutputPostprocJoinHyphen->setCheckable(true);
		actionOutputPostprocCollapseSpaces = new QAction(gettext("Collapse whitespace"), MainWindow);
		actionOutputPostprocCollapseSpaces->setCheckable(true);

		menuOutputPostproc = new QMenu(MainWindow);
		menuOutputPostproc->addAction(actionOutputPostprocTitle1);
		menuOutputPostproc->addAction(actionOutputPostprocKeepDot);
		menuOutputPostproc->addAction(actionOutputPostprocKeepQuote);
		menuOutputPostproc->addAction(actionOutputPostprocTitle2);
		menuOutputPostproc->addAction(actionOutputPostprocJoinHyphen);
		menuOutputPostproc->addAction(actionOutputPostprocCollapseSpaces);

		// Output toolbar
		toolButtonOutputMode = new QToolButton(MainWindow);
		toolButtonOutputMode->setIcon(QIcon(":/icons/ins_append"));
		toolButtonOutputMode->setToolTip(gettext("Select insert mode"));
		toolButtonOutputMode->setPopupMode(QToolButton::InstantPopup);
		toolButtonOutputMode->setMenu(menuOutputMode);

		toolButtonOutputPostproc = new QToolButton(MainWindow);
		toolButtonOutputPostproc->setIcon(QIcon(":/icons/stripcrlf"));
		toolButtonOutputPostproc->setToolTip(gettext("Strip line breaks on selected text"));
		toolButtonOutputPostproc->setPopupMode(QToolButton::MenuButtonPopup);
		toolButtonOutputPostproc->setMenu(menuOutputPostproc);

		actionOutputReplace = new QAction(QIcon::fromTheme("edit-find-replace"), gettext("Find and Replace"), MainWindow);
		actionOutputReplace->setToolTip(gettext("Find and replace"));
		actionOutputReplace->setCheckable(true);
		actionOutputUndo = new QAction(QIcon::fromTheme("edit-undo"), gettext("Undo"), MainWindow);
		actionOutputUndo->setToolTip(gettext("Undo"));
		actionOutputUndo->setEnabled(false);
		actionOutputRedo = new QAction(QIcon::fromTheme("edit-redo"), gettext("Redo"), MainWindow);
		actionOutputRedo->setToolTip(gettext("Redo"));
		actionOutputRedo->setEnabled(false);
		actionOutputSave = new QAction(QIcon::fromTheme("document-save-as"), gettext("Save Output"), MainWindow);
		actionOutputSave->setToolTip(gettext("Save output"));
		actionOutputClear = new QAction(QIcon::fromTheme("edit-clear"), gettext("Clear Output"), MainWindow);
		actionOutputClear->setToolTip(gettext("Clear output"));

		toolBarOutput = new QToolBar(MainWindow);
		toolBarOutput->setIconSize(QSize(1, 1) * toolBarSources->style()->pixelMetric(QStyle::PM_SmallIconSize));
		toolBarOutput->addWidget(toolButtonOutputMode);
		toolBarOutput->addWidget(toolButtonOutputPostproc);
		toolBarOutput->addAction(actionOutputReplace);
		toolBarOutput->addAction(actionOutputUndo);
		toolBarOutput->addAction(actionOutputRedo);
		toolBarOutput->addAction(actionOutputSave);
		toolBarOutput->addAction(actionOutputClear);

		static_cast<QVBoxLayout*>(dockWidgetContentsOutput->layout()->layout())->insertWidget(0, toolBarOutput);
	}
};

#endif // UI_MAINWINDOW_HH
