#ifndef REVERSIGUI_H
#define REVERSIGUI_H

#include <QWidget>
#include <QEvent>
#include <QKeyEvent>
#include "commonincludes.h"
#include "calculation.h"
#include "ui_reversigui.h"
#include "clickablehelper.h"
#include <chrono>
#include <thread>

namespace Ui {
class ReversiGui;
}

class ReversiGui : public QWidget
{
    Q_OBJECT

public:
    explicit ReversiGui(QWidget *parent = 0);
    ~ReversiGui();
	int boardwidth;
	int boardheight;

public slots:
	void terminate();
	void startNewGame();
	void getMove(QMouseEvent * e);

private:
	bool eventFilter(QObject *object, QEvent *e);
	void init();
	void makeNewLog();
	void updateBoard(bool last);
	void logic(field kandidat);
	void training();

	cv::Scalar ColorWhite;
	cv::Scalar ColorBlack;
	Ui::ReversiGui *ui;
	calc game;
	calc game2;
		
	int numPCgamers;
	int actualColor;
	

	std::ofstream logfile;
	tl::TinyLog testfile;
	QString exportDirectory;
};

#endif // REVERSIGUI_H
