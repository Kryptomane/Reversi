#pragma once
// Ebene ueber dem Spielfenster zum Aufbau einer Bluetooth-Partie (gleicher Stil wie MessageOverlay):
// erst "Spiel eroeffnen" oder "Spiel beitreten", dann Warten auf den Mitspieler bzw. Liste der
// gefundenen Geraete zum Antippen. Steht die Verbindung, schliesst ReversiGui die Ebene.
#include <QWidget>

class QLabel;
class QPushButton;
class QListWidget;
class QBoxLayout;
class BluetoothLink;

class BluetoothLobby : public QWidget
{
	Q_OBJECT

public:
	BluetoothLobby(QWidget* parent, BluetoothLink* link);

	void open();                    // Berechtigungen pruefen, dann Auswahl zeigen
	void dismiss();                 // ohne Signal schliessen (z. B. nach dem Verbinden)
	bool isOpen() const { return isVisible(); }

signals:
	void cancelled();               // Nutzer hat abgebrochen; die Verbindung ist geschlossen

public slots:
	void cancel();

protected:
	bool eventFilter(QObject* watched, QEvent* e) override;
	void paintEvent(QPaintEvent* e) override;
	void mousePressEvent(QMouseEvent* e) override;

private:
	enum class Page { Waiting, Choice, Hosting, Joining };
	void showPage(Page p, const QString& info = QString());
	void startHosting();
	void startJoining();
	void onFailed(const QString& message);

	BluetoothLink* link;
	Page page = Page::Waiting;
	QLabel* titleLabel;
	QLabel* infoLabel;
	QListWidget* deviceList;
	QPushButton* hostButton;
	QPushButton* joinButton;
	QPushButton* againButton;
	QPushButton* cancelButton;
};
