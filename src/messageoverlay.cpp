#include "messageoverlay.h"
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QBoxLayout>
#include <QFrame>
#include <QPainter>
#include <QMouseEvent>

MessageOverlay::MessageOverlay(QWidget* parent) : QWidget(parent)
{
	QFrame* panel = new QFrame(this);
	panel->setObjectName(QStringLiteral("messagePanel"));
	// feste Farben: lesbar in hellem und dunklem Design
	setStyleSheet(QStringLiteral(
		"QFrame#messagePanel { background: #fafafa; border-radius: 14px; }"
		"QFrame#messagePanel QLabel, QFrame#messagePanel QCheckBox { color: #202020; }"
		"QPushButton#messageNo { background: #757575; }"
		"QPushButton#messageNo:pressed { background: #505050; }"));

	titleLabel = new QLabel(panel);
	detailLabel = new QLabel(panel);
	for (QLabel* l : { titleLabel, detailLabel })
	{
		l->setWordWrap(true);
		l->setAlignment(Qt::AlignCenter);
	}
	QFont f = titleLabel->font();
	f.setBold(true);
	titleLabel->setFont(f);

	yesButton = new QPushButton(panel);
	noButton = new QPushButton(panel);
	noButton->setObjectName(QStringLiteral("messageNo"));
	const int touch = (int)(QFontMetrics(font()).height() * 2.6);
	for (QPushButton* b : { yesButton, noButton })
		b->setMinimumHeight(touch);

	QHBoxLayout* buttons = new QHBoxLayout();
	buttons->setSpacing(12);
	buttons->addWidget(noButton, 1);
	buttons->addWidget(yesButton, 1);

	QVBoxLayout* panelLayout = new QVBoxLayout(panel);
	panelLayout->setContentsMargins(20, 20, 20, 20);
	panelLayout->setSpacing(18);
	panelLayout->addStretch(1);
	panelLayout->addWidget(titleLabel);
	panelLayout->addWidget(detailLabel);
	optionLayout = new QVBoxLayout();
	optionLayout->setSpacing(4);
	panelLayout->addLayout(optionLayout);
	panelLayout->addStretch(1);
	panelLayout->addLayout(buttons);

	outer = new QVBoxLayout(this);
	outer->addStretch(1);
	outer->addWidget(panel);
	outer->addStretch(1);

	connect(noButton, &QPushButton::clicked, this, &MessageOverlay::cancel);
	connect(yesButton, &QPushButton::clicked, this, [this]() {
		hide();
		if (yesAction)
			yesAction();
	});

	parent->installEventFilter(this);
	setGeometry(parent->rect());
	hide();
}

void MessageOverlay::show(const Message& m, std::function<void()> onYes)
{
	titleLabel->setText(m.title);
	detailLabel->setText(m.detail);
	detailLabel->setVisible(!m.detail.isEmpty());
	yesButton->setText(m.yesText);
	noButton->setText(m.noText);
	yesAction = std::move(onYes);

	for (QCheckBox* box : optionBoxes)
		box->deleteLater();
	optionBoxes.clear();
	const int touch = (int)(QFontMetrics(font()).height() * 2.0);
	for (const auto& [text, checked] : m.options)
	{
		QCheckBox* box = new QCheckBox(text, this);
		box->setChecked(checked);
		box->setMinimumHeight(touch);
		optionLayout->addWidget(box);
		optionBoxes.push_back(box);
	}

	// Vollflaechig: Kasten fuellt das Fenster, Titel deutlich groesser
	const int margin = m.fullScreen ? 12 : 24;
	outer->setContentsMargins(margin, margin, margin, margin);
	outer->setStretch(0, m.fullScreen ? 0 : 1);
	outer->setStretch(1, m.fullScreen ? 1 : 0);
	outer->setStretch(2, m.fullScreen ? 0 : 1);
	QFont f = font();
	f.setBold(true);
	f.setPointSizeF(f.pointSizeF() * (m.fullScreen ? 2.4 : 1.15));
	titleLabel->setFont(f);
	QFont d = font();
	d.setPointSizeF(d.pointSizeF() * (m.fullScreen ? 1.3 : 1.0));
	detailLabel->setFont(d);

	setGeometry(parentWidget()->rect());
	raise();
	QWidget::show();
}

bool MessageOverlay::isOptionChecked(int index) const
{
	return index >= 0 && index < (int)optionBoxes.size() && optionBoxes[index]->isChecked();
}

void MessageOverlay::cancel()
{
	hide();
	yesAction = nullptr;
}

bool MessageOverlay::eventFilter(QObject* watched, QEvent* e)
{
	if (watched == parentWidget() && e->type() == QEvent::Resize)
		setGeometry(parentWidget()->rect());
	return QWidget::eventFilter(watched, e);
}

void MessageOverlay::paintEvent(QPaintEvent*)
{
	QPainter p(this);
	p.fillRect(rect(), QColor(0, 0, 0, 140)); // Spiel dahinter abdunkeln
}

void MessageOverlay::mousePressEvent(QMouseEvent* e)
{
	e->accept();
}
