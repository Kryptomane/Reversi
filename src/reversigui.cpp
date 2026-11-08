#include "reversigui.h"
#include <QTimer>
#include "apppaths.h"

ReversiGui::ReversiGui(QWidget *parent) :  QWidget(parent), ui(new Ui::ReversiGui), game(1), game2(0)
{
    ui->setupUi(this);
    connect(ui->exitGameButton, &QPushButton::clicked, this, &ReversiGui::terminate);
	connect(ui->board, &BoardWidget::fieldClicked, this, &ReversiGui::onFieldClicked); // Klick/Tipp ins Spielbrett
	connect(ui->newGameButton, &QPushButton::clicked, this, &ReversiGui::startNewGame);
	exportDirectory = logDirectory();
#ifndef Q_OS_ANDROID
	resize(440, 680); // Desktop: Hochformat wie am Handy, frei skalierbar
#endif
	numPCgamers = 1;   // Mensch (Schwarz) gegen PC (Weiss)
	humanColor = 1;
	actualColor = 1;
	busy = false;
	gameOver = false;
	aiSettings.timeMs = 3000;        // Bedenkzeit des PCs pro Zug
	aiSettings.endgameEmpties = 20;  // ab 20 freien Feldern rechnet der PC bis zum Ende
	init();
}

void ReversiGui::init()
{
	makeNewLog();
	QObject::installEventFilter(this);
	testfile << "Spiel gestartet" << std::endl;
	// Auswahl der Bedenkzeit (Widgets kommen aus dem Layout in ui_reversigui.h)
	timeLabel = ui->timeLabel;
	timeBox = ui->timeBox;
	for (int sec : { 1, 2, 3, 5, 10, 20, 30, 60 })
		timeBox->addItem(QString::number(sec) + " s", sec * 1000);
	timeBox->setCurrentIndex(timeBox->findData(aiSettings.timeMs));

	// Schalter: besten Zug fuer den menschlichen Spieler anzeigen
	hintBox = ui->hintBox;
	connect(hintBox, &QCheckBox::toggled, this, &ReversiGui::onHintToggled);


	game.mind = 8;
	game.eval.sfield = 8;
	game.eval.efield = 1000;
	game.eval.cfield = -8;
	game.eval.xfield = -50;
	game.eval.normal = 2;
	game.eval.weight = 1.5;
	game.eval.sieg = 100000;

	game.sayHello();

	// training(); // blockiert die GUI (1000 PC-gegen-PC-Partien) - nur bei Bedarf manuell aufrufen
	startNewGame();
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
	stopSearch();     // rechnet der PC noch an der alten Partie: abbrechen
	posId++;
	hintSquare = -1;
	game.initBoard(); // setzt auch die Zugmarker fuer Schwarz
	actualColor = 1;
	gameOver = false;
	busy = false;
	lastPcInfo.clear();
	ai.clear();
	updateBoard(false);
	if (isPC(actualColor))
	{
		bb::clearMarkers(game.mboard);
		startSearch(false);
	}
	else
	{
		showStatus(QStringLiteral("Du bist dran (Schwarz)."));
		if (hintBox && hintBox->isChecked())
			startSearch(true);
	}
}

bool ReversiGui::humanToMove() const
{
	return !gameOver && !busy && !isPC(actualColor);
}

void ReversiGui::onHintToggled(bool on)
{
	if (on)
	{
		if (humanToMove() && hintSquare < 0 && !hintRunning)
			startSearch(true);
	}
	else
	{
		if (hintRunning)
			stopSearch(); // nur die Tipp-Suche abbrechen, nie die PC-Suche
		hintSquare = -1;
		updateBoard(false);
	}
}

// Startet eine Suche fuer die aktuelle Stellung: PC-Zug (hint = false) oder Tipp fuer den Menschen.
void ReversiGui::startSearch(bool hint)
{
	if (searchThread.joinable())
	{
		ai.stop();            // falls noch eine (Tipp-)Suche laeuft
		searchThread.join();
	}
	engine::Settings s = aiSettings;
	const int selected = timeBox ? timeBox->currentData().toInt() : aiSettings.timeMs;
	if (hint)
	{
		hintRunning = true;
		s.timeMs = std::min(selected, 3000); // Tipp soll schnell da sein
	}
	else
	{
		busy = true;
		hintRunning = false;
		s.timeMs = selected;
		showStatus(QStringLiteral("PC denkt ..."));
	}
	const board pos = game.mboard;   // Kopie: der Thread fasst die GUI-Daten nicht an
	const int8_t color = (int8_t)actualColor;
	const int id = posId;
	ai.resetStop();
	searchThread = std::thread([this, pos, color, s, id, hint]() {
		const engine::Result r = engine::bestMove(ai, pos, color, s);
		// Ergebnis in den GUI-Thread schicken (Qt::QueuedConnection blockiert nicht)
		QMetaObject::invokeMethod(this, [this, r, id, hint]() { onSearchResult(r, id, hint); }, Qt::QueuedConnection);
	});
}

