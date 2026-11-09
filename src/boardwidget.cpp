#include "boardwidget.h"
#include <QPainter>
#include <QMouseEvent>
#include <QVariantAnimation>
#include <algorithm>
#include <cmath>

namespace
{
	const QColor BOARD_GREEN(0, 150, 0);
	const QColor GRID_GREEN(0, 85, 0);
	const QColor FRAME_GREEN(0, 60, 0);
	const QColor BLACK_STONE(20, 20, 20);
	const QColor WHITE_STONE(245, 245, 245);
	const QColor HINT_YELLOW(255, 215, 0);
	const QColor LAST_MOVE_RED(230, 30, 30);
	const QColor FLIPPED_ORANGE(255, 140, 0);

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

	// widthScale < 1 laesst den Stein schmaler erscheinen (Drehung um die senkrechte Achse).
	void drawStones(QPainter& p, const QRectF& r, double cell, bb::u64 stones, const QColor& fill, double widthScale = 1.0)
	{
		p.setPen(QPen(QColor(40, 40, 40), std::max(1.0, cell / 50.0)));
		p.setBrush(fill);
		for (; stones; stones &= stones - 1)
			p.drawEllipse(cellCenter(r, cell, bb::lsb(stones)), 0.40 * cell * widthScale, 0.40 * cell);
	}

	void drawMarkers(QPainter& p, const QRectF& r, double cell, bb::u64 markers)
	{
		p.setPen(Qt::NoPen);
		p.setBrush(QColor(0, 0, 0, 90));
		for (; markers; markers &= markers - 1)
			p.drawEllipse(cellCenter(r, cell, bb::lsb(markers)), 0.11 * cell, 0.11 * cell);
	}

	// Duenner Ring direkt um einen Stein (letzter Zug, umgedrehte Steine).
	void drawStoneRing(QPainter& p, const QRectF& r, double cell, int square, const QColor& color)
	{
		QPen pen(color);
		pen.setWidthF(std::max(1.5, 0.035 * cell));
		p.setPen(pen);
		p.setBrush(Qt::NoBrush);
		p.drawEllipse(cellCenter(r, cell, square), 0.45 * cell, 0.45 * cell);
	}

	void drawSquareFrame(QPainter& p, const QRectF& r, double cell, int square, const QColor& color)
	{
		const double inset = 0.06 * cell;
		const QPointF c = cellCenter(r, cell, square);
		const QRectF box(c.x() - cell / 2 + inset, c.y() - cell / 2 + inset, cell - 2 * inset, cell - 2 * inset);
		QPen pen(color);
		pen.setWidthF(std::max(2.0, 0.07 * cell));
		p.setPen(pen);
		p.setBrush(Qt::NoBrush);
		p.drawRoundedRect(box, 0.12 * cell, 0.12 * cell);
	}
}

BoardWidget::BoardWidget(QWidget* parent) : QWidget(parent), m_animation(new QVariantAnimation(this))
{
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	m_animation->setDuration(FLIP_ANIMATION_MS);
	m_animation->setStartValue(0.0);
	m_animation->setEndValue(1.0);
	connect(m_animation, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
		m_flipProgress = v.toDouble();
		update();
	});
	connect(m_animation, &QVariantAnimation::finished, this, [this]() {
		m_flipping = 0;
		update();
	});
}

void BoardWidget::setState(const BoardState& s, bool animateFlips)
{
	if (animateFlips)
	{
		const bb::u64 flipped = (m_state.black & s.white) | (m_state.white & s.black);
		if (flipped)
		{
			m_animation->stop();
			m_flipping = flipped;
			m_flipProgress = 0.0;
			m_animation->start();
		}
	}
	else if (s.black != m_state.black || s.white != m_state.white)
	{
		m_animation->stop(); // Brett ohne Zug geaendert (z. B. neues Spiel): Animation verwerfen
		m_flipping = 0;
	}
	m_state = s;
	update();
}

int BoardWidget::animationRemainingMs() const
{
	return m_animation->state() == QAbstractAnimation::Running ? FLIP_ANIMATION_MS - m_animation->currentTime() : 0;
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
	drawStones(p, r, cell, m_state.black & ~m_flipping, BLACK_STONE);
	drawStones(p, r, cell, m_state.white & ~m_flipping, WHITE_STONE);
	if (m_flipping)
	{
		// erste Haelfte: alte Farbe wird schmaler, zweite Haelfte: neue Farbe wird wieder breiter
		const double scale = std::abs(std::cos(m_flipProgress * 3.14159265358979));
		const bool showNewColor = m_flipProgress >= 0.5;
		drawStones(p, r, cell, m_state.black & m_flipping, showNewColor ? BLACK_STONE : WHITE_STONE, scale);
		drawStones(p, r, cell, m_state.white & m_flipping, showNewColor ? WHITE_STONE : BLACK_STONE, scale);
	}
	drawMarkers(p, r, cell, m_state.markers);
	for (bb::u64 f = m_state.lastFlipped; f; f &= f - 1)
		drawStoneRing(p, r, cell, bb::lsb(f), FLIPPED_ORANGE);
	if (m_state.lastMove >= 0)
		drawStoneRing(p, r, cell, m_state.lastMove, LAST_MOVE_RED);
	if (m_state.hint >= 0)
		drawSquareFrame(p, r, cell, m_state.hint, HINT_YELLOW);

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
