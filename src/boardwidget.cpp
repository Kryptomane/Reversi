#include "boardwidget.h"
#include <QPainter>
#include <QMouseEvent>
#include <algorithm>

namespace
{
	const QColor BOARD_GREEN(0, 150, 0);
	const QColor GRID_GREEN(0, 85, 0);
	const QColor FRAME_GREEN(0, 60, 0);
	const QColor HINT_YELLOW(255, 215, 0);

	QPointF cellCenter(const QRectF& r, double cell, int square)
	{
		return QPointF(r.left() + (square / bb::SIZE + 0.5) * cell, r.top() + (square % bb::SIZE + 0.5) * cell);
	}

	void drawGrid(QPainter& p, const QRectF& r, double cell)
	{
		QPen pen(GRID_GREEN);
		pen.setStyle(Qt::DashLine);
		pen.setWidthF(std::max(1.0, cell / 45.0));
		p.setPen(pen);
		for (int k = 1; k < bb::SIZE; k++)
		{
			const double x = r.left() + k * cell, y = r.top() + k * cell;
			p.drawLine(QPointF(x, r.top()), QPointF(x, r.bottom()));
			p.drawLine(QPointF(r.left(), y), QPointF(r.right(), y));
		}
	}

	void drawStones(QPainter& p, const QRectF& r, double cell, bb::u64 stones, const QColor& fill)
	{
		p.setPen(QPen(QColor(40, 40, 40), std::max(1.0, cell / 50.0)));
		p.setBrush(fill);
		for (; stones; stones &= stones - 1)
			p.drawEllipse(cellCenter(r, cell, bb::lsb(stones)), 0.40 * cell, 0.40 * cell);
	}

	void drawMarkers(QPainter& p, const QRectF& r, double cell, bb::u64 markers)
	{
		p.setPen(Qt::NoPen);
		p.setBrush(QColor(0, 0, 0, 90));
		for (; markers; markers &= markers - 1)
			p.drawEllipse(cellCenter(r, cell, bb::lsb(markers)), 0.11 * cell, 0.11 * cell);
	}

	void drawHint(QPainter& p, const QRectF& r, double cell, int square)
	{
		const double inset = 0.08 * cell;
		const QPointF c = cellCenter(r, cell, square);
		const QRectF box(c.x() - cell / 2 + inset, c.y() - cell / 2 + inset, cell - 2 * inset, cell - 2 * inset);
		QPen pen(HINT_YELLOW);
		pen.setWidthF(std::max(2.0, 0.07 * cell));
		p.setPen(pen);
		p.setBrush(Qt::NoBrush);
		p.drawRoundedRect(box, 0.12 * cell, 0.12 * cell);
	}
}

BoardWidget::BoardWidget(QWidget* parent) : QWidget(parent)
{
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void BoardWidget::setPosition(bb::u64 black, bb::u64 white, bb::u64 markers, int hint)
{
	m_black = black;
	m_white = white;
	m_markers = markers;
	m_hint = hint;
	update();
}

QRectF BoardWidget::boardRect() const
{
	const double side = std::max(1, std::min(width(), height()) - 2);
	return QRectF((width() - side) / 2.0, (height() - side) / 2.0, side, side);
}

void BoardWidget::paintEvent(QPaintEvent*)
{
	QPainter p(this);
	p.setRenderHint(QPainter::Antialiasing, true);
	const QRectF r = boardRect();
	const double cell = r.width() / bb::SIZE;

	p.fillRect(r, BOARD_GREEN);
	drawGrid(p, r, cell);
	drawStones(p, r, cell, m_black, QColor(20, 20, 20));
	drawStones(p, r, cell, m_white, QColor(245, 245, 245));
	drawMarkers(p, r, cell, m_markers);
	if (m_hint >= 0)
		drawHint(p, r, cell, m_hint);

	p.setPen(QPen(FRAME_GREEN, std::max(1.0, cell / 30.0)));
	p.setBrush(Qt::NoBrush);
	p.drawRect(r);
}

void BoardWidget::mousePressEvent(QMouseEvent* e)
{
	const QRectF r = boardRect();
	const QPointF pos = e->position();
	if (!r.contains(pos))
		return;
	const int x = std::clamp((int)((pos.x() - r.left()) / r.width() * bb::SIZE), 0, bb::SIZE - 1);
	const int y = std::clamp((int)((pos.y() - r.top()) / r.height() * bb::SIZE), 0, bb::SIZE - 1);
	emit fieldClicked(x, y);
}