void ReversiGui::stopSearch()
{
	ai.stop();
	if (searchThread.joinable())
		searchThread.join();
	busy = false;
	hintRunning = false;
}

bool ReversiGui::isPC(int color) const
{
	return numPCgamers == 2 || (numPCgamers == 1 && color != humanColor);
}

void ReversiGui::showStatus(const QString& text)
{
	ui->MessageLabel->setText(text);
}

static QString colorName(int color)
{
	return color == 1 ? QStringLiteral("Schwarz") : QStringLiteral("Wei\u00DF");
}

// Nach jedem Zug: wer ist dran? Hat ein Spieler keinen Zug, setzt der andere nochmal.
void ReversiGui::afterMove()
{
	posId++;          // neue Stellung: alte Tipps/Ergebnisse sind ungueltig
	hintSquare = -1;
	bb::clearMarkers(game.mboard);
	bb::u64 P, O;
	const int next = 3 - actualColor;
	bb::load(game.mboard, next, P, O);
	QString msg;
	if (bb::moves(P, O))
		actualColor = next;
	else if (bb::moves(O, P))
		msg = colorName(next) + QStringLiteral(" muss passen. ");
	else
		gameOver = true;

	if (!gameOver && !isPC(actualColor))
		game.fastfind(game.mboard, actualColor); // Marker fuer den menschlichen Spieler
	updateBoard(gameOver);

	if (gameOver)
	{
		game.countChips(game.mboard);
		const int b = game.BlackChips, w = game.WhiteChips;
		if (b > w) msg += QStringLiteral("Schwarz gewinnt ");
		else if (w > b) msg += QStringLiteral("Wei\u00DF gewinnt ");
		else msg += QStringLiteral("Unentschieden ");
		msg += QString::number(b) + ":" + QString::number(w);
		testfile << "Spielende " << b << ":" << w << std::endl;
		showStatus(msg);
		return;
	}
	if (isPC(actualColor))
	{
		startSearch(false);
		if (!msg.isEmpty())
			showStatus(msg + QStringLiteral("PC denkt ..."));
	}
	else
	{
		showStatus(msg + QStringLiteral("Du bist dran (") + colorName(actualColor) + ")." + lastPcInfo);
		if (hintBox && hintBox->isChecked())
			startSearch(true);
	}
}

void ReversiGui::onSearchResult(const engine::Result& r, int id, bool hint)
{
	if (id != posId || gameOver)
		return; // Ergebnis fuer eine Stellung, die es nicht mehr gibt
	if (hint)
	{
		hintRunning = false;
		if (hintBox && hintBox->isChecked() && humanToMove() && !r.stopped)
		{
			hintSquare = r.move;
			updateBoard(false);
		}
		return;
	}
	if (r.move >= 0)
	{
		game.fastMove(game.mboard, field((int8_t)(r.move / 8), (int8_t)(r.move % 8)), (int8_t)actualColor);
		testfile << "PC " << (int)actualColor << " Zug " << r.move / 8 << "," << r.move % 8
			<< " Tiefe " << r.depth << " Wert " << r.score << (r.exact ? " exakt" : (r.wld ? " WLD" : ""))
			<< " Knoten " << r.nodes << " Zeit " << r.seconds << " Threads " << r.threads << std::endl;
		lastPcInfo = QStringLiteral("\n\nPC: Tiefe ") + QString::number(r.depth);
		if (r.depth == 0)
			lastPcInfo = QStringLiteral("\n\nPC: einziger Zug");
		else if (r.exact || r.wld)
		{
			if (r.score > 0) lastPcInfo += QStringLiteral("\nPC gewinnt sicher");
			else if (r.score < 0) lastPcInfo += QStringLiteral("\nPC verliert bei bestem Spiel");
			else lastPcInfo += QStringLiteral("\nRemis bei bestem Spiel");
		}
		else
			lastPcInfo += QStringLiteral("\nBewertung ") + QString::number(-r.score); // aus deiner Sicht
	}
	busy = false;
	afterMove();
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

void ReversiGui::onFieldClicked(int xCoord, int yCoord)
{
	if (busy || gameOver || isPC(actualColor))
		return;
	if (game.mboard[xCoord][yCoord] == actualColor+2)
	{
		if (hintRunning)
			stopSearch(); // Tipp wird nicht mehr gebraucht
		game.fastMove(game.mboard, field((int8_t)xCoord, (int8_t)yCoord), (int8_t)actualColor);
		afterMove();
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

void ReversiGui::updateBoard(bool /*last*/)
{
	ui->board->setBoard(game.mboard, hintSquare);
	game.countChips(game.mboard);
	ui->BlackChipsLabel->setText(QStringLiteral("Schwarz: ") + QString::number(game.BlackChips));
	ui->WhiteChipsLabel->setText(QStringLiteral("Wei\u00DF: ") + QString::number(game.WhiteChips));
}

void ReversiGui::terminate()
{
	qApp->quit();
}

ReversiGui::~ReversiGui()
{
	stopSearch();
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
