#include "boardwidget.h"
#include <QPainter>
#include <QMouseEvent>
#include <algorithm>

void drawBoard(QPainter& p, const QRectF& r, const board& m, int hint)
{
	p.save();
	p.setRenderHint(QPainter::Antialiasing, true);
	p.fillRect(r, QColor(0, 150, 0));
	const double cell = r.width() / BSIZE;

	// duenne gestrichelte Linien zwischen den Feldern (horizontal und vertikal)
	QPen grid(QColor(0, 85, 0));
	grid.setStyle(Qt::DashLine);
	grid.setWidthF(std::max(1.0, cell / 45.0));
	p.setPen(grid);
	for (int k = 1; k < BSIZE; k++)
	{
		const double x = r.left() + k * cell;
		const double y = r.top() + k * cell;
		p.drawLine(QPointF(x, r.top()), QPointF(x, r.bottom()));
		p.drawLine(QPointF(r.left(), y), QPointF(r.right(), y));
	}

	// Steine und Zugmarker
	const QPen stoneEdge(QColor(40, 40, 40), std::max(1.0, cell / 50.0));
	for (int i = 0; i < BSIZE; i++)
	{
		for (int j = 0; j < BSIZE; j++)
		{
			const QPointF c(r.left() + (i + 0.5) * cell, r.top() + (j + 0.5) * cell);
			const unsigned char v = m[i][j];
			if (v == 1 || v == 2)
			{
				p.setPen(stoneEdge);
				p.setBrush(v == 1 ? QColor(20, 20, 20) : QColor(245, 245, 245));
				p.drawEllipse(c, 0.40 * cell, 0.40 * cell);
			}
			else if (v == 3 || v == 4)
			{
				p.setPen(Qt::NoPen);
				p.setBrush(QColor(0, 0, 0, 90));
				p.drawEllipse(c, 0.11 * cell, 0.11 * cell);
			}
		}
	}

	// empfohlener Zug: gelber Rahmen
	if (hint >= 0 && hint < BSIZE * BSIZE)
	{
		const int hx = hint / BSIZE, hy = hint % BSIZE;
		const double inset = 0.08 * cell;
		const QRectF hr(r.left() + hx * cell + inset, r.top() + hy * cell + inset, cell - 2 * inset, cell - 2 * inset);
		QPen hp(QColor(255, 215, 0));
		hp.setWidthF(std::max(2.0, 0.07 * cell));
		p.setPen(hp);
		p.setBrush(Qt::NoBrush);
		p.drawRoundedRect(hr, 0.12 * cell, 0.12 * cell);
	}

	// Rahmen ums Brett
	p.setPen(QPen(QColor(0, 60, 0), std::max(1.0, cell / 30.0)));
	p.setBrush(Qt::NoBrush);
	p.drawRect(r);
	p.restore();
}

BoardWidget::BoardWidget(QWidget* parent) : QWidget(parent)
{
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void BoardWidget::setBoard(const board& m, int hint)
{
	m_board = m;
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
	drawBoard(p, boardRect(), m_board, m_hint);
}

void BoardWidget::mousePressEvent(QMouseEvent* e)
{
	const QRectF br = boardRect();
	const QPointF pos = e->position();
	if (!br.contains(pos))
		return;
	const int x = std::clamp((int)((pos.x() - br.left()) / br.width() * BSIZE), 0, BSIZE - 1);
	const int y = std::clamp((int)((pos.y() - br.top()) / br.height() * BSIZE), 0, BSIZE - 1);
	emit fieldClicked(x, y);
}
