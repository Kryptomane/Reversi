#include "reversigui.h"

ReversiGui::ReversiGui(QWidget *parent) :  QWidget(parent), ui(new Ui::ReversiGui), game(1), game2(0)
{
    ui->setupUi(this);
    connect(ui->exitGameButton, &QPushButton::clicked, this, &ReversiGui::terminate);
    connect(ui->boardlabel, &ClickableLabel::clicked, this, &ReversiGui::getMove); //klick ins spielbrett
	connect(ui->newGameButton, &QPushButton::clicked, this, &ReversiGui::startNewGame);
	//TODO: createNewFolder mit Time
	exportDirectory = "../Output/";
	numPCgamers = 2;
	actualColor = 1;
	init();
}

void ReversiGui::init()
{
	ColorWhite = cv::Scalar(255, 255, 255);
	ColorBlack = cv::Scalar(0, 0, 0);
	boardwidth = ui->boardlabel->width();
	boardheight = ui->boardlabel->height();
	makeNewLog();
	QObject::installEventFilter(this);
	testfile << "Spiel gestartet" << std::endl;
	updateBoard(false);

	game.mind = 8;
	game.eval.sfield = 8;
	game.eval.efield = 1000;
	game.eval.cfield = -8;
	game.eval.xfield = -50;
	game.eval.normal = 2;
	game.eval.weight = 1.5;
	game.eval.sieg = 100000;

	game.sayHello();

	training();
}

void ReversiGui::makeNewLog()
{
	logfile.flush();
	logfile.close();
	QString path = exportDirectory + "GUI.txt";
	logfile.open(path.toStdString());
	testfile.init(tl::TinyLog::INFO, &logfile);	
}

bool ReversiGui::eventFilter(QObject *object, QEvent *e)
{
	if (e->type() == QEvent::KeyPress) 
	{
		QKeyEvent* keyEvent = (QKeyEvent*)e;
		testfile << keyEvent->text().toStdString();
	}
	return QObject::eventFilter(object, e);
}

void ReversiGui::startNewGame()
{
	game.initBoard();
	game.findMove(game.mboard, 1);
	updateBoard(false);
}

void ReversiGui::training()
{
	srand(time(NULL));
	
	game2.mind = 1;
	game2.eval.sfield = 8;
	game2.eval.efield = 1000;
	game2.eval.cfield = -8;
	game2.eval.xfield = -50;
	game2.eval.normal = 3;
	game2.eval.weight = 0.5;
	game2.eval.sieg = 100000;
	int counter = 0;
	int siegfolge = 0;
	for(;;)
	{
		game.mind = 1;
		game.eval.sfield = rand() % 2000 + 1 - 1000;
		game.eval.efield = rand() % 2000 + 1 - 1000;
		game.eval.cfield = rand() % 2000 + 1 - 1000;
		game.eval.xfield = rand() % 2000 + 1 - 1000;
		game.eval.normal = rand() % 2000 + 1 - 1000;
		game.eval.weight = rand() % 1000 + 1 /(double)100;
		game.eval.sieg = rand() % 2000 + 1 - 1000;
		game.instance += 1;
		game.initBoard();
		game.sayHello();
		logic(field(2, 3));
		game.countChips(game.mboard);
		if (game.BlackChips > game.WhiteChips)
		{
			game2.eval = game.eval;
			game2.mind = game.mind;
			game2.sayHello();
			testfile << "getauscht" << std::endl;
			counter++;
			testfile << "siegfolge: " << siegfolge << std::endl;
			siegfolge = 0;
		}
		else
		{
			siegfolge++;
		}
		if (counter == 1000)
		{
			break;
		}
	}
}

void ReversiGui::getMove(QMouseEvent * e)
{
	int xx = e->x();
	int yy = e->y();
	int fieldWidth = boardwidth / ((double)game.bsize);
	int fieldHeight = boardheight / ((double)game.bsize);
	int xCoord, yCoord;
	for (int i=0; i<game.bsize; i++)
	{
		if (xx >= i*fieldWidth && xx < (i + 1)*fieldWidth)
			xCoord = i;
	}
	for (int j=0; j<game.bsize; j++)
	{
		if (yy >= j*fieldHeight && yy < (j + 1)*fieldHeight)
			yCoord = j;
	}
	if (game.mboard[xCoord][yCoord] == actualColor+2)
	{		
		logic(field(xCoord, yCoord));
	}
}

