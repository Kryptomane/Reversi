#include "checkboxstyle.h"
#include <QPainter>
#include <QPainterPath>
#include <QStyleOption>
#include <QWidget>
#include <algorithm>

int CheckBoxStyle::pixelMetric(PixelMetric metric, const QStyleOption* option, const QWidget* widget) const
{
	if (metric == PM_IndicatorWidth || metric == PM_IndicatorHeight)
	{
		const int lineHeight = widget ? widget->fontMetrics().height() : (option ? option->fontMetrics.height() : 16);
		return std::max(20, (int)(lineHeight * 1.35)); // etwas groesser als Standard
	}
	if (metric == PM_CheckBoxLabelSpacing)
		return QProxyStyle::pixelMetric(metric, option, widget) + 4;
	return QProxyStyle::pixelMetric(metric, option, widget);
}

void CheckBoxStyle::drawPrimitive(PrimitiveElement element, const QStyleOption* option, QPainter* painter,
                                  const QWidget* widget) const
{
	if (element != PE_IndicatorCheckBox)
	{
		QProxyStyle::drawPrimitive(element, option, painter, widget);
		return;
	}
	const bool checked = option->state & State_On;
	const bool enabled = option->state & State_Enabled;
	const QColor green = enabled ? QColor(46, 125, 50) : QColor(158, 158, 158);
	const QRectF box = QRectF(option->rect).adjusted(1.5, 1.5, -1.5, -1.5);
	const double w = box.width();

	painter->save();
	painter->setRenderHint(QPainter::Antialiasing, true);
	painter->setPen(QPen(checked ? green : QColor(117, 117, 117), std::max(1.5, w / 12.0)));
	painter->setBrush(checked ? green : QColor(255, 255, 255));
	painter->drawRoundedRect(box, w * 0.18, w * 0.18);
	if (checked)
	{
		QPainterPath tick; // weisses Haekchen
		tick.moveTo(box.left() + 0.22 * w, box.top() + 0.52 * w);
		tick.lineTo(box.left() + 0.42 * w, box.top() + 0.72 * w);
		tick.lineTo(box.left() + 0.78 * w, box.top() + 0.30 * w);
		QPen pen(QColor(255, 255, 255), std::max(2.0, w / 7.0));
		pen.setCapStyle(Qt::RoundCap);
		pen.setJoinStyle(Qt::RoundJoin);
		painter->setPen(pen);
		painter->setBrush(Qt::NoBrush);
		painter->drawPath(tick);
	}
	painter->restore();
}
