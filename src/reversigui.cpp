#include "reversigui.h"
#include "ui_reversigui.h"
#include <QApplication>
#include <QCloseEvent>
#include <QKeyEvent>
#include <QMessageBox>
#include <QSettings>
#include <QTimer>
#include <QDebug>

ReversiGui::ReversiGui(QWidget* parent) : QWidget(parent), ui(new Ui::ReversiGui)
{
	ui->setupUi(this);
#ifndef Q_OS_ANDROID
	resize(460, 860); // Desktop: Hochformat wie am Handy, frei skalierbar
#endif
	setupModeBox();
	setupLevelBox();
	ui->hintBox->setChecked(QSettings().value("hint", false).toBool());
	updateHintLabel(nullptr);

	connect(ui->board, &BoardWidget::fieldClicked, this, &ReversiGui::onFieldClicked);
	connect(ui->newGameButton, &QPushButton::clicked, this, &ReversiGui::startNewGame);
	connect(ui->exitGameButton, &QPushButton::clicked, this, &ReversiGui::quit);
	connect(ui->hintBox, &QCheckBox::toggled, this, &ReversiGui::onHintToggled);
	qApp->installEventFilter(this);

	startNewGame();
}

ReversiGui::~ReversiGui()
{
	stopSearch();
	delete ui;
}

// ======================= Ablauf =======================

void ReversiGui::startNewGame()
{
	stopSearch();
	game.reset();
	ai.clear();
	lastResultInfo.clear();
	handleTurn(Game::Turn::Next);
}

void ReversiGui::onFieldClicked(int x, int y)
{
	const int square = bb::square(x, y);
	if (!humanToMove() || !game.isLegal(square))
		return;
	if (hintRunning)
		stopSearch(); // Tipp wird nicht mehr gebraucht
	handleTurn(game.play(square));
}

void ReversiGui::handleTurn(Game::Turn turn)
{
	positionId++;
	hintSquare = -1;
	updateHintLabel(nullptr);
	updateView();

	if (game.isOver())
	{
		showStatus(gameOverText());
		return;
	}
	passInfo = turn == Game::Turn::OpponentPasses
		? colorName(opponent(game.toMove())) + QStringLiteral(" muss passen. ") : QString();

	if (isComputer(game.toMove()))
	{
		startSearch(SearchKind::ComputerMove);
		showStatus(passInfo + QStringLiteral("PC denkt ..."));
	}
	else
	{
		showStatus(passInfo + yourTurnText() + lastResultInfo);
		if (ui->hintBox->isChecked())
			startSearch(SearchKind::Hint);
	}
}

// ======================= Engine im Hintergrund =======================

void ReversiGui::startSearch(SearchKind kind)
{
	stopSearch(); // hoechstens eine Suche gleichzeitig
	hintRunning = kind == SearchKind::Hint;
	computerThinking = !hintRunning;

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
	if (kind == SearchKind::Hint)
		showHint(r);
	else if (computerThinking)
		showStatus(passInfo + QStringLiteral("PC denkt ... Tiefe ") + QString::number(r.depth));
}

void ReversiGui::onSearchResult(const engine::Result& r, int forPosition, SearchKind kind)
{
	if (forPosition != positionId || game.isOver() || r.move < 0)
		return; // Ergebnis fuer eine Stellung, die es nicht mehr gibt

	if (kind == SearchKind::Hint)
	{
		hintRunning = false;
		if (!r.stopped)
			showHint(r);
		return;
	}

	computerThinking = false;
	qDebug() << "PC-Zug" << r.move / 8 << r.move % 8 << "Tiefe" << r.depth << "Wert" << r.score
	         << (r.exact ? "exakt" : r.wld ? "WLD" : "") << "Knoten" << r.nodes << "Zeit" << r.seconds;
	lastResultInfo = provenResultText(r);
	handleTurn(game.play(r.move));
}

void ReversiGui::showHint(const engine::Result& r)
{
	if (!ui->hintBox->isChecked() || !humanToMove())
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
}

// ======================= Einstellungen =======================

