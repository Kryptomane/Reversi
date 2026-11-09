#ifndef UI_REVERSIGUI_H
#define UI_REVERSIGUI_H
#pragma once
// Oberflaeche fuer Handy (Hochformat) und Desktop: oben das Brett, darunter Titel, Steinstand,
// Spielstaerke, Tipp-Schalter, Status und grosse Knoepfe. Alle Groessen haengen an der
// Schriftgroesse -> passen sich der Display-Dichte an (Qt rechnet in geraeteunabhaengigen Pixeln).

#include <QtWidgets/QWidget>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QBoxLayout>
#include <algorithm>
#include "boardwidget.h"

class Ui_ReversiGui
{
public:
	BoardWidget* board;
	QLabel* titleLabel;
	QLabel* BlackChipsLabel;
	QLabel* WhiteChipsLabel;
	QPushButton* swapButton;
	QLabel* evalLabel;
	QLabel* modeLabel;
	QComboBox* modeBox;
	QLabel* levelLabel;
	QComboBox* levelBox;
	QCheckBox* hintBox;
	QCheckBox* animateBox;
	QLabel* MessageLabel;
	QPushButton* newGameButton;
	QPushButton* undoButton;
	QPushButton* exitGameButton;

	void setupUi(QWidget* w)
	{
		w->setObjectName(QStringLiteral("ReversiGui"));
		const QString title = QStringLiteral("Reversi v" APP_VERSION " \u00A9 Silas Maile"); // APP_VERSION kommt aus CMake
		w->setWindowTitle(title);

		// Grundschrift etwas groesser als Standard, daraus alle Hoehen ableiten
		QFont base = w->font();
		base.setPointSizeF(base.pointSizeF() * 1.15);
		w->setFont(base);
		const int lh = QFontMetrics(base).height();   // eine Textzeile in logischen Pixeln
		const int touch = (int)(lh * 2.6);            // Mindesthoehe fuer Fingerbedienung (~48 dp)

		w->setStyleSheet(QStringLiteral(
			"QPushButton { background: #2e7d32; color: white; border: none; border-radius: 10px;"
			"  padding: 6px 14px; font-weight: bold; }"
			"QPushButton:pressed { background: #1b5e20; }"
			"QPushButton:disabled { background: #9e9e9e; color: #e0e0e0; }"
			"QPushButton#undoButton { background: #1565c0; }"
			"QPushButton#undoButton:pressed { background: #0d47a1; }"
			"QPushButton#undoButton:disabled { background: #9e9e9e; }"
			"QPushButton#swapButton { background: #1565c0; padding: 4px 10px; }"
			"QPushButton#swapButton:pressed { background: #0d47a1; }"
			"QPushButton#exitGameButton { background: #757575; }"
			"QPushButton#exitGameButton:pressed { background: #505050; }"
			"QLabel#BlackChipsLabel, QLabel#WhiteChipsLabel { border-radius: 8px; padding: 4px 10px; }"));

		QVBoxLayout* mainLayout = new QVBoxLayout(w);
		mainLayout->setContentsMargins(12, 12, 12, 12);
		mainLayout->setSpacing(10);

		board = new BoardWidget(w);
		mainLayout->addWidget(board, 1);

		titleLabel = new QLabel(title, w);
		titleLabel->setAlignment(Qt::AlignHCenter);
		QFont tf = base;
		tf.setPointSizeF(base.pointSizeF() * 0.8);
		tf.setItalic(true);
		titleLabel->setFont(tf);
		titleLabel->setStyleSheet(QStringLiteral("color: gray;"));
		mainLayout->addWidget(titleLabel);

		// Steinstand; wer am Zug ist, wird in updateBoard() hervorgehoben
		QHBoxLayout* scoreRow = new QHBoxLayout();
		BlackChipsLabel = new QLabel(w);
		WhiteChipsLabel = new QLabel(w);
		BlackChipsLabel->setObjectName(QStringLiteral("BlackChipsLabel"));
		WhiteChipsLabel->setObjectName(QStringLiteral("WhiteChipsLabel"));
		QFont sf = base;
		sf.setBold(true);
		sf.setPointSizeF(base.pointSizeF() * 1.15);
		for (QLabel* l : { BlackChipsLabel, WhiteChipsLabel })
		{
			l->setFont(sf);
			l->setMinimumHeight((int)(lh * 1.8));
			l->setAlignment(Qt::AlignCenter);
			l->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
		}
		swapButton = new QPushButton(QStringLiteral("Farbe\ntauschen"), w); // zweizeilig: passt neben den Steinstand
		swapButton->setObjectName(QStringLiteral("swapButton"));
		swapButton->setMinimumHeight((int)(lh * 1.8));
		scoreRow->addWidget(BlackChipsLabel, 1);
		scoreRow->addWidget(WhiteChipsLabel, 1);
		scoreRow->addWidget(swapButton);
		mainLayout->addLayout(scoreRow);

		// Bewertung der Stellung ("Gewinnchance: Schwarz > Weiss"), Zeichen waechst mit dem Vorteil
		evalLabel = new QLabel(w);
		evalLabel->setAlignment(Qt::AlignCenter);
		evalLabel->setTextFormat(Qt::RichText);
		evalLabel->setMinimumHeight((int)(lh * 2.6)); // Platz fuer das grosse Zeichen, Layout springt nicht
		mainLayout->addWidget(evalLabel);

		// Gegner (Computer oder zweiter Mensch) und Spielstaerke; Beschriftungen gleich breit
		QHBoxLayout* modeRow = new QHBoxLayout();
		modeLabel = new QLabel(QStringLiteral("Gegner:"), w);
		modeBox = new QComboBox(w);
		modeBox->setMinimumHeight((int)(lh * 2.2));
		modeBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		modeRow->addWidget(modeLabel);
		modeRow->addWidget(modeBox, 1);
		mainLayout->addLayout(modeRow);

		QHBoxLayout* levelRow = new QHBoxLayout();
		levelLabel = new QLabel(QStringLiteral("Spielst\u00E4rke:"), w);
		levelBox = new QComboBox(w);
		levelBox->setMinimumHeight((int)(lh * 2.2));
		levelBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		levelRow->addWidget(levelLabel);
		levelRow->addWidget(levelBox, 1);
		mainLayout->addLayout(levelRow);
		const int labelWidth = std::max(modeLabel->sizeHint().width(), levelLabel->sizeHint().width());
		modeLabel->setMinimumWidth(labelWidth);
		levelLabel->setMinimumWidth(labelWidth);

		QHBoxLayout* optionRow = new QHBoxLayout();
		hintBox = new QCheckBox(QStringLiteral("Besten Zug zeigen"), w);
		animateBox = new QCheckBox(QStringLiteral("Animation"), w);
		for (QCheckBox* c : { hintBox, animateBox })
			c->setMinimumHeight((int)(lh * 2.0));
		optionRow->addWidget(hintBox, 1);
		optionRow->addWidget(animateBox);
		mainLayout->addLayout(optionRow);

		MessageLabel = new QLabel(w);
		MessageLabel->setWordWrap(true);
		MessageLabel->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
		MessageLabel->setMinimumHeight(lh * 3); // Layout springt nicht, wenn der Text wechselt
		mainLayout->addWidget(MessageLabel);

		QHBoxLayout* buttonRow = new QHBoxLayout();
		buttonRow->setSpacing(12);
		newGameButton = new QPushButton(QStringLiteral("Neues Spiel"), w);
		undoButton = new QPushButton(QStringLiteral("Zug zur\u00FCck"), w);
		undoButton->setObjectName(QStringLiteral("undoButton"));
		exitGameButton = new QPushButton(QStringLiteral("Beenden"), w);
		exitGameButton->setObjectName(QStringLiteral("exitGameButton"));
		for (QPushButton* b : { newGameButton, undoButton, exitGameButton })
		{
			b->setMinimumHeight(touch);
			b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		}
		buttonRow->addWidget(newGameButton, 3);
		buttonRow->addWidget(undoButton, 3);
		buttonRow->addWidget(exitGameButton, 2);
		mainLayout->addLayout(buttonRow);
	}
};

namespace Ui {
	class ReversiGui : public Ui_ReversiGui {};
}

#endif // UI_REVERSIGUI_H
