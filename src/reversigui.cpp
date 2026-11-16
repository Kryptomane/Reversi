#include "reversigui.h"
#include "ui_reversigui.h"
#include "messageoverlay.h"
#include "bluetoothlink.h"
#include "bluetoothlobby.h"
#include <QApplication>
#include <QCloseEvent>
#include <QKeyEvent>
#include <QSettings>
#include <QSignalBlocker>
#include <QTimer>
#include <QDebug>
#include <cstdlib>

ReversiGui::ReversiGui(QWidget* parent) : QWidget(parent), ui(new Ui::ReversiGui)
{
	ui->setupUi(this);
	strongLabel = new QLabel(this);
	strongLabel->setAlignment(Qt::AlignCenter);
	strongLabel->setAttribute(Qt::WA_TransparentForMouseEvents); // Klicks gehen aufs Brett
	strongLabel->setStyleSheet(QStringLiteral(
		"background: #2e7d32; color: white; border: 2px solid white; border-radius: 12px; padding: 6px 16px; font-weight: bold;"));
	strongLabel->hide();
	link = new BluetoothLink(this);
	lobby = new BluetoothLobby(this, link);
	overlay = new MessageOverlay(this); // nach der Lobby anlegen: Meldungen liegen darueber
	computerColor = QSettings().value("computerColor", 1).toInt() == 0 ? Color::Black : Color::White;
#ifndef Q_OS_ANDROID
	resize(460, 860); // Desktop: Hochformat wie am Handy, frei skalierbar
#endif
	setupModeBox();
	setupLevelBox();
	ui->hintBox->setChecked(QSettings().value("hint", false).toBool());
	ui->animateBox->setChecked(QSettings().value("animate", true).toBool());
	updateHintLabel(nullptr);

	connect(ui->board, &BoardWidget::fieldClicked, this, &ReversiGui::onFieldClicked);
	connect(ui->newGameButton, &QPushButton::clicked, this, &ReversiGui::requestNewGame);
	connect(ui->undoButton, &QPushButton::clicked, this, &ReversiGui::undoMove);
	connect(ui->swapButton, &QPushButton::clicked, this, &ReversiGui::requestColorSwap);
	connect(ui->exitGameButton, &QPushButton::clicked, this, &ReversiGui::confirmQuit);
	connect(ui->hintBox, &QCheckBox::toggled, this, &ReversiGui::onHintToggled);
	connect(ui->animateBox, &QCheckBox::toggled, this, [](bool on) { QSettings().setValue("animate", on); });
	connect(lobby, &BluetoothLobby::cancelled, this, [this]() { selectModeBox(mode); });
	connect(link, &BluetoothLink::connected, this, &ReversiGui::onBluetoothConnected);
	connect(link, &BluetoothLink::lineReceived, this, &ReversiGui::onBluetoothLine);
	connect(link, &BluetoothLink::disconnected, this, &ReversiGui::onBluetoothLost);
	qApp->installEventFilter(this);

	startNewGame();
}

ReversiGui::~ReversiGui()
{
	stopSearch();
	stopAnalysis();
	delete ui;
}

// ======================= Ablauf =======================

// Dialog mit Startoptionen; laeuft eine Partie, wird zusaetzlich darauf hingewiesen.
void ReversiGui::requestNewGame()
{
	QSettings settings;
	MessageOverlay::Message m;
	m.title = QStringLiteral("Neues Spiel");
	if (mode == Mode::Bluetooth)
		m.detail = peerName() + QStringLiteral(" muss zustimmen. Die Farben werden getauscht.");
	else if (gameInProgress())
		m.detail = QStringLiteral("Die laufende Partie geht verloren.");
	m.yesText = mode == Mode::Bluetooth ? QStringLiteral("Anfragen") : QStringLiteral("Starten");
	m.options = { { QStringLiteral("Startstellung tauschen"), settings.value("swapStart", false).toBool() },
	              { QStringLiteral("Wei\u00DF beginnt"), settings.value("whiteBegins", false).toBool() } };
	overlay->show(m, [this]() {
		QSettings s;
		s.setValue("swapStart", overlay->isOptionChecked(0));
		s.setValue("whiteBegins", overlay->isOptionChecked(1));
		if (mode != Mode::Bluetooth)
			return startNewGame();
		askedStart = savedStartOptions();
		newGameAsked = true;
		link->send(QStringLiteral("NEW? %1 %2").arg(askedStart.swappedCenter ? 1 : 0).arg(askedStart.first == Color::White ? 1 : 0));
		showStatus(QStringLiteral("Warte auf Zustimmung von ") + peerName() + QStringLiteral(" \u2026"));
	});
}

void ReversiGui::requestColorSwap()
{
	const Color human = opponent(computerColor);
	overlay->show({ QStringLiteral("Farben tauschen?"),
	                QStringLiteral("Du spielst dann ") + colorName(computerColor) + QStringLiteral(", der PC ") + colorName(human) + ".",
	                QStringLiteral("Tauschen") }, [this]() { swapColors(); });
}

