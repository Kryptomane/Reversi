#pragma once
// Hauptfenster: Mensch gegen PC (PC spielt Weiss) oder zwei Menschen an einem Geraet.
// Die Engine rechnet in einem Hintergrund-Thread, damit die Oberflaeche nie haengt;
// Zwischenstaende und Ergebnisse kommen per Qt-Ereignis zurueck.

#include <QWidget>
#include <thread>
#include "engine.h"
#include "game.h"

namespace Ui { class ReversiGui; }
class QCloseEvent;

class ReversiGui : public QWidget
{
	Q_OBJECT

public:
	explicit ReversiGui(QWidget* parent = nullptr);
	~ReversiGui() override;

protected:
	bool eventFilter(QObject* object, QEvent* e) override; // Android-Zurueck-Geste abfangen
	void closeEvent(QCloseEvent* e) override;

private:
	enum class Mode { VsComputer, TwoPlayers };
	enum class SearchKind { ComputerMove, Hint };

	static constexpr Color COMPUTER = Color::White;

	// Ablauf
	void startNewGame();
	void onFieldClicked(int x, int y);
	void handleTurn(Game::Turn turn);          // nach jedem Zug: Anzeige, Status, ggf. PC/Tipp starten

	// Engine im Hintergrund
	void startSearch(SearchKind kind);
	void onSearchProgress(const engine::Result& r, int forPosition, SearchKind kind);
	void onSearchResult(const engine::Result& r, int forPosition, SearchKind kind);
	void showHint(const engine::Result& r);
	void stopSearch();

	// Einstellungen
	void setupModeBox();
	void setupLevelBox();
	void onModeChanged();
	void onHintToggled(bool on);
	int level() const;

	// Anzeige
	void updateView();
	void updateHintLabel(const engine::Result* r);
	void showStatus(const QString& text);
	static QString colorName(Color c);
	QString yourTurnText() const;
	QString gameOverText() const;
	static QString provenResultText(const engine::Result& r);

	// Beenden
	void confirmQuit();
	void quit();

	bool isComputer(Color c) const { return mode == Mode::VsComputer && c == COMPUTER; }
	bool humanToMove() const { return !game.isOver() && !computerThinking && !isComputer(game.toMove()); }

	Ui::ReversiGui* ui;
	Game game;
	engine::Engine ai;
	std::thread searchThread;
	Mode mode = Mode::VsComputer;

	int positionId = 0;            // steigt bei jeder neuen Stellung -> veraltete Ergebnisse verwerfen
	bool computerThinking = false;
	bool hintRunning = false;
	int hintSquare = -1;           // Feldindex des Zugvorschlags, -1 = keiner
	QString passInfo;              // z. B. "Weiss muss passen. "
	QString lastResultInfo;        // z. B. "Der PC gewinnt bei bestem Spiel."
	bool quitDialogOpen = false;
	bool quitConfirmed = false;
};
