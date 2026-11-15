#pragma once
// Verbindung zweier Geraete ueber klassisches Bluetooth (RFCOMM) fuer eine Partie Mensch gegen Mensch.
// Ein Geraet eroeffnet (Server, sichtbar), das andere sucht Geraete in der Naehe und tritt bei.
// Uebertragen werden einzelne Textzeilen (UTF-8, mit '\n' abgeschlossen), siehe ReversiGui.
// Ohne Qt Bluetooth (REVERSI_BLUETOOTH nicht gesetzt) ist available() false und alles andere tut nichts.
#include <QObject>
#include <QByteArray>
#include <QString>
#include <functional>

class QBluetoothServer;
class QBluetoothSocket;
class QBluetoothDeviceDiscoveryAgent;
class QBluetoothServiceInfo;
class QBluetoothLocalDevice;
class QTimer;

class BluetoothLink : public QObject
{
	Q_OBJECT

public:
	static bool compiledIn();                   // mit Qt Bluetooth gebaut?

	explicit BluetoothLink(QObject* parent = nullptr);
	~BluetoothLink() override;

	// Berechtigungen anfragen und Bluetooth einschalten; ruft done(ok, Fehlertext) genau einmal auf.
	void prepare(std::function<void(bool, const QString&)> done);

	void host();                                // Spiel eroeffnen: lauschen und sichtbar werden
	void startSearch();                         // Geraete in der Naehe suchen (deviceFound)
	void stopSearch();
	void join(const QString& address, const QString& name); // mit einem gefundenen Geraet verbinden
	void close();                               // alles beenden (ohne Signal disconnected)

	bool isConnected() const;
	bool isHost() const { return hosting; }
	QString peerName() const { return peer; }
	QString localName() const;

	void send(const QString& line);
	void sendAndClose(const QString& line);     // letzte Zeile (z. B. BYE) noch zustellen, dann trennen

signals:
	void deviceFound(const QString& name, const QString& address);
	void searchFinished();
	void connected();
	void lineReceived(const QString& line);
	void disconnected();                        // Verbindung nach dem Aufbau verloren
	void failed(const QString& message);        // Aufbau gescheitert (Eroeffnen, Suche, Beitreten)

private:
	void adoptSocket(QBluetoothSocket* s);
	void readLines();
	void dropSocket();
	void stopServer();

	QBluetoothLocalDevice* local = nullptr;
	QBluetoothServer* server = nullptr;
	QBluetoothServiceInfo* service = nullptr;
	QBluetoothSocket* socket = nullptr;
	QBluetoothDeviceDiscoveryAgent* agent = nullptr;
	QTimer* powerTimer = nullptr;
	std::function<void(bool, const QString&)> prepareDone;
	QByteArray buffer;
	QString peer;
	bool hosting = false;
	bool linked = false;                        // Verbindung steht (fuer disconnected)
};