// Die Partie laeuft weiter; der PC uebernimmt die bisherige Farbe des Menschen.
void ReversiGui::swapColors()
{
	stopSearch();
	computerColor = opponent(computerColor);
	QSettings().setValue("computerColor", computerColor == Color::Black ? 0 : 1);
	clearFrames();
	if (!game.isOver())
		handleTurn(Game::Turn::Next);
	else
		updateView();
}

// Erst nach der Animation des letzten Zugs zeigen, damit man ihn noch sieht.
void ReversiGui::showGameOver()
{
	const int wait = ui->board->animationRemainingMs();
	QTimer::singleShot(wait + 400, this, [this, forPosition = positionId]() {
		if (forPosition != positionId || !game.isOver())
			return;
		const int b = game.count(Color::Black), w = game.count(Color::White);
		const QString score = QStringLiteral("Schwarz ") + QString::number(b) + " : " + QString::number(w) + QStringLiteral(" Wei\u00DF");
		QString title;
		if (b == w)
			title = QStringLiteral("Unentschieden!");
		else if (mode == Mode::TwoPlayers)
			title = colorName(b > w ? Color::Black : Color::White) + QStringLiteral(" gewinnt!");
		else if (mode == Mode::Bluetooth)
			title = (b > w ? Color::Black : Color::White) == myColor ? QStringLiteral("Gl\u00FCckwunsch!")
			                                                         : peerName() + QStringLiteral(" gewinnt!");
		else
			title = isComputer(b > w ? Color::Black : Color::White) ? QStringLiteral("Weiter \u00FCben ;)") : QStringLiteral("Gl\u00FCckwunsch!");
		overlay->show({ title, score, QStringLiteral("Neues Spiel"), QStringLiteral("Zur\u00FCck zum Spiel"), true },
		              [this]() { requestNewGame(); }); // Dialog mit Startoptionen
	});
}

Game::StartOptions ReversiGui::savedStartOptions()
{
	QSettings settings; // Startoptionen aus dem "Neues Spiel"-Dialog
	Game::StartOptions start;
	start.swappedCenter = settings.value("swapStart", false).toBool();
	start.first = settings.value("whiteBegins", false).toBool() ? Color::White : Color::Black;
	return start;
}

void ReversiGui::startNewGame()
{
	startGame(savedStartOptions());
}

void ReversiGui::startGame(const Game::StartOptions& start)
{
	stopSearch();
	strongCheck = StrongMoveCheck();
	game.reset(start);
	ai.clear();
	clearFrames();
	handleTurn(Game::Turn::Next);
}

void ReversiGui::onFieldClicked(int x, int y)
{
	const int square = bb::square(x, y);
	if (!humanToMove() || !game.isLegal(square))
		return;
	if (hintRunning)
		stopSearch(); // Tipp wird nicht mehr gebraucht
	// gegen den PC (und den Mitspieler am anderen Geraet) verschwinden die Rahmen, sobald man selbst
	// zieht; zu zweit an einem Geraet zeigen sie den letzten Zug
	if (mode == Mode::Bluetooth)
		link->send(QStringLiteral("MOVE ") + QString::number(square));
	handleTurn(playAndRemember(square, mode == Mode::TwoPlayers));
}

// Gegen den PC wird zurueckgenommen, bis wieder der Mensch am Zug ist: also der letzte PC-Zug
// (bzw. beide, wenn der Mensch passen musste) und der letzte eigene Zug. Zu zweit genau ein Zug.
void ReversiGui::undoMove()
{
	if (!game.canUndo())
		return;
	if (mode == Mode::Bluetooth) // der Mitspieler muss zustimmen
	{
		if (undoAsked)
			return;
		undoAsked = true;
		link->send(QStringLiteral("UNDO? ") + QString::number(moveStamp()));
		showStatus(QStringLiteral("Warte auf Zustimmung von ") + peerName() + QStringLiteral(" \u2026"));
		updateView();
		return;
	}
	if (mode == Mode::TwoPlayers)
	{
		stopSearch();
		strongCheck = StrongMoveCheck();
		game.undo();
		clearFrames();
		handleTurn(Game::Turn::Next);
		return;
	}
	undoUntil(opponent(computerColor));
}

// Beide Geraete einer Bluetooth-Partie rufen das mit derselben Farbe auf -> gleiche Stellung.
void ReversiGui::undoUntil(Color who)
{
	stopSearch();
	strongCheck = StrongMoveCheck();
	do
		game.undo();
	while (game.canUndo() && game.toMove() != who);
	clearFrames();
	handleTurn(Game::Turn::Next);
}

void ReversiGui::handleTurn(Game::Turn turn)
{
	positionId++;
	hintSquare = -1;
	updateHintLabel(nullptr);
	updateView();

	if (game.isOver())
	{
		stopAnalysis();
		showFinalEvaluation();
		showStatus(gameOverText());
		showGameOver();
		return;
	}
	startAnalysis();
	passInfo = turn == Game::Turn::OpponentPasses
		? colorName(opponent(game.toMove())) + QStringLiteral(" muss passen. ") : QString();

	if (isComputer(game.toMove()))
	{
		startSearch(SearchKind::ComputerMove);
		showStatus(passInfo + QStringLiteral("PC (") + colorName(computerColor) + QStringLiteral(") denkt ..."));
	}
	else if (remoteToMove())
		showStatus(passInfo + peerName() + QStringLiteral(" (") + colorName(game.toMove()) + QStringLiteral(") ist dran \u2026"));
	else
	{
		showStatus(passInfo + yourTurnText());
		if (hintAllowed())
			startSearch(SearchKind::Hint);
	}
}

