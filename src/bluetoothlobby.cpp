#include "bluetoothlobby.h"
#include "bluetoothlink.h"
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QBoxLayout>
#include <QFrame>
#include <QPainter>
#include <QMouseEvent>
#include <QScroller>

BluetoothLobby::BluetoothLobby(QWidget* parent, BluetoothLink* bluetooth) : QWidget(parent), link(bluetooth)
{
	QFrame* panel = new QFrame(this);
	panel->setObjectName(QStringLiteral("lobbyPanel"));
	// feste Farben wie MessageOverlay: lesbar in hellem und dunklem Design
	setStyleSheet(QStringLiteral(
		"QFrame#lobbyPanel { background: #fafafa; border-radius: 14px; }"
		"QFrame#lobbyPanel QLabel { color: #202020; }"
		"QListWidget { background: white; color: #202020; border: 1px solid #bdbdbd; border-radius: 8px; }"
		"QListWidget::item { padding: 10px; border-bottom: 1px solid #e0e0e0; }"
		"QListWidget::item:selected { background: #c8e6c9; color: #202020; }"
		"QPushButton#lobbyCancel { background: #757575; }"
		"QPushButton#lobbyCancel:pressed { background: #505050; }"
		"QPushButton#lobbyJoin, QPushButton#lobbyAgain { background: #1565c0; }"
		"QPushButton#lobbyJoin:pressed, QPushButton#lobbyAgain:pressed { background: #0d47a1; }"));

	titleLabel = new QLabel(QStringLiteral("Partie über Bluetooth"), panel);
	infoLabel = new QLabel(panel);
	for (QLabel* l : { titleLabel, infoLabel })
	{
		l->setWordWrap(true);
		l->setAlignment(Qt::AlignCenter);
	}
	QFont f = font();
	f.setBold(true);
	f.setPointSizeF(f.pointSizeF() * 1.15);
	titleLabel->setFont(f);

	deviceList = new QListWidget(panel);
	deviceList->setMinimumHeight(QFontMetrics(font()).height() * 10);
	QScroller::grabGesture(deviceList->viewport(), QScroller::LeftMouseButtonGesture); // Wischen am Handy

	hostButton = new QPushButton(QStringLiteral("Spiel eröffnen"), panel);
	joinButton = new QPushButton(QStringLiteral("Spiel beitreten"), panel);
	againButton = new QPushButton(QStringLiteral("Erneut suchen"), panel);
	cancelButton = new QPushButton(QStringLiteral("Abbrechen"), panel);
	joinButton->setObjectName(QStringLiteral("lobbyJoin"));
	againButton->setObjectName(QStringLiteral("lobbyAgain"));
	cancelButton->setObjectName(QStringLiteral("lobbyCancel"));
	const int touch = (int)(QFontMetrics(font()).height() * 2.6);
	for (QPushButton* b : { hostButton, joinButton, againButton, cancelButton })
		b->setMinimumHeight(touch);

	QVBoxLayout* panelLayout = new QVBoxLayout(panel);
	panelLayout->setContentsMargins(20, 20, 20, 20);
	panelLayout->setSpacing(14);
	panelLayout->addWidget(titleLabel);
	panelLayout->addWidget(infoLabel);
	panelLayout->addWidget(deviceList, 1);
	panelLayout->addWidget(hostButton);
	panelLayout->addWidget(joinButton);
	panelLayout->addWidget(againButton);
	panelLayout->addWidget(cancelButton);

	QVBoxLayout* outer = new QVBoxLayout(this);
	outer->setContentsMargins(24, 24, 24, 24);
	outer->addStretch(1);
	outer->addWidget(panel, 3);
	outer->addStretch(1);

	connect(hostButton, &QPushButton::clicked, this, &BluetoothLobby::startHosting);
	connect(joinButton, &QPushButton::clicked, this, &BluetoothLobby::startJoining);
	connect(againButton, &QPushButton::clicked, this, &BluetoothLobby::startJoining);
	connect(cancelButton, &QPushButton::clicked, this, &BluetoothLobby::cancel);
	connect(deviceList, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
		if (page != Page::Joining)
			return;
		const QString name = item->text();
		showPage(Page::Waiting, QStringLiteral("Verbinde mit ") + name + QStringLiteral(" …\n\n"
			"Beim ersten Mal fragen beide Geräte nach dem Koppeln – bitte bestätigen."));
		link->join(item->data(Qt::UserRole).toString(), name);
	});
	connect(link, &BluetoothLink::deviceFound, this, [this](const QString& name, const QString& address) {
		if (page != Page::Joining)
			return;
		for (int i = 0; i < deviceList->count(); i++)
			if (deviceList->item(i)->data(Qt::UserRole).toString() == address)
			{
				deviceList->item(i)->setText(name); // Name kommt manchmal erst spaeter
				return;
			}
		QListWidgetItem* item = new QListWidgetItem(name, deviceList);
		item->setData(Qt::UserRole, address);
		infoLabel->setText(QStringLiteral("Tippe auf das Gerät deines Mitspielers.\nSuche läuft …"));
	});
	connect(link, &BluetoothLink::searchFinished, this, [this]() {
		if (page != Page::Joining)
			return;
		infoLabel->setText(deviceList->count() > 0
			? QStringLiteral("Tippe auf das Gerät deines Mitspielers.")
			: QStringLiteral("Kein Gerät gefunden. Hat dein Mitspieler „Spiel eröffnen“ gewählt?"));
		againButton->setVisible(true);
	});
	connect(link, &BluetoothLink::failed, this, &BluetoothLobby::onFailed);

	parent->installEventFilter(this);
	setGeometry(parent->rect());
	hide();
}

