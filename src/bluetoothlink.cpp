#include "bluetoothlink.h"
#include <QTimer>

#ifdef REVERSI_BLUETOOTH
#include <QCoreApplication>
#include <QBluetoothServer>
#include <QBluetoothSocket>
#include <QBluetoothDeviceDiscoveryAgent>
#include <QBluetoothDeviceInfo>
#include <QBluetoothServiceInfo>
#include <QBluetoothLocalDevice>
#include <QBluetoothAddress>
#include <QBluetoothUuid>
#include <QUuid>
#include <QDebug>
#if QT_CONFIG(permissions)
#include <QPermissions>
#endif

namespace {
// Eigene Dienst-Kennung: beide Geraete muessen dieselbe verwenden.
QBluetoothUuid serviceUuid()
{
	return QBluetoothUuid(QUuid(QStringLiteral("{7d3c1e2a-5b4f-4c8e-9a61-2f0b8e5d4c17}")));
}
}

bool BluetoothLink::compiledIn() { return true; }

BluetoothLink::BluetoothLink(QObject* parent) : QObject(parent)
{
}

BluetoothLink::~BluetoothLink()
{
	close();
	delete service;
}

void BluetoothLink::prepare(std::function<void(bool, const QString&)> done)
{
	prepareDone = std::move(done);
	auto finish = [this](bool ok, const QString& message) {
		if (powerTimer)
			powerTimer->stop();
		if (auto f = std::move(prepareDone))
		{
			prepareDone = nullptr;
			f(ok, message);
		}
	};
	auto checkAdapter = [this, finish]() {
		if (!local)
		{
			local = new QBluetoothLocalDevice(this);
			connect(local, &QBluetoothLocalDevice::hostModeStateChanged, this, [this, finish](QBluetoothLocalDevice::HostMode m) {
				if (prepareDone && m != QBluetoothLocalDevice::HostPoweredOff)
					finish(true, QString());
			});
		}
		if (!local->isValid())
			return finish(false, QStringLiteral("Dieses Gerät hat kein Bluetooth."));
		if (local->hostMode() != QBluetoothLocalDevice::HostPoweredOff)
			return finish(true, QString());
		// Bluetooth ist aus: einschalten (Android fragt nach) und auf die Rueckmeldung warten
		local->powerOn();
		if (!powerTimer)
		{
			powerTimer = new QTimer(this);
			powerTimer->setSingleShot(true);
		}
		powerTimer->disconnect();
		connect(powerTimer, &QTimer::timeout, this, [finish]() {
			finish(false, QStringLiteral("Bluetooth ist ausgeschaltet. Bitte einschalten und erneut versuchen."));
		});
		powerTimer->start(30000);
	};

#if QT_CONFIG(permissions)
	QBluetoothPermission permission;
	permission.setCommunicationModes(QBluetoothPermission::Access | QBluetoothPermission::Advertise);
	switch (qApp->checkPermission(permission))
	{
	case Qt::PermissionStatus::Granted:
		checkAdapter();
		return;
	case Qt::PermissionStatus::Denied:
		finish(false, QStringLiteral("Die App darf Bluetooth nicht verwenden. Bitte in den Einstellungen erlauben "
		                             "(Berechtigung „Geräte in der Nähe“)."));
		return;
	case Qt::PermissionStatus::Undetermined:
		qApp->requestPermission(permission, this, [finish, checkAdapter](const QPermission& p) {
			if (p.status() == Qt::PermissionStatus::Granted)
				checkAdapter();
			else
				finish(false, QStringLiteral("Ohne Bluetooth-Berechtigung ist keine Partie über Bluetooth möglich."));
		});
		return;
	}
#endif
	checkAdapter();
}

