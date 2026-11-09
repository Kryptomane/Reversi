#pragma once
// Meldung/Rueckfrage als Ebene ueber dem Spielfenster (statt QMessageBox, die unter Android als
// nativer Dialog nicht zuverlaessig erscheint): abgedunkelter Hintergrund, Text, zwei grosse Knoepfe.
#include <QWidget>
#include <functional>

class QLabel;
class QPushButton;
class QBoxLayout;

class MessageOverlay : public QWidget
{
	Q_OBJECT

public:
	struct Message
	{
		QString title;                          // gross, fett
		QString detail;                         // optional, darunter
		QString yesText;                        // rechter (gruener) Knopf
		QString noText = QStringLiteral("Abbrechen");
		bool fullScreen = false;                // Kasten fuellt das ganze Fenster (z. B. Spielende)
	};

	explicit MessageOverlay(QWidget* parent);

	// onYes wird nur beim rechten Knopf aufgerufen; der linke Knopf (und cancel) schliesst nur.
	void show(const Message& m, std::function<void()> onYes);
	void cancel();
	bool isOpen() const { return isVisible(); }

protected:
	bool eventFilter(QObject* watched, QEvent* e) override; // Groesse an das Elternfenster anpassen
	void paintEvent(QPaintEvent* e) override;
	void mousePressEvent(QMouseEvent* e) override;          // Tippen daneben bewirkt nichts

private:
	QBoxLayout* outer;
	QLabel* titleLabel;
	QLabel* detailLabel;
	QPushButton* yesButton;
	QPushButton* noButton;
	std::function<void()> yesAction;
};
