#ifndef UI_MAINWINDOW_HH
#define UI_MAINWINDOW_HH

#include "ui_MainWindow.h"
#include <QDoubleSpinBox>
#include <QMenu>
#include <QWidgetAction>

class UI_MainWindow : public Ui_MainWindow {
public:
	QAction* actionAbout;
	QAction* actionHelp;
	QAction* actionPreferences;
	QAction* actionRedetectLanguages;
	QAction* actionSourceClear;
	QAction* actionSourceDelete;
	QAction* actionSourcePaste;
	QAction* actionSourceRecent;
	QAction* actionSourceRemove;
	QAction* actionSourceScreenshot;
	QComboBox* comboBoxOCRMode;
	QDoubleSpinBox* spinBoxRotation;
	QSpinBox* spinBoxPage;
	QFrame* frameRotation;
	QFrame* framePage;
	QLabel* labelRotation;
	QLabel* labelPage;
	QMenu* menuAppMenu;
	QMenu* menuAddSource;
	QMenu* menuLanguages;
	QToolBar* toolBarSources;
	QToolButton* toolButtonRecognize;
	QToolButton* toolButtonAppMenu;
	QToolButton* toolButtonSourceAdd;
	QWidgetAction* actionRotate;
	QWidgetAction* actionPage;


	void setupUi(QMainWindow* MainWindow) {
		Ui_MainWindow::setupUi(MainWindow);

		// Do remaining things which are not possible in designer
		toolBarMain->setContextMenuPolicy(Qt::PreventContextMenu);

		// Remove & from some labels which designer insists in adding
		dockWidgetSources->setWindowTitle(gettext("Sources"));
		dockWidgetOutput->setWindowTitle(gettext("Output"));

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
		spinBoxRotation->setKeyboardTracking(false);
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
		spinBoxPage->setKeyboardTracking(false);
		layoutPage->addWidget(spinBoxPage);

		actionPage = new QWidgetAction(MainWindow);
		actionPage->setDefaultWidget(framePage);

		toolBarMain->insertAction(actionImageControls, actionPage);
		actionPage->setVisible(false);

		QFont smallFont;
		smallFont.setPointSizeF(smallFont.pointSizeF() * 0.9);

		// OCR mode button
		QWidget* ocrModeWidget = new QWidget();
		ocrModeWidget->setLayout(new QVBoxLayout());
		ocrModeWidget->layout()->setContentsMargins(0, 0, 0, 0);
		ocrModeWidget->layout()->setSpacing(0);
		QLabel* outputModeLabel = new QLabel(gettext("OCR mode:"));
		outputModeLabel->setFont(smallFont);
		ocrModeWidget->layout()->addWidget(outputModeLabel);
		comboBoxOCRMode = new QComboBox();
		comboBoxOCRMode->addItems(QStringList() << gettext("Plain text") << gettext("hOCR, PDF"));
		comboBoxOCRMode->setFont(smallFont);
		comboBoxOCRMode->setFrame(false);
		comboBoxOCRMode->setCurrentIndex(-1);
		ocrModeWidget->layout()->addWidget(comboBoxOCRMode);
		toolBarMain->insertWidget(actionAutodetectLayout, ocrModeWidget);

		actionAutodetectLayout->setVisible(false);

		// Recognize button
		toolButtonRecognize = new QToolButton(MainWindow);
		toolButtonRecognize->setIcon(QIcon::fromTheme("insert-text"));
		toolButtonRecognize->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
		toolButtonRecognize->setFont(smallFont);
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

		actionPreferences = new QAction(QIcon::fromTheme("preferences-system"), gettext("Preferences"), MainWindow);
		menuAppMenu->addAction(actionPreferences);

		menuAppMenu->addSeparator();

		actionHelp = new QAction(QIcon::fromTheme("help-contents"), gettext("Help"), MainWindow);
		menuAppMenu->addAction(actionHelp);

		actionAbout = new QAction(QIcon::fromTheme("help-about"), gettext("About"), MainWindow);
		menuAppMenu->addAction(actionAbout);

		// App menu button
		toolButtonAppMenu = new QToolButton(MainWindow);
		toolButtonAppMenu->setIcon(QIcon::fromTheme("preferences-system"));
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
		toolButtonSourceAdd->setText(gettext("Add Images"));
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
		toolBarSources->setToolButtonStyle(Qt::ToolButtonIconOnly);
		toolBarSources->setIconSize(QSize(1, 1) * toolBarSources->style()->pixelMetric(QStyle::PM_SmallIconSize));
		toolBarSources->addWidget(toolButtonSourceAdd);
		toolBarSources->addSeparator();
		toolBarSources->addAction(actionSourceRemove);
		toolBarSources->addAction(actionSourceDelete);
		toolBarSources->addAction(actionSourceClear);
		static_cast<QVBoxLayout*>(tabSources->layout())->insertWidget(0, toolBarSources);
	}
};

#endif // UI_MAINWINDOW_HH
