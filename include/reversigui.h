#ifndef REVERSIGUI_H
#define REVERSIGUI_H

#include <QWidget>
#include <QEvent>
#include <QKeyEvent>
#include "commonincludes.h"
#include "calculation.h"
#include "ui_reversigui.h"
#include "engine.h"
#include <chrono>
#include <thread>
#include <QComboBox>
#include <QCheckBox>

namespace Ui {
class ReversiGui;
}

class ReversiGui : public QWidget
{
    Q_OBJECT

public:
    explicit ReversiGui(QWidget *parent = 0);
    ~ReversiGui();

public slots:
	void terminate();
	void startNewGame();
	void onFieldClicked(int x, int y); // Klick/Tipp auf ein Feld

private:
	bool eventFilter(QObject *object, QEvent *e);
	void init();
	void makeNewLog();
	void updateBoard(bool last);
	void logic(field kandidat);   // alte PC-gegen-PC-Schleife, nur noch fuer training()
	void training();              // Parameter-Training der alten Suche (silasindustries)
	void afterMove();             // Spieler wechseln / passen / Spielende, dann ggf. PC ziehen lassen
	bool isPC(int color) const;
	void showStatus(const QString& text);
	void startSearch(bool hint);                                      // Suche im Hintergrund-Thread (PC-Zug oder Tipp)
	void onSearchResult(const engine::Result& r, int posId, bool hint); // laeuft wieder im GUI-Thread
	void stopSearch();                                                // laufende Suche abbrechen und warten
	void onHintToggled(bool on);
	bool humanToMove() const;

	Ui::ReversiGui *ui;
	calc game;
	calc game2;
		
	int numPCgamers;   // 0 = Mensch gegen Mensch, 1 = Mensch gegen PC, 2 = PC gegen PC
	int actualColor;   // 1 = Schwarz, 2 = Weiss
	int humanColor;    // bei numPCgamers == 1
	bool busy;         // PC rechnet gerade
	bool gameOver;

	engine::Engine ai;
	engine::Settings aiSettings;
	std::thread searchThread;
	int posId = 0;           // wird bei jeder Stellungsaenderung erhoeht -> veraltete Ergebnisse verwerfen
	bool hintRunning = false;
	int hintSquare = -1;     // Feldindex x*8+y des empfohlenen Zugs, -1 = keiner
	QString lastPcInfo;
	QLabel* timeLabel = nullptr;
	QComboBox* timeBox = nullptr;
	QCheckBox* hintBox = nullptr;
	

	std::ofstream logfile;
	tl::TinyLog testfile;
	QString exportDirectory;
};

#endif // REVERSIGUI_H
