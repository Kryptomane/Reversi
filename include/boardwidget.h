#pragma once
// Spielbrett-Widget: zeichnet mit QPainter (skaliert auf jede Groesse), meldet angetippte Felder
// und kann das Umdrehen der Steine animieren.
#include <QWidget>
#include "bitboard.h"

class QVariantAnimation;

// Alles, was auf dem Brett zu sehen ist.
struct BoardState
{
	bb::u64 black = 0, white = 0;
	bb::u64 markers = 0;              // Felder mit Zugpunkt
	int hint = -1;                    // Zugvorschlag (gelber Rahmen), -1 = keiner
	int humanMove = -1;               // letzter Zug eines Menschen (blauer Rahmen), -1 = keiner
	int computerMove = -1;            // letzter Zug des PCs (roter Rahmen), -1 = keiner
};

class BoardWidget : public QWidget
{
	Q_OBJECT

public:
	static constexpr int FLIP_ANIMATION_MS = 500;

	explicit BoardWidget(QWidget* parent = nullptr);

	// animateFlips: Steine, die die Farbe gewechselt haben, drehen sich sichtbar um.
	void setState(const BoardState& s, bool animateFlips);
	int animationRemainingMs() const;

	QSize sizeHint() const override { return QSize(400, 400); }
	QSize minimumSizeHint() const override { return QSize(160, 160); }

signals:
	void fieldClicked(int x, int y);

protected:
	void paintEvent(QPaintEvent* e) override;
	void mousePressEvent(QMouseEvent* e) override; // Touch kommt unter Android als Mausklick an

private:
	QRectF boardRect() const; // groesstes zentriertes Quadrat im Widget

	BoardState m_state;
	bb::u64 m_flipping = 0;        // Steine, die gerade umgedreht werden
	double m_flipProgress = 1.0;   // 0 = Anfang, 1 = fertig
	QVariantAnimation* m_animation;
};