void BluetoothLink::host()
{
	close();
	hosting = true;
	server = new QBluetoothServer(QBluetoothServiceInfo::RfcommProtocol, this);
	connect(server, &QBluetoothServer::newConnection, this, [this]() {
		while (server && server->hasPendingConnections())
		{
			QBluetoothSocket* s = server->nextPendingConnection();
			if (socket) // schon ein Mitspieler da
			{
				s->close();
				s->deleteLater();
				continue;
			}
			adoptSocket(s);
			linked = true;
			peer = s->peerName().isEmpty() ? s->peerAddress().toString() : s->peerName();
			QTimer::singleShot(0, this, [this]() { stopServer(); }); // nicht mehr sichtbar sein / annehmen
			emit connected();
		}
	});
	connect(server, &QBluetoothServer::errorOccurred, this, [this](QBluetoothServer::Error e) {
		qDebug() << "Bluetooth-Server-Fehler" << e;
		if (!linked)
			emit failed(QStringLiteral("Das Spiel konnte nicht eröffnet werden."));
	});
	const QBluetoothServiceInfo info = server->listen(serviceUuid(), QStringLiteral("Reversi"));
	if (!info.isValid())
	{
		emit failed(QStringLiteral("Das Spiel konnte nicht eröffnet werden (Bluetooth belegt?)."));
		return;
	}
	delete service;
	service = new QBluetoothServiceInfo(info);
	if (local)
		local->setHostMode(QBluetoothLocalDevice::HostDiscoverable); // Android fragt, ob sichtbar werden
}

void BluetoothLink::startSearch()
{
	stopSearch();
	if (!agent)
	{
		agent = new QBluetoothDeviceDiscoveryAgent(this);
		connect(agent, &QBluetoothDeviceDiscoveryAgent::deviceDiscovered, this, [this](const QBluetoothDeviceInfo& d) {
			if (!(d.coreConfigurations() & QBluetoothDeviceInfo::BaseRateCoreConfiguration))
				return; // nur klassisches Bluetooth (RFCOMM)
			const auto major = d.majorDeviceClass();
			if (major != QBluetoothDeviceInfo::PhoneDevice && major != QBluetoothDeviceInfo::ComputerDevice
				&& major != QBluetoothDeviceInfo::UncategorizedDevice && major != QBluetoothDeviceInfo::MiscellaneousDevice)
				return; // Kopfhoerer, Uhren, Autos ... ausblenden
			const QString address = d.address().toString();
			emit deviceFound(d.name().isEmpty() ? address : d.name(), address);
		});
		connect(agent, &QBluetoothDeviceDiscoveryAgent::finished, this, &BluetoothLink::searchFinished);
		connect(agent, &QBluetoothDeviceDiscoveryAgent::canceled, this, &BluetoothLink::searchFinished);
		connect(agent, &QBluetoothDeviceDiscoveryAgent::errorOccurred, this, [this](QBluetoothDeviceDiscoveryAgent::Error e) {
			qDebug() << "Bluetooth-Suche-Fehler" << e;
			emit failed(e == QBluetoothDeviceDiscoveryAgent::PoweredOffError
				? QStringLiteral("Bluetooth ist ausgeschaltet.")
				: e == QBluetoothDeviceDiscoveryAgent::MissingPermissionsError
				? QStringLiteral("Die Suche braucht die Berechtigung „Geräte in der Nähe“ (Android 9–11: Standort einschalten).")
				: QStringLiteral("Die Gerätesuche ist fehlgeschlagen."));
		});
	}
	agent->start(QBluetoothDeviceDiscoveryAgent::ClassicMethod);
}

void BluetoothLink::stopSearch()
{
	if (agent && agent->isActive())
		agent->stop();
}

void BluetoothLink::join(const QString& address, const QString& name)
{
	stopSearch();
	dropSocket();
	hosting = false;
	peer = name;
	QBluetoothSocket* s = new QBluetoothSocket(QBluetoothServiceInfo::RfcommProtocol, this);
	adoptSocket(s);
	connect(s, &QBluetoothSocket::connected, this, [this]() {
		linked = true;
		emit connected();
	});
	s->connectToService(QBluetoothAddress(address), serviceUuid());
}

