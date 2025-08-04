#pragma once

#include <QtCore/QVariant>
#include <QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QWidget>

#ifndef UI_ClickHelper
#define UI_ClickHelper

class ClickableLabel : public QLabel 
{
	Q_OBJECT

public:
	ClickableLabel(QWidget* parent = Q_NULLPTR, Qt::WindowFlags f = Qt::WindowFlags());
	~ClickableLabel();

signals:
    void clicked(QMouseEvent* event);

protected:
	void mousePressEvent(QMouseEvent* event);

};

#endif
