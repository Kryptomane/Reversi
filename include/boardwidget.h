#pragma once
// Spielbrett als eigenes Widget: zeichnet mit QPainter (skaliert auf jede Groesse, scharf auf
// hochaufloesenden Displays) und meldet angeklickte/angetippte Felder.
#include <QWidget>
#include "reversi_core.h"

class QPainter;

// Zeichnet das Brett in das Quadrat r. hint = Feldindex x*8+y fuer den Tipp-Rahmen, -1 = keiner.
void drawBoard(QPainter& p, const QRectF& r, const board& m, int hint);

class BoardWidget : public QWidget
{
	Q_OBJECT

public:
	explicit BoardWidget(QWidget* parent = nullptr);
	void setBoard(const board& m, int hint);
	QSize sizeHint() const override { return QSize(400, 400); }
	QSize minimumSizeHint() const override { return QSize(160, 160); }

signals:
	void fieldClicked(int x, int y);

protected:
	void paintEvent(QPaintEvent* e) override;
	void mousePressEvent(QMouseEvent* e) override; // Touch kommt unter Android als Mausklick an

private:
	QRectF boardRect() const; // groesstes zentriertes Quadrat im Widget
	board m_board;
	int m_hint = -1;
};
