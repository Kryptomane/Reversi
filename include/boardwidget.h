#pragma once
// Spielbrett-Widget: zeichnet mit QPainter (skaliert auf jede Groesse) und meldet angetippte Felder.
#include <QWidget>
#include "bitboard.h"

class BoardWidget : public QWidget
{
	Q_OBJECT

public:
	explicit BoardWidget(QWidget* parent = nullptr);

	// markers = Felder mit Zugpunkt, hint = Feldindex fuer den Tipp-Rahmen (-1 = keiner)
	void setPosition(bb::u64 black, bb::u64 white, bb::u64 markers, int hint);

	QSize sizeHint() const override { return QSize(400, 400); }
	QSize minimumSizeHint() const override { return QSize(160, 160); }

signals:
	void fieldClicked(int x, int y);

protected:
	void paintEvent(QPaintEvent* e) override;
	void mousePressEvent(QMouseEvent* e) override; // Touch kommt unter Android als Mausklick an

private:
	QRectF boardRect() const; // groesstes zentriertes Quadrat im Widget

	bb::u64 m_black = 0, m_white = 0, m_markers = 0;
	int m_hint = -1;
};