void BluetoothLobby::open()
{
	showPage(Page::Waiting, QStringLiteral("Bluetooth wird vorbereitet …"));
	setGeometry(parentWidget()->rect());
	raise();
	show();
	link->prepare([this](bool ok, const QString& message) {
		if (!isVisible())
			return;
		if (ok)
			showPage(Page::Choice, QStringLiteral("Ein Handy eröffnet das Spiel, das andere tritt bei.\n"
				"Wer eröffnet, spielt Schwarz."));
		else
		{
			showPage(Page::Choice, message); // z. B. Berechtigung fehlt: nur "Abbrechen" anbieten
			hostButton->setVisible(false);
			joinButton->setVisible(false);
		}
	});
}

void BluetoothLobby::showPage(Page p, const QString& info)
{
	page = p;
	infoLabel->setText(info);
	deviceList->setVisible(p == Page::Joining);
	hostButton->setVisible(p == Page::Choice);
	joinButton->setVisible(p == Page::Choice);
	againButton->setVisible(false);
}

void BluetoothLobby::startHosting()
{
	const QString me = link->localName();
	showPage(Page::Hosting, QStringLiteral("Warte auf Mitspieler …\n\n")
		+ (me.isEmpty() ? QString() : QStringLiteral("Dein Gerät heißt „") + me + QStringLiteral("“.\n"))
		+ QStringLiteral("Auf dem anderen Handy „Spiel beitreten“ wählen und dieses Gerät antippen."));
	link->host();
}

void BluetoothLobby::startJoining()
{
	deviceList->clear();
	showPage(Page::Joining, QStringLiteral("Suche Geräte in der Nähe …"));
	link->startSearch();
}

void BluetoothLobby::onFailed(const QString& message)
{
	if (!isVisible())
		return;
	link->close();
	showPage(Page::Choice, message);
}

void BluetoothLobby::dismiss()
{
	hide();
}

void BluetoothLobby::cancel()
{
	if (!isVisible())
		return;
	link->close();
	hide();
	emit cancelled();
}

bool BluetoothLobby::eventFilter(QObject* watched, QEvent* e)
{
	if (watched == parentWidget() && e->type() == QEvent::Resize)
		setGeometry(parentWidget()->rect());
	return QWidget::eventFilter(watched, e);
}

void BluetoothLobby::paintEvent(QPaintEvent*)
{
	QPainter p(this);
	p.fillRect(rect(), QColor(0, 0, 0, 140));
}

void BluetoothLobby::mousePressEvent(QMouseEvent* e)
{
	e->accept();
}