void BluetoothLink::adoptSocket(QBluetoothSocket* s)
{
	socket = s;
	buffer.clear();
	connect(s, &QBluetoothSocket::readyRead, this, &BluetoothLink::readLines);
	connect(s, &QBluetoothSocket::disconnected, this, [this, s]() {
		if (s != socket)
			return;
		const bool wasLinked = linked;
		dropSocket();
		if (wasLinked)
			emit disconnected();
		else
			emit failed(QStringLiteral("Keine Verbindung. Läuft auf dem anderen Gerät „Spiel eröffnen“?"));
	});
	connect(s, &QBluetoothSocket::errorOccurred, this, [this, s](QBluetoothSocket::SocketError e) {
		qDebug() << "Bluetooth-Socket-Fehler" << e;
		if (s != socket)
			return;
		const bool wasLinked = linked;
		dropSocket();
		if (wasLinked)
			emit disconnected();
		else
			emit failed(QStringLiteral("Keine Verbindung. Läuft auf dem anderen Gerät „Spiel eröffnen“?"));
	});
}

void BluetoothLink::readLines()
{
	if (!socket)
		return;
	buffer += socket->readAll();
	int end;
	while ((end = buffer.indexOf('\n')) >= 0)
	{
		const QString line = QString::fromUtf8(buffer.left(end)).trimmed();
		buffer.remove(0, end + 1);
		if (!line.isEmpty())
			emit lineReceived(line); // kann close() ausloesen -> socket pruefen
		if (!socket)
			return;
	}
}

void BluetoothLink::send(const QString& line)
{
	if (socket && linked)
		socket->write((line + QLatin1Char('\n')).toUtf8());
}

void BluetoothLink::sendAndClose(const QString& line)
{
	QBluetoothSocket* s = linked ? socket : nullptr;
	if (s)
	{
		socket = nullptr; // abkoppeln: kein disconnected-Signal mehr, neue Verbindung moeglich
		s->disconnect(this);
		s->write((line + QLatin1Char('\n')).toUtf8());
		QTimer::singleShot(500, s, [s]() { s->abort(); s->deleteLater(); }); // abort verwirft Ungesendetes
	}
	close();
}

bool BluetoothLink::isConnected() const
{
	return socket && linked;
}

QString BluetoothLink::localName() const
{
	return local && local->isValid() ? local->name() : QString();
}

void BluetoothLink::dropSocket()
{
	linked = false;
	if (socket)
	{
		QBluetoothSocket* s = socket;
		socket = nullptr;
		s->disconnect(this);
		s->abort();
		s->deleteLater();
	}
	buffer.clear();
}

void BluetoothLink::stopServer()
{
	if (server)
	{
		server->disconnect(this);
		server->close();
		server->deleteLater();
		server = nullptr;
	}
	if (service)
	{
		service->unregisterService();
		delete service;
		service = nullptr;
	}
	if (local && local->hostMode() == QBluetoothLocalDevice::HostDiscoverable)
		local->setHostMode(QBluetoothLocalDevice::HostConnectable);
}

void BluetoothLink::close()
{
	stopSearch();
	stopServer();
	dropSocket();
	hosting = false;
	peer.clear();
}

#else // ohne Qt Bluetooth: Platzhalter, der Modus wird gar nicht angeboten

bool BluetoothLink::compiledIn() { return false; }
BluetoothLink::BluetoothLink(QObject* parent) : QObject(parent) {}
BluetoothLink::~BluetoothLink() = default;
void BluetoothLink::prepare(std::function<void(bool, const QString&)> done)
{
	done(false, QStringLiteral("Diese Version wurde ohne Bluetooth gebaut."));
}
void BluetoothLink::host() {}
void BluetoothLink::startSearch() {}
void BluetoothLink::stopSearch() {}
void BluetoothLink::join(const QString&, const QString&) {}
void BluetoothLink::close() {}
bool BluetoothLink::isConnected() const { return false; }
QString BluetoothLink::localName() const { return QString(); }
void BluetoothLink::send(const QString&) {}
void BluetoothLink::sendAndClose(const QString&) {}
void BluetoothLink::adoptSocket(QBluetoothSocket*) {}
void BluetoothLink::readLines() {}
void BluetoothLink::dropSocket() {}
void BluetoothLink::stopServer() {}

#endif
