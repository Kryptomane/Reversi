#ifndef UI_REVERSIGUI_H
#define UI_REVERSIGUI_H
#pragma once
// Oberflaeche mit Layouts (passt sich an Fenster- bzw. Handy-Bildschirm an):
// oben das Brett, darunter Steinzahl, Einstellungen, Status und Knoepfe.

#include <QtWidgets/QWidget>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QBoxLayout>
#include "boardwidget.h"

class Ui_ReversiGui
{
public:
	BoardWidget* board;
	QLabel* BlackChipsLabel;
	QLabel* WhiteChipsLabel;
	QLabel* timeLabel;
	QComboBox* timeBox;
	QCheckBox* hintBox;
	QLabel* MessageLabel;
	QPushButton* newGameButton;
	QPushButton* exitGameButton;

	void setupUi(QWidget* w)
	{
		w->setObjectName(QStringLiteral("ReversiGui"));
		w->setWindowTitle(QStringLiteral("Reversi"));

		QVBoxLayout* mainLayout = new QVBoxLayout(w);
		mainLayout->setContentsMargins(8, 8, 8, 8);
		mainLayout->setSpacing(6);

		board = new BoardWidget(w);
		mainLayout->addWidget(board, 1);

		QHBoxLayout* scoreRow = new QHBoxLayout();
		BlackChipsLabel = new QLabel(w);
		WhiteChipsLabel = new QLabel(w);
		QFont f = BlackChipsLabel->font();
		f.setBold(true);
		BlackChipsLabel->setFont(f);
		WhiteChipsLabel->setFont(f);
		scoreRow->addWidget(BlackChipsLabel);
		scoreRow->addStretch(1);
		scoreRow->addWidget(WhiteChipsLabel);
		mainLayout->addLayout(scoreRow);

		QHBoxLayout* timeRow = new QHBoxLayout();
		timeLabel = new QLabel(QStringLiteral("Bedenkzeit PC:"), w);
		timeBox = new QComboBox(w);
		timeRow->addWidget(timeLabel);
		timeRow->addWidget(timeBox);
		timeRow->addStretch(1);
		mainLayout->addLayout(timeRow);

		hintBox = new QCheckBox(QStringLiteral("Besten Zug zeigen"), w);
		mainLayout->addWidget(hintBox);

		MessageLabel = new QLabel(w);
		MessageLabel->setWordWrap(true);
		MessageLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
		MessageLabel->setMinimumHeight(MessageLabel->fontMetrics().lineSpacing() * 4); // Layout springt nicht
		mainLayout->addWidget(MessageLabel);

		QHBoxLayout* buttonRow = new QHBoxLayout();
		newGameButton = new QPushButton(QStringLiteral("Neues Spiel"), w);
		exitGameButton = new QPushButton(QStringLiteral("Beenden"), w);
		buttonRow->addWidget(newGameButton);
		buttonRow->addWidget(exitGameButton);
		mainLayout->addLayout(buttonRow);
	}
};

namespace Ui {
	class ReversiGui : public Ui_ReversiGui {};
}

#endif // UI_REVERSIGUI_H
