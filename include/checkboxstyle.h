#pragma once
// Eigene Darstellung der Ankreuzfelder: gut sichtbar auf jedem Geraet und in hellem wie dunklem
// Design (der Android-Stil zeichnet das Haekchen sonst teils unsichtbar). Etwas groesser fuer Finger.
#include <QProxyStyle>

class CheckBoxStyle : public QProxyStyle
{
public:
	using QProxyStyle::QProxyStyle;

	int pixelMetric(PixelMetric metric, const QStyleOption* option = nullptr, const QWidget* widget = nullptr) const override;
	void drawPrimitive(PrimitiveElement element, const QStyleOption* option, QPainter* painter,
	                   const QWidget* widget = nullptr) const override;
};