// ======================= Engine im Hintergrund =======================

void ReversiGui::startSearch(SearchKind kind)
{
	stopSearch(); // hoechstens eine Suche gleichzeitig
	hintRunning = kind == SearchKind::Hint;
	computerThinking = !hintRunning;
	if (hintRunning)
	{
		hintPending = false;
		hintClock.start();
		QTimer::singleShot(HINT_DELAY_MS, this, [this, forPosition = positionId]() {
			if (forPosition == positionId && hintPending)
				showHint(pendingHint);
		});
	}

	engine::Settings s = hintRunning ? engine::hintSettings() : engine::levelSettings(level());
	const int forPosition = positionId;
	s.onProgress = [this, forPosition, kind](const engine::Result& r) { // laeuft im Such-Thread
		QMetaObject::invokeMethod(this, [this, r, forPosition, kind]() { onSearchProgress(r, forPosition, kind); },
		                          Qt::QueuedConnection);
	};
	const bb::u64 P = game.own(), O = game.opponentDiscs(); // Kopie: der Thread fasst die GUI-Daten nicht an
	ai.resetStop();
	searchThread = std::thread([this, P, O, s, forPosition, kind]() {
		const engine::Result r = ai.search(P, O, s);
		QMetaObject::invokeMethod(this, [this, r, forPosition, kind]() { onSearchResult(r, forPosition, kind); },
		                          Qt::QueuedConnection);
	});
}

// Zwischenstand: Tipp sofort aktualisieren, beim PC die erreichte Tiefe anzeigen.
void ReversiGui::onSearchProgress(const engine::Result& r, int forPosition, SearchKind kind)
{
	if (forPosition != positionId || r.move < 0)
		return;
	onEvaluation(r, forPosition, game.toMove()); // beide Suchen rechnen fuer die aktuelle Stellung
	if (kind == SearchKind::Hint)
		offerHint(r);
	else if (computerThinking)
		showStatus(passInfo + QStringLiteral("PC (") + colorName(computerColor) + QStringLiteral(") denkt ...\nTiefe ") + QString::number(r.depth));
}

void ReversiGui::onSearchResult(const engine::Result& r, int forPosition, SearchKind kind)
{
	if (forPosition != positionId || game.isOver() || r.move < 0)
		return; // Ergebnis fuer eine Stellung, die es nicht mehr gibt

	if (kind == SearchKind::Hint)
	{
		hintRunning = false;
		if (!r.stopped)
		{
			onEvaluation(r, forPosition, game.toMove());
			offerHint(r);
		}
		return;
	}

	// Laeuft noch die Animation des menschlichen Zugs, den PC-Zug erst danach zeigen.
	if (const int wait = ui->board->animationRemainingMs(); wait > 0)
	{
		QTimer::singleShot(wait + 30, this, [this, r, forPosition, kind]() { onSearchResult(r, forPosition, kind); });
		return;
	}
	applyComputerMove(r);
}

void ReversiGui::applyComputerMove(const engine::Result& r)
{
	computerThinking = false;
	qDebug() << "PC-Zug" << r.move / 8 << r.move % 8 << "Tiefe" << r.depth << "Wert" << r.score
	         << (r.exact ? "exakt" : r.wld ? "WLD" : "") << "Knoten" << r.nodes << "Zeit" << r.seconds;
	handleTurn(playAndRemember(r.move, true));
}

// Zieht derselbe Spieler mehrmals hintereinander (der andere musste passen), werden seine Zuege
// nummeriert gesammelt; die orangen Ringe gibt es nur bei einem einzelnen Zug.
Game::Turn ReversiGui::playAndRemember(int square, bool showFrames)
{
	const Color mover = game.toMove();
	checkStrongMove(); // kam dieser Zug, bevor die Bewertung des vorigen fertig war: jetzt entscheiden
	rememberEvalBeforeMove(mover);
	if (!showFrames)
		clearFrames();
	else
	{
		if (framedMoves.empty() || framedMover != mover)
			framedMoves.clear();
		framedMoves.push_back(square);
		framedMover = mover;
		framedFlips = framedMoves.size() == 1 ? bb::flips(game.own(), game.opponentDiscs(), 1ULL << square) : 0;
	}
	animateNextUpdate = true;
	return game.play(square);
}

void ReversiGui::clearFrames()
{
	framedMoves.clear();
	framedFlips = 0;
}

void ReversiGui::offerHint(const engine::Result& r)
{
	if (hintClock.elapsed() < HINT_DELAY_MS)
	{
		pendingHint = r; // der Timer aus startSearch zeigt ihn nach Ablauf der Sekunde
		hintPending = true;
		return;
	}
	showHint(r);
}