void ReversiGui::logic(field kandidat)
{
	bool gameend = false;
	bool half = false;
	int zug = 0;
	std::vector<field> moves;
	for (;;)
	{
		if (zug == 0)
		{
			if (numPCgamers == 0 || numPCgamers == 1) 
			{
				game.fastMove(game.mboard, kandidat, actualColor);
			}
			if (numPCgamers == 2)
			{
				game.silasindustries(game.mboard, 0, actualColor);
			}
			actualColor = 3 - actualColor;
			zug++;
		}
		else
		{
			if (half != true)
			{
				if (numPCgamers == 0)
				{
					break;
				}
				if (numPCgamers == 1 && zug > 1)
				{
					break;
				}
				if (numPCgamers == 1 && actualColor == 2)
				{
					game.silasindustries(game.mboard, 0, actualColor);
				}
				if (numPCgamers == 2)
				{
					if (actualColor == 2)
					{
						game2.silasindustries(game.mboard, 0, actualColor);
						
					}
					else
					{
						game.silasindustries(game.mboard, 0, actualColor);
					}
				}
				actualColor = 3 - actualColor;
				zug++;
			}
		}
		moves = game.fastfind(game.mboard, actualColor);
		updateBoard(false);
		if (moves.size() == 0)
		{
			actualColor = 3 - actualColor;
			if (half == true)
			{
				gameend = true;
				break;
			}
			else
			{
				half = true;
			}
		}
		else
		{
			if (half == true && actualColor == 1 && numPCgamers == 1)
			{
				break;  //pc kann nicht, aber mensch
			}
			if (half == true && actualColor == 2 && numPCgamers == 1)
			{
				half = false; // mensch kann nicht, pc nochmal
			}
			if (half == true && numPCgamers == 2)
			{
				half = false;
			}
		}
	}
	if (gameend)
	{
		updateBoard(true);
		game.countChips(game.mboard);
		if (game.WhiteChips > game.BlackChips)
		{
			testfile << "Weiß hat gewonnen" << std::endl;
			//return true;
		}
		else
		{
			testfile << "Schwarz hat gewonnen" << std::endl;
			//return false;
		}
	}
}

void ReversiGui::updateBoard(bool last)
{
	static int count = 0;
	std::ostringstream evala, evalb;
	evala << "Black: " << (int)game.mind << " " << game.eval.efield << " " << game.eval.xfield << " " << game.eval.cfield << " " << game.eval.normal << " " << game.eval.sfield << " " << game.eval.sieg << " " << game.eval.weight;
	evalb << "White: " << (int)game2.mind << " " << game2.eval.efield << " " << game2.eval.xfield << " " << game2.eval.cfield << " " << game2.eval.normal << " " << game2.eval.sfield << " " << game2.eval.sieg << " " << game2.eval.weight;
	cv::Mat qboard = game.paint(game.mboard, count, "update", evala.str(), evalb.str(), last);
	count++;
	QPixmap pixmap = QPixmap::fromImage(QImage((unsigned char*)qboard.data, qboard.cols, qboard.rows, QImage::Format_RGB888));
	ui->boardlabel->setPixmap(pixmap);
	game.countChips(game.mboard);
	ui->BlackChipsLabel->setText(QString::number(game.BlackChips));
	ui->WhiteChipsLabel->setText(QString::number(game.WhiteChips));
}

void ReversiGui::terminate()
{
	qApp->quit();
}

ReversiGui::~ReversiGui()
{
    delete ui;
}

/*
for (int imind = 1; imind < 2; imind++)
{
	for (int inormal = 1; inormal < 102; inormal += 20)
	{
		for (int ibad = 1; ibad < 502; ibad += 100)
		{
			for (int isuccess = 100; isuccess < 10200; isuccess += 2000)
			{
				for (int iweight = 1; iweight < 52; iweight += 10)
				{
					game.mind = imind;
					game.eval.sfield = 2;
					game.eval.efield = isuccess / (double)10;
					game.eval.cfield = -ibad;
					game.eval.xfield = -5 * ibad;
					game.eval.normal = inormal;
					game.eval.weight = iweight / (double)10;
					game.eval.sieg = isuccess;
					game.instance += 1;
					game.initBoard();
					logic(field(2, 3));
					game.countChips(game.mboard);
					if (game.BlackChips > game.WhiteChips)
					{
						game2.eval = game.eval;
						game2.mind = game.mind;
						game2.sayHello();
						testfile << "getauscht" << std::endl;
					}
				}
			}
		}
	}
}
*/
