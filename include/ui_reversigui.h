#ifndef UI_REVERSIGUI_H
#define UI_REVERSIGUI_H

#pragma once

#include <QtCore/QVariant>
#include <QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QWidget>
#include "clickablehelper.h"

QT_BEGIN_NAMESPACE

class Ui_ReversiGui
{
public:
	ClickableLabel *boardlabel;
    QLabel *WhiteLabel;
    QLabel *BlackLabel;
    QPushButton *newGameButton;
    QPushButton *exitGameButton;
    QLabel *WhiteChipsLabel;
    QLabel *BlackChipsLabel;
	QLabel *MessageLabel;

    void setupUi(QWidget *ReversiGui)
    {
        if (ReversiGui->objectName().isEmpty())
            ReversiGui->setObjectName(QStringLiteral("ReversiGui"));
        ReversiGui->resize(430, 320);
        ReversiGui->setMinimumSize(QSize(430, 320));
        boardlabel = new ClickableLabel(ReversiGui);
        boardlabel->setObjectName(QStringLiteral("boardlabel"));
        boardlabel->setGeometry(QRect(10, 10, 300, 300));
        WhiteLabel = new QLabel(ReversiGui);
        WhiteLabel->setObjectName(QStringLiteral("WhiteLabel"));
        WhiteLabel->setGeometry(QRect(320, 10, 40, 15));
        BlackLabel = new QLabel(ReversiGui);
        BlackLabel->setObjectName(QStringLiteral("BlackLabel"));
        BlackLabel->setGeometry(QRect(320, 35, 40, 15));
        newGameButton = new QPushButton(ReversiGui);
        newGameButton->setObjectName(QStringLiteral("newGameButton"));
        newGameButton->setGeometry(QRect(320, 70, 100, 25));
        exitGameButton = new QPushButton(ReversiGui);
        exitGameButton->setObjectName(QStringLiteral("exitGameButton"));
        exitGameButton->setGeometry(QRect(320, 110, 100, 25));
        WhiteChipsLabel = new QLabel(ReversiGui);
        WhiteChipsLabel->setObjectName(QStringLiteral("WhiteChipsLabel"));
        WhiteChipsLabel->setGeometry(QRect(370, 10, 20, 15));
        BlackChipsLabel = new QLabel(ReversiGui);
        BlackChipsLabel->setObjectName(QStringLiteral("BlackChipsLabel"));
        BlackChipsLabel->setGeometry(QRect(370, 35, 20, 15));
		MessageLabel = new QLabel(ReversiGui);
		MessageLabel->setObjectName(QStringLiteral("MessageLabel"));
		MessageLabel->setGeometry(QRect(320, 150, 100, 25));
        retranslateUi(ReversiGui);

        QMetaObject::connectSlotsByName(ReversiGui);
    } // setupUi

    void retranslateUi(QWidget *ReversiGui)
    {
        ReversiGui->setWindowTitle(QApplication::translate("ReversiGui", "ReversiGui", nullptr));
        boardlabel->setText(QString());
        WhiteLabel->setText(QApplication::translate("ReversiGui", "Wei\303\237", nullptr));
        BlackLabel->setText(QApplication::translate("ReversiGui", "Schwarz", nullptr));
        newGameButton->setText(QApplication::translate("ReversiGui", "Neues Spiel", nullptr));
        exitGameButton->setText(QApplication::translate("ReversiGui", "Spiel beenden", nullptr));
        WhiteChipsLabel->setText(QApplication::translate("ReversiGui", "0", nullptr));
        BlackChipsLabel->setText(QApplication::translate("ReversiGui", "0", nullptr));
		MessageLabel->setText(QApplication::translate("ReversiGui", "", nullptr));
    } // retranslateUi

};

namespace Ui {
    class ReversiGui: public Ui_ReversiGui {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_REVERSIGUI_H