void ReversiGui::showHint(const engine::Result& r)
{
	hintPending = false;
	if (!hintAllowed() || !humanToMove())
		return;
	hintSquare = r.move;
	updateHintLabel(&r);
	updateView();
}

void ReversiGui::stopSearch()
{
	ai.stop();
	if (searchThread.joinable())
		searchThread.join();
	computerThinking = false;
	hintRunning = false;
	hintPending = false;
}

// ======================= Bewertungsanzeige =======================

void ReversiGui::startAnalysis()
{
	stopAnalysis();
	shownEvalDepth = -1;
	engine::Settings s;
	s.maxDepth = 10;
	s.endgameEmpties = 16;
	s.threads = 1;
	s.timeMs = 500;
	const bb::u64 P = game.own(), O = game.opponentDiscs();
	const int forPosition = positionId;
	const Color side = game.toMove();
	analyzer.resetStop();
	analysisThread = std::thread([this, P, O, s, forPosition, side]() {
		const engine::Result r = analyzer.search(P, O, s);
		QMetaObject::invokeMethod(this, [this, r, forPosition, side]() { onAnalysisDone(r, forPosition, side); },
		                          Qt::QueuedConnection);
	});
}

void ReversiGui::stopAnalysis()
{
	analyzer.stop();
	if (analysisThread.joinable())
		analysisThread.join();
}

// Nimmt eine Bewertung der aktuellen Stellung entgegen; tiefere (oder bewiesene) ersetzen flachere.
void ReversiGui::onEvaluation(const engine::Result& r, int forPosition, Color sideToMove)
{
	if (forPosition != positionId || r.move < 0 || r.depth <= 0)
		return;
	const bool proven = r.exact || r.wld;
	const int depth = proven ? 1000 : r.depth;
	if (depth <= shownEvalDepth)
		return;
	shownEvalDepth = depth;
	shownEvalBlack = sideToMove == Color::Black ? r.score : -r.score;
	shownEvalProven = proven;
	showEvaluation(shownEvalBlack, proven);
}

void ReversiGui::showEvaluation(int blackScore, bool proven)
{
	const QString white = QStringLiteral("Wei\u00DF");
	QString result;
	if (proven)
		result = QStringLiteral("<b>") + (blackScore > 0 ? QStringLiteral("Schwarz") : blackScore < 0 ? white : QStringLiteral("Remis")) + "</b>";
	else
	{
		const int advantage = std::abs(blackScore);
		const QString sign = advantage < EVAL_EQUAL_BAND ? QStringLiteral("=") : blackScore > 0 ? QStringLiteral("&gt;") : QStringLiteral("&lt;");
		const double scale = 1.0 + 2.0 * std::min(advantage, EVAL_FULL_SCALE) / EVAL_FULL_SCALE; // 1x .. 3x
		const int px = (int)(ui->evalLabel->fontInfo().pixelSize() * scale);
		result = QStringLiteral("Schwarz <span style=\"font-size:%1px; font-weight:bold;\">%2</span> ").arg(px).arg(sign) + white;
	}
	ui->evalLabel->setText(QStringLiteral("Gewinnchance:<br>") + result);
}

void ReversiGui::showFinalEvaluation()
{
	showEvaluation(game.count(Color::Black) - game.count(Color::White), true);
}

void ReversiGui::onAnalysisDone(const engine::Result& r, int forPosition, Color sideToMove)
{
	onEvaluation(r, forPosition, sideToMove);
	if (forPosition == positionId)
		checkStrongMove();
}

// ======================= Starker Zug =======================
// Ein Zug ist "stark", wenn danach die Gewinnchance des Ziehenden deutlich besser ist als davor.
// Bei perfekter Rechnung koennte ein Zug die Lage nur halten; besser wird sie, wenn der Zug
// staerker ist, als die Bewertung vorher erkannt hatte. Gilt fuer alle Spieler in allen Modi.

int ReversiGui::clampedScore(int score, bool proven)
{
	if (proven) // bewiesen: Sieg/Niederlage auf die Skala der Schaetzung abbilden
		return score > 0 ? EVAL_FULL_SCALE : score < 0 ? -EVAL_FULL_SCALE : 0;
	return std::clamp(score, -EVAL_FULL_SCALE, EVAL_FULL_SCALE);
}

// Direkt vor dem Setzen; die Stellung danach bekommt positionId + 1 (handleTurn).
void ReversiGui::rememberEvalBeforeMove(Color mover)
{
	strongCheck = StrongMoveCheck();
	if (shownEvalDepth < STRONG_MOVE_MIN_DEPTH || shownEvalProven)
		return; // zu ungenau bzw. schon bewiesen (dann kann kein Zug mehr etwas verbessern)
	strongCheck.active = true;
	strongCheck.forPosition = positionId + 1;
	strongCheck.mover = mover;
	const int black = clampedScore(shownEvalBlack, false);
	strongCheck.before = mover == Color::Black ? black : -black;
}