void ReversiGui::setupModeBox()
{
	ui->modeBox->addItem(QStringLiteral("Computer"), (int)Mode::VsComputer);
	ui->modeBox->addItem(QStringLiteral("Mensch (2 Spieler)"), (int)Mode::TwoPlayers);
	mode = QSettings().value("mode", 0).toInt() == (int)Mode::TwoPlayers ? Mode::TwoPlayers : Mode::VsComputer;
	ui->modeBox->setCurrentIndex(ui->modeBox->findData((int)mode));
	ui->levelBox->setEnabled(mode == Mode::VsComputer);
	connect(ui->modeBox, &QComboBox::currentIndexChanged, this, &ReversiGui::onModeChanged);
}

void ReversiGui::setupLevelBox()
{
	static const char16_t* const names[engine::LEVELS] = {
		u"Anf\u00E4nger", u"Leicht", u"Leicht+", u"Mittel", u"Mittel+", u"Fortgeschritten",
		u"Stark", u"Sehr stark", u"Experte", u"Meister", u"Gro\u00DFmeister", u"Weltklasse" };
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
void ReversiGui::onModeChanged()
{
	mode = (Mode)ui->modeBox->currentData().toInt();
	QSettings().setValue("mode", (int)mode);
	ui->levelBox->setEnabled(mode == Mode::VsComputer);
	stopSearch();
	lastResultInfo.clear();
	handleTurn(Game::Turn::Next);
}

int ReversiGui::level() const
{
	return ui->levelBox->currentData().toInt();
}

void ReversiGui::onHintToggled(bool on)
{
	QSettings().setValue("hint", on);
	if (on && humanToMove() && hintSquare < 0 && !hintRunning)
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
	const bb::u64 markers = humanToMove() ? game.legalMoves() : 0;
	ui->board->setPosition(game.discs(Color::Black), game.discs(Color::White), markers, hintSquare);

	ui->BlackChipsLabel->setText(QStringLiteral("\u25CF Schwarz  ") + QString::number(game.count(Color::Black)));
	ui->WhiteChipsLabel->setText(QStringLiteral("\u25CB Wei\u00DF  ") + QString::number(game.count(Color::White)));
	const QString active = QStringLiteral("background: #c8e6c9;"), inactive = QStringLiteral("background: transparent;");
	const bool running = !game.isOver();
	ui->BlackChipsLabel->setStyleSheet(running && game.toMove() == Color::Black ? active : inactive);
	ui->WhiteChipsLabel->setStyleSheet(running && game.toMove() == Color::White ? active : inactive);
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
	return QStringLiteral("Du bist dran (") + colorName(game.toMove()) + ").";
}

QString ReversiGui::gameOverText() const
{
	const int b = game.count(Color::Black), w = game.count(Color::White);
	const QString score = QString::number(b) + ":" + QString::number(w);
	if (b == w)
		return QStringLiteral("Unentschieden ") + score;
	return colorName(b > w ? Color::Black : Color::White) + QStringLiteral(" gewinnt ") + score;
}

// Nur bewiesene Ergebnisse (Partie bis zum Ende durchgerechnet) werden angezeigt.
QString ReversiGui::provenResultText(const engine::Result& r)
{
	if (!r.exact && !r.wld)
		return QString();
	if (r.score > 0) return QStringLiteral("\nDer PC gewinnt bei bestem Spiel.");
	if (r.score < 0) return QStringLiteral("\nDu kannst noch gewinnen!");
	return QStringLiteral("\nRemis bei bestem Spiel.");
}

// ======================= Beenden =======================

bool ReversiGui::eventFilter(QObject* object, QEvent* e)
{
	if (e->type() == QEvent::KeyPress || e->type() == QEvent::KeyRelease)
	{
		const QKeyEvent* key = static_cast<QKeyEvent*>(e);
		if (key->key() == Qt::Key_Back) // sonst schliesst Android die App sofort
		{
			if (e->type() == QEvent::KeyRelease && !key->isAutoRepeat())
				QTimer::singleShot(0, this, &ReversiGui::confirmQuit); // Dialog nicht im Filter oeffnen
			return true;
		}
	}
	return QWidget::eventFilter(object, e);
}

void ReversiGui::closeEvent(QCloseEvent* e)
{
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
	if (quitDialogOpen)
		return;
	quitDialogOpen = true;
	const auto answer = QMessageBox::question(this, QStringLiteral("Reversi"), QStringLiteral("Spiel wirklich beenden?"),
	                                          QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
	quitDialogOpen = false;
	if (answer == QMessageBox::Yes)
		quit();
}

void ReversiGui::quit()
{
	quitConfirmed = true;
	qApp->quit();
}
