#pragma once
// Hauptfenster: Mensch gegen PC (PC spielt Weiss), zwei Menschen an einem Geraet oder
// zwei Menschen an zwei Geraeten ueber Bluetooth.
// Die Engine rechnet in einem Hintergrund-Thread, damit die Oberflaeche nie haengt;
// Zwischenstaende und Ergebnisse kommen per Qt-Ereignis zurueck.

#include <QWidget>
#include <QElapsedTimer>
#include <thread>
#include "engine.h"
#include "game.h"

namespace Ui { class ReversiGui; }
class QCloseEvent;
class QLabel;
class MessageOverlay;
class BluetoothLink;
class BluetoothLobby;

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
	enum class Mode { VsComputer, TwoPlayers, Bluetooth };
	enum class SearchKind { ComputerMove, Hint };

	static constexpr int HINT_DELAY_MS = 1000;
	static constexpr int EVAL_EQUAL_BAND = 80;  // |Bewertung| darunter gilt als ausgeglichen ("=")
	static constexpr int EVAL_FULL_SCALE = 1500; // ab hier ist das Vergleichszeichen maximal gross (~4 Ecken)
	static constexpr int STRONG_MOVE_GAIN = 150; // so viel muss sich die Gewinnchance des Ziehenden verbessern
	static constexpr int STRONG_MOVE_MIN_DEPTH = 4; // flachere Bewertungen sind zu ungenau
	static constexpr int STRONG_MOVE_SHOW_MS = 2500;

	// Ablauf
	void requestNewGame();                     // Dialog: Startstellung tauschen, Weiss beginnt
	void startNewGame();                       // mit den Startoptionen aus den Einstellungen
	void startGame(const Game::StartOptions& start);
	static Game::StartOptions savedStartOptions();
	void requestColorSwap();                   // Mensch uebernimmt die andere Farbe (nach Rueckfrage)
	void swapColors();
	void showGameOver();                       // vollflaechige Meldung: Glueckwunsch / Weiter ueben
	void onFieldClicked(int x, int y);
	void handleTurn(Game::Turn turn);          // nach jedem Zug: Anzeige, Status, ggf. PC/Tipp starten
	void undoMove();                           // gegen den PC: letzten PC-Zug und eigenen Zug zuruecknehmen
	void undoUntil(Color who);                 // zuruecknehmen, bis wieder `who` am Zug ist (mind. ein Zug)

	// Bluetooth (zwei Geraete): Protokoll in Textzeilen, siehe onBluetoothLine
	void openBluetoothLobby();
	void onBluetoothConnected();
	void onBluetoothLine(const QString& line);
	void onBluetoothLost();
	void leaveBluetooth(bool notifyPeer);      // Partie geht danach zu zweit an diesem Geraet weiter
	void startBluetoothGame(const Game::StartOptions& start, bool swapColors);
	void showNotice(const QString& title, const QString& detail = QString());
	bool remoteToMove() const { return mode == Mode::Bluetooth && !game.isOver() && game.toMove() != myColor; }
	int moveStamp() const { return game.count(Color::Black) + game.count(Color::White); } // steigt mit jedem Zug
	QString peerName() const;

	// Engine im Hintergrund
	void startSearch(SearchKind kind);
	void onSearchProgress(const engine::Result& r, int forPosition, SearchKind kind);
	void onSearchResult(const engine::Result& r, int forPosition, SearchKind kind);
	void applyComputerMove(const engine::Result& r);
	Game::Turn playAndRemember(int square, bool showFrames); // setzt; merkt Zug + umgedrehte Steine fuer die Ringe
	void clearFrames();
	void offerHint(const engine::Result& r);   // erst nach HINT_DELAY_MS anzeigen (springt sonst stark)
	void showHint(const engine::Result& r);
	void stopSearch();

	// Stellungsbewertung (eigene kleine Engine im Hintergrund + Zwischenstaende der anderen Suchen)
	void startAnalysis();
	void stopAnalysis();
	void onEvaluation(const engine::Result& r, int forPosition, Color sideToMove);
	void showEvaluation(int blackScore, bool proven);
	void showFinalEvaluation();
	void onAnalysisDone(const engine::Result& r, int forPosition, Color sideToMove);

	// "Starker Zug": Gewinnchance des Ziehenden vor und nach dem Zug vergleichen
	void rememberEvalBeforeMove(Color mover);
	void checkStrongMove();
	void showStrongMove(Color mover);
	static int clampedScore(int score, bool proven);

	// Einstellungen
	void setupModeBox();
	void setupLevelBox();
	void onModeChanged();
	void setMode(Mode m);
	void selectModeBox(Mode m);                // Auswahl anzeigen, ohne onModeChanged auszuloesen
	void onHintToggled(bool on);
	void applyModeUi();                        // Spielstaerke/Tipp je nach Modus freigeben
	bool hintAllowed() const;                  // angehakt und nicht ueber Bluetooth (dort waere es Schummeln)
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
	bool humanToMove() const
	{
		return !game.isOver() && !computerThinking && !isComputer(game.toMove()) && !remoteToMove() && !undoAsked;
	}

	Ui::ReversiGui* ui;
	MessageOverlay* overlay;
	Game game;
	engine::Engine ai;
	std::thread searchThread;
	engine::Engine analyzer{ 18 };  // nur fuer die Bewertungsanzeige
	std::thread analysisThread;
	int shownEvalDepth = -1;       // Tiefe der angezeigten Bewertung (tiefere ersetzen flachere)
	int shownEvalBlack = 0;        // angezeigte Bewertung aus Sicht von Schwarz
	bool shownEvalProven = false;
	struct StrongMoveCheck
	{
		bool active = false;
		int forPosition = -1;      // Stellung nach dem Zug
		Color mover = Color::Black;
		int before = 0;            // Bewertung vor dem Zug aus Sicht des Ziehenden (begrenzt)
	} strongCheck;
	QLabel* strongLabel;           // kurz eingeblendet ueber dem Brett
	int strongShowId = 0;
	Mode mode = Mode::VsComputer;
	Color computerColor = Color::White;

	int positionId = 0;            // steigt bei jeder neuen Stellung -> veraltete Ergebnisse verwerfen
	bool computerThinking = false;
	bool hintRunning = false;
	int hintSquare = -1;           // Feldindex des Zugvorschlags, -1 = keiner
	QElapsedTimer hintClock;       // seit wann der Tipp rechnet
	engine::Result pendingHint;    // bester Vorschlag aus der ersten Sekunde
	bool hintPending = false;
	std::vector<int> framedMoves;  // rote Ringe: Zug/Zuege des PCs (zu zweit: des letzten Spielers) in Folge
	Color framedMover = Color::Black;
	bb::u64 framedFlips = 0;       // orange Ringe: umgedrehte Steine (nur bei einem einzelnen Zug)
	bool animateNextUpdate = false;
	QString passInfo;              // z. B. "Weiss muss passen. "
	bool quitConfirmed = false;

	// Bluetooth
	BluetoothLink* link;
	BluetoothLobby* lobby;
	Color myColor = Color::Black;  // eigene Farbe in der Bluetooth-Partie
	bool undoAsked = false;        // Rueckfrage "Zug zuruecknehmen" laeuft -> eigene Zuege gesperrt
	bool newGameAsked = false;     // Rueckfrage "Neues Spiel" laeuft
	Game::StartOptions askedStart; // Startoptionen dieser Rueckfrage
};