void ReversiGui::checkStrongMove()
{
	if (!strongCheck.active || strongCheck.forPosition != positionId || shownEvalDepth < STRONG_MOVE_MIN_DEPTH)
		return;
	strongCheck.active = false;
	const int black = clampedScore(shownEvalBlack, shownEvalProven);
	const int after = strongCheck.mover == Color::Black ? black : -black;
	if (after - strongCheck.before >= STRONG_MOVE_GAIN)
		showStrongMove(strongCheck.mover);
}

void ReversiGui::showStrongMove(Color mover)
{
	QString text = QStringLiteral("Starker Zug!");
	if (mode == Mode::TwoPlayers)
		text = QStringLiteral("Starker Zug von ") + colorName(mover) + "!";
	else if (isComputer(mover))
		text = QStringLiteral("Starker Zug vom PC");
	else if (mode == Mode::Bluetooth && mover != myColor)
		text = QStringLiteral("Starker Zug von ") + peerName();
	strongLabel->setText(text);
	strongLabel->adjustSize();
	const QRect board = ui->board->geometry();
	strongLabel->move(board.x() + (board.width() - strongLabel->width()) / 2, board.y() + board.height() / 25);
	strongLabel->show();
	const int id = ++strongShowId;
	QTimer::singleShot(STRONG_MOVE_SHOW_MS, this, [this, id]() {
		if (id == strongShowId)
			strongLabel->hide();
	});
}

// ======================= Bluetooth (zwei Geraete) =======================
//
// Protokoll: eine Textzeile je Nachricht. Wer eroeffnet (Host), legt die Startstellung fest.
//   START <version> <tausch 0/1> <weissBeginnt 0/1> <HostFarbe B/W>   Host -> Gast, nach dem Verbinden
//   MOVE <feld>                     Zug (Feld 0..63); der Empfaenger prueft, ob er legal ist
//   UNDO? <stempel>  UNDO_OK  UNDO_NO   Zug zuruecknehmen (Stempel = Steinzahl, erkennt veraltete Fragen)
//   NEW? <tausch> <weissBeginnt>  NEW_OK  NEW_NO   neues Spiel, die Farben werden dabei getauscht
//   BYE                             Mitspieler verlaesst die Partie
// Kommen beide Rueckfragen gleichzeitig, gilt die des Hosts (sonst liefen die Stellungen auseinander).

namespace {
constexpr int BLUETOOTH_PROTOCOL = 1;
}

void ReversiGui::openBluetoothLobby()
{
	selectModeBox(mode); // bis die Verbindung steht, bleibt der bisherige Modus
	stopSearch();
	lobby->open();
}

// Host: eigene Startoptionen schicken und loslegen. Gast: wartet in der Lobby auf START.
void ReversiGui::onBluetoothConnected()
{
	if (!link->isHost())
		return;
	lobby->dismiss();
	const Game::StartOptions start = savedStartOptions();
	myColor = Color::Black;
	link->send(QStringLiteral("START %1 %2 %3 B").arg(BLUETOOTH_PROTOCOL)
		.arg(start.swappedCenter ? 1 : 0).arg(start.first == Color::White ? 1 : 0));
	mode = Mode::Bluetooth;
	selectModeBox(mode);
	applyModeUi();
	startBluetoothGame(start, false);
}

