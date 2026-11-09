#pragma once
// Hauptfenster: Mensch gegen PC (PC spielt Weiss) oder zwei Menschen an einem Geraet.
// Die Engine rechnet in einem Hintergrund-Thread, damit die Oberflaeche nie haengt;
// Zwischenstaende und Ergebnisse kommen per Qt-Ereignis zurueck.

#include <QWidget>
#include <QElapsedTimer>
#include <thread>
#include "engine.h"
#include "game.h"

namespace Ui { class ReversiGui; }
class QCloseEvent;
class MessageOverlay;

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

	static constexpr int HINT_DELAY_MS = 1000;
	static constexpr int EVAL_EQUAL_BAND = 80;  // |Bewertung| darunter gilt als ausgeglichen ("=")
	static constexpr int EVAL_FULL_SCALE = 1500; // ab hier ist das Vergleichszeichen maximal gross (~4 Ecken)

	// Ablauf
	void requestNewGame();                     // laeuft eine Partie, erst nachfragen
	void startNewGame();
	void requestColorSwap();                   // Mensch uebernimmt die andere Farbe (nach Rueckfrage)
	void swapColors();
	void showGameOver();                       // vollflaechige Meldung: Glueckwunsch / Weiter ueben
	void onFieldClicked(int x, int y);
	void handleTurn(Game::Turn turn);          // nach jedem Zug: Anzeige, Status, ggf. PC/Tipp starten
	void undoMove();                           // gegen den PC: letzten PC-Zug und eigenen Zug zuruecknehmen

	// Engine im Hintergrund
	void startSearch(SearchKind kind);
	void onSearchProgress(const engine::Result& r, int forPosition, SearchKind kind);
	void onSearchResult(const engine::Result& r, int forPosition, SearchKind kind);
	void applyComputerMove(const engine::Result& r);
	Game::Turn playAndRemember(int square, bool showFrames); // setzt; merkt Zug + umgedrehte Steine fuer die Rahmen
	void offerHint(const engine::Result& r);   // erst nach HINT_DELAY_MS anzeigen (springt sonst stark)
	void showHint(const engine::Result& r);
	void stopSearch();

	// Stellungsbewertung (eigene kleine Engine im Hintergrund + Zwischenstaende der anderen Suchen)
	void startAnalysis();
	void stopAnalysis();
	void onEvaluation(const engine::Result& r, int forPosition, Color sideToMove);
	void showEvaluation(int blackScore, bool proven);
	void showFinalEvaluation();

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

	// Beenden
	void confirmQuit();
	void quit();
	bool gameInProgress() const { return game.canUndo() && !game.isOver(); }

	bool isComputer(Color c) const { return mode == Mode::VsComputer && c == computerColor; }
	bool humanToMove() const { return !game.isOver() && !computerThinking && !isComputer(game.toMove()); }

	Ui::ReversiGui* ui;
	MessageOverlay* overlay;
	Game game;
	engine::Engine ai;
	std::thread searchThread;
	engine::Engine analyzer{ 18 };  // nur fuer die Bewertungsanzeige
	std::thread analysisThread;
	int shownEvalDepth = -1;       // Tiefe der angezeigten Bewertung (tiefere ersetzen flachere)
	Mode mode = Mode::VsComputer;
	Color computerColor = Color::White;

	int positionId = 0;            // steigt bei jeder neuen Stellung -> veraltete Ergebnisse verwerfen
	bool computerThinking = false;
	bool hintRunning = false;
	int hintSquare = -1;           // Feldindex des Zugvorschlags, -1 = keiner
	QElapsedTimer hintClock;       // seit wann der Tipp rechnet
	engine::Result pendingHint;    // bester Vorschlag aus der ersten Sekunde
	bool hintPending = false;
	int framedMove = -1;           // roter Rahmen: Zug des PCs (bzw. im 2-Spieler-Modus der letzte Zug)
	bb::u64 framedFlips = 0;       // oranger Rahmen: dabei umgedrehte Steine
	bool animateNextUpdate = false;
	QString passInfo;              // z. B. "Weiss muss passen. "
	bool quitConfirmed = false;
};