void ReversiGui::onBluetoothLine(const QString& line)
{
	const QStringList part = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
	const QString cmd = part.value(0);
	auto number = [&part](int i) { return part.value(i).toInt(); };
	qDebug() << "Bluetooth <-" << line;

	if (cmd == QStringLiteral("START"))
	{
		if (link->isHost())
			return;
		lobby->dismiss();
		if (part.size() < 5 || number(1) != BLUETOOTH_PROTOCOL)
		{
			leaveBluetooth(true);
			showNotice(QStringLiteral("Andere App-Version"),
			           QStringLiteral("Bitte auf beiden Ger\u00E4ten dieselbe Version von Reversi installieren."));
			return;
		}
		Game::StartOptions start;
		start.swappedCenter = number(2) != 0;
		start.first = number(3) != 0 ? Color::White : Color::Black;
		myColor = part.value(4) == QStringLiteral("B") ? Color::White : Color::Black;
		mode = Mode::Bluetooth;
		selectModeBox(mode);
		applyModeUi();
		startBluetoothGame(start, false);
		return;
	}
	if (mode != Mode::Bluetooth)
		return;

	if (cmd == QStringLiteral("MOVE"))
	{
		bool ok = false;
		const int square = part.value(1).toInt(&ok);
		if (!ok || !remoteToMove() || !game.isLegal(square))
		{
			qDebug() << "Ungueltiger Zug vom Mitspieler:" << line;
			onBluetoothLost(); // genauso wie ein Verbindungsabbruch
			return;
		}
		if (hintRunning)
			stopSearch();
		handleTurn(playAndRemember(square, true)); // roter Ring wie beim PC-Zug
	}
	else if (cmd == QStringLiteral("UNDO?"))
	{
		if (undoAsked)
		{
			if (link->isHost())
				return link->send(QStringLiteral("UNDO_NO")); // eigene Frage hat Vorrang
			undoAsked = false;                            // eigene Frage verwerfen, die des Hosts gilt
		}
		const int stamp = number(1);
		if (stamp != moveStamp() || !game.canUndo())
			return link->send(QStringLiteral("UNDO_NO"));
		overlay->show({ QStringLiteral("Zug zur\u00FCcknehmen?"),
		                peerName() + QStringLiteral(" m\u00F6chte den letzten Zug zur\u00FCcknehmen."),
		                QStringLiteral("Erlauben"), QStringLiteral("Ablehnen") },
		              [this, stamp]() {
			              if (mode != Mode::Bluetooth)
				              return;
			              if (stamp != moveStamp() || !game.canUndo())
				              return link->send(QStringLiteral("UNDO_NO"));
			              link->send(QStringLiteral("UNDO_OK"));
			              undoUntil(opponent(myColor));
		              },
		              [this]() { link->send(QStringLiteral("UNDO_NO")); });
	}
	else if (cmd == QStringLiteral("UNDO_OK"))
	{
		if (!undoAsked)
			return;
		undoAsked = false;
		undoUntil(myColor);
	}
	else if (cmd == QStringLiteral("UNDO_NO"))
	{
		if (!undoAsked)
			return;
		undoAsked = false;
		handleTurn(Game::Turn::Next); // Status und Zugmarken wiederherstellen
		showNotice(peerName() + QStringLiteral(" m\u00F6chte den Zug nicht zur\u00FCcknehmen."));
	}
	else if (cmd == QStringLiteral("NEW?"))
	{
		if (newGameAsked)
		{
			if (link->isHost())
				return link->send(QStringLiteral("NEW_NO"));
			newGameAsked = false;
		}
		Game::StartOptions start;
		start.swappedCenter = number(1) != 0;
		start.first = number(2) != 0 ? Color::White : Color::Black;
		QString detail = peerName() + QStringLiteral(" m\u00F6chte ein neues Spiel starten. Du spielst dann ")
			+ colorName(opponent(myColor)) + ".";
		if (start.swappedCenter)
			detail += QStringLiteral("\nStartstellung getauscht.");
		if (start.first == Color::White)
			detail += QStringLiteral("\nWei\u00DF beginnt.");
		overlay->show({ QStringLiteral("Neues Spiel?"), detail, QStringLiteral("Starten"), QStringLiteral("Ablehnen") },
		              [this, start]() {
			              if (mode != Mode::Bluetooth)
				              return;
			              link->send(QStringLiteral("NEW_OK"));
			              startBluetoothGame(start, true);
		              },
		              [this]() { link->send(QStringLiteral("NEW_NO")); });
	}
	else if (cmd == QStringLiteral("NEW_OK"))
	{
		if (newGameAsked)
			startBluetoothGame(askedStart, true);
	}
	else if (cmd == QStringLiteral("NEW_NO"))
	{
		if (!newGameAsked)
			return;
		newGameAsked = false;
		handleTurn(Game::Turn::Next);
		showNotice(peerName() + QStringLiteral(" m\u00F6chte gerade kein neues Spiel."));
	}
	else if (cmd == QStringLiteral("BYE"))
	{
		const QString name = peerName();
		leaveBluetooth(false);
		showNotice(name + QStringLiteral(" hat die Partie verlassen."),
		           QStringLiteral("Ihr k\u00F6nnt hier zu zweit an einem Ger\u00E4t weiterspielen."));
	}
	// unbekannte Befehle (neuere Version) ignorieren
}

void ReversiGui::onBluetoothLost()
{
	const QString name = peerName();
	if (lobby->isOpen()) // Gast wartete noch auf START
	{
		lobby->dismiss();
		link->close();
		selectModeBox(mode);
		showNotice(QStringLiteral("Verbindung getrennt"));
		return;
	}
	if (mode != Mode::Bluetooth)
		return;
	leaveBluetooth(false);
	showNotice(QStringLiteral("Verbindung zu ") + name + QStringLiteral(" verloren"),
	           QStringLiteral("Ihr k\u00F6nnt hier zu zweit an einem Ger\u00E4t weiterspielen."));
}

void ReversiGui::leaveBluetooth(bool notifyPeer)
{
	if (notifyPeer)
		link->sendAndClose(QStringLiteral("BYE"));
	else
		link->close();
	undoAsked = false;
	newGameAsked = false;
	if (overlay->isOpen())
		overlay->cancel(); // offene Rueckfrage des Mitspielers ist hinfaellig
	if (mode == Mode::Bluetooth)
		setMode(Mode::TwoPlayers);
}

void ReversiGui::startBluetoothGame(const Game::StartOptions& start, bool swapColors)
{
	if (swapColors)
		myColor = opponent(myColor);
	undoAsked = false;
	newGameAsked = false;
	startGame(start);
}

void ReversiGui::showNotice(const QString& title, const QString& detail)
{
	overlay->show({ title, detail, QStringLiteral("OK"), QString() }, nullptr);
}

QString ReversiGui::peerName() const
{
	const QString name = link->peerName();
	return name.isEmpty() ? QStringLiteral("Mitspieler") : name;
}

// ======================= Einstellungen =======================

void ReversiGui::setupModeBox()
{
	ui->modeBox->addItem(QStringLiteral("Computer"), (int)Mode::VsComputer);
	ui->modeBox->addItem(QStringLiteral("Mensch (2 Spieler)"), (int)Mode::TwoPlayers);
	if (BluetoothLink::compiledIn())
		ui->modeBox->addItem(QStringLiteral("Mensch \u00FCber Bluetooth"), (int)Mode::Bluetooth);
	mode = QSettings().value("mode", 0).toInt() == (int)Mode::TwoPlayers ? Mode::TwoPlayers : Mode::VsComputer;
	ui->modeBox->setCurrentIndex(ui->modeBox->findData((int)mode));
	applyModeUi();
	connect(ui->modeBox, &QComboBox::currentIndexChanged, this, &ReversiGui::onModeChanged);
}

void ReversiGui::setupLevelBox()
{
	static const char16_t* const names[engine::LEVELS] = {
		u"Anf\u00E4nger", u"Leicht", u"Leicht+", u"Mittel", u"Mittel+", u"Fortgeschritten",
		u"Stark", u"Meister", u"Gro\u00DFmeister", u"Weltklasse" };
	for (int l = 1; l <= engine::LEVELS; l++)
	{
		const QString text = QString::number(l) + QStringLiteral(" \u2013 ") + QString::fromUtf16(names[l - 1])
			+ QStringLiteral(" (Tiefe ") + QString::number(engine::levelSettings(l).maxDepth) + ")";
		ui->levelBox->addItem(text, l);
	}
	ui->levelBox->setCurrentIndex(std::clamp(QSettings().value("level", 5).toInt(), 1, engine::LEVELS) - 1);
	connect(ui->levelBox, &QComboBox::currentIndexChanged, this, [this]() {
		QSettings().setValue("level", level()); // gilt ab dem naechsten PC-Zug
	});
}

// Wechsel zwischen PC- und Zwei-Spieler-Modus: die laufende Partie geht einfach weiter.
// Bluetooth wird erst nach dem Verbindungsaufbau aktiv; eine Bluetooth-Partie verlaesst man nach Rueckfrage.
void ReversiGui::onModeChanged()
{
	const Mode chosen = (Mode)ui->modeBox->currentData().toInt();
	if (chosen == mode)
		return;
	if (mode == Mode::Bluetooth)
	{
		selectModeBox(mode); // erst nach "Verlassen" wirklich wechseln
		overlay->show({ QStringLiteral("Bluetooth-Partie verlassen?"),
		                QStringLiteral("Die Verbindung zu ") + peerName() + QStringLiteral(" wird getrennt."),
		                QStringLiteral("Verlassen") }, [this, chosen]() {
			leaveBluetooth(true);
			setMode(chosen);
		});
		return;
	}
	if (chosen == Mode::Bluetooth)
		return openBluetoothLobby();
	setMode(chosen);
}

void ReversiGui::setMode(Mode m)
{
	mode = m;
	selectModeBox(m);
	if (m != Mode::Bluetooth) // Bluetooth braucht beim Start eine neue Verbindung: nicht merken
		QSettings().setValue("mode", (int)m);
	applyModeUi();
	stopSearch();
	clearFrames();
	handleTurn(Game::Turn::Next);
}

void ReversiGui::selectModeBox(Mode m)
{
	const QSignalBlocker block(ui->modeBox);
	ui->modeBox->setCurrentIndex(ui->modeBox->findData((int)m));
}

bool ReversiGui::hintAllowed() const
{
	return ui->hintBox->isChecked() && mode != Mode::Bluetooth;
}

void ReversiGui::applyModeUi()
{
	ui->levelBox->setEnabled(mode == Mode::VsComputer);
	ui->hintBox->setEnabled(mode != Mode::Bluetooth); // Haken bleibt gemerkt, wirkt nur ohne Bluetooth
}

int ReversiGui::level() const
{
	return ui->levelBox->currentData().toInt();
}

void ReversiGui::onHintToggled(bool on)
{
	QSettings().setValue("hint", on);
	if (on && hintAllowed() && humanToMove() && hintSquare < 0 && !hintRunning)
		startSearch(SearchKind::Hint);
	if (!on)
	{
		if (hintRunning)
			stopSearch(); // nur die Tipp-Suche abbrechen, nie die PC-Suche
		hintSquare = -1;
		updateHintLabel(nullptr);
		updateView();
	}
}

// ======================= Anzeige =======================

void ReversiGui::updateView()
{
	BoardState s;
	s.black = game.discs(Color::Black);
	s.white = game.discs(Color::White);
	s.markers = humanToMove() ? game.legalMoves() : 0;
	s.hint = hintSquare;
	s.lastMoves = framedMoves;
	s.lastFlipped = framedFlips;
	ui->board->setState(s, animateNextUpdate && ui->animateBox->isChecked());
	animateNextUpdate = false;

	// Feste Farben (unabhaengig von hellem/dunklem Design): Schwarz dunkel, Weiss hell;
	// wer am Zug ist, bekommt einen gruenen Rand, sonst eine graue Kontur (sichtbar in beiden Designs).
	const bool running = !game.isOver();
	auto scoreStyle = [running, this](Color c) {
		const QString colors = c == Color::Black ? QStringLiteral("background: #202020; color: #ffffff;")
		                                         : QStringLiteral("background: #f2f2f2; color: #202020;");
		const QString border = running && game.toMove() == c ? QStringLiteral("#43a047") : QStringLiteral("#8a8a8a"); // gruen = am Zug, grau = Kontur
		return colors + QStringLiteral(" border: 3px solid ") + border + ";";
	};
	ui->BlackChipsLabel->setText(QStringLiteral("Schwarz  ") + QString::number(game.count(Color::Black)));
	ui->WhiteChipsLabel->setText(QStringLiteral("Wei\u00DF  ") + QString::number(game.count(Color::White)));
	ui->BlackChipsLabel->setStyleSheet(scoreStyle(Color::Black));
	ui->WhiteChipsLabel->setStyleSheet(scoreStyle(Color::White));
	ui->undoButton->setEnabled(game.canUndo() && !undoAsked);
	ui->swapButton->setVisible(mode == Mode::VsComputer);
}

// Beschriftung des Tipp-Schalters: zeigt, wie tief der Vorschlag schon gerechnet ist.
void ReversiGui::updateHintLabel(const engine::Result* r)
{
	QString text = QStringLiteral("Besten Zug zeigen");
	if (r && (r->exact || r->wld))
		text += r->score > 0 ? QStringLiteral(" (Sieg sicher)")
		      : r->score < 0 ? QStringLiteral(" (beste Verteidigung)") : QStringLiteral(" (Remis sicher)");
	else if (r)
		text += QStringLiteral(" (Tiefe ") + QString::number(r->depth) + ")";
	ui->hintBox->setText(text);
}

void ReversiGui::showStatus(const QString& text)
{
	ui->MessageLabel->setText(text);
}

QString ReversiGui::colorName(Color c)
{
	return c == Color::Black ? QStringLiteral("Schwarz") : QStringLiteral("Wei\u00DF");
}

QString ReversiGui::yourTurnText() const
{
	if (mode == Mode::TwoPlayers)
		return colorName(game.toMove()) + QStringLiteral(" ist dran.");
	if (mode == Mode::Bluetooth)
		return QStringLiteral("Du bist dran (") + colorName(game.toMove()) + QStringLiteral(")\nGegner: ") + peerName();
	return QStringLiteral("Du bist dran (") + colorName(game.toMove()) + QStringLiteral(")\nPC: ") + colorName(computerColor);
}

QString ReversiGui::gameOverText() const
{
	const int b = game.count(Color::Black), w = game.count(Color::White);
	const QString score = QString::number(b) + ":" + QString::number(w);
	if (b == w)
		return QStringLiteral("Unentschieden ") + score;
	return colorName(b > w ? Color::Black : Color::White) + QStringLiteral(" gewinnt ") + score;
}

// ======================= Beenden =======================

bool ReversiGui::eventFilter(QObject* object, QEvent* e)
{
	if (e->type() == QEvent::KeyPress || e->type() == QEvent::KeyRelease)
	{
		const QKeyEvent* key = static_cast<QKeyEvent*>(e);
		if (key->key() == Qt::Key_Back) // sonst schliesst Android die App sofort
		{
			// Schon beim Druecken reagieren: bei der Gesten-Navigation (Android 13+) kommt das
			// Loslassen nicht immer an. Ist die Rueckfrage offen, wirkt Zurueck wie "Abbrechen".
			if (e->type() == QEvent::KeyPress && !key->isAutoRepeat())
			{
				qDebug() << "Zurueck-Taste";
				if (overlay->isOpen())
					overlay->cancel();
				else if (lobby->isOpen())
					lobby->cancel();
				else
					QTimer::singleShot(0, this, &ReversiGui::confirmQuit);
			}
			return true;
		}
	}
	return QWidget::eventFilter(object, e);
}

void ReversiGui::closeEvent(QCloseEvent* e)
{
	qDebug() << "closeEvent, bestaetigt:" << quitConfirmed;
#ifdef Q_OS_ANDROID
	if (!quitConfirmed) // falls Android das Fenster trotzdem schliessen will: erst nachfragen
	{
		e->ignore();
		QTimer::singleShot(0, this, &ReversiGui::confirmQuit);
		return;
	}
#endif
	e->accept();
}

void ReversiGui::confirmQuit()
{
	if (!overlay->isOpen())
		overlay->show({ QStringLiteral("Spiel wirklich beenden?"), QString(), QStringLiteral("Beenden") }, [this]() { quit(); });
}

void ReversiGui::quit()
{
	quitConfirmed = true;
	if (mode == Mode::Bluetooth)
		link->sendAndClose(QStringLiteral("BYE"));
	qApp->quit();
}
