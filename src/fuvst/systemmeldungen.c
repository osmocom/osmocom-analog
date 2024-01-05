#include <stdlib.h>
#include <inttypes.h>
#include "../liblogging/logging.h"
#include "systemmeldungen.h"

static struct systemmeldungen {
	uint16_t code;
	const char *desc;
	int bytes;
	const char *ind[10];
} systemmeldungen[] =
{
	{
		0x0025,
		"OGK, SPK, FME, PFG oder PHE(STB) antwortet nicht",
		1,
		{
			"Physikalische Einrichtungs-Adresse",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x0026,
		"Datenverfaelschung bei Verwaltungsdaten des OS (TO abgelaufen, obwohl kein TO vom Prozess verlangt wurde Indizien lassen nur bedingt auf Fehler schliessen!)",
		8,
		{
			"DKV-Nummer",
			"#Ident-Nummer#",
			"LOW-PC des letzten Prozesses",
			"HIGH-PC des letzten Prozesses",
			"Prozess-Status 1 des letzten Prozesses (LOW-TO)",
			"Prozess-Status 1 des letzten Prozesses (HIGH-TO)",
			"Prozess-Status 2 des letzten Prozesses",
			"Prozess-Status 3 des letzten Prozesses",
			"", "",
		},
	},

	{
		0x0027,
		"HW-Timer defekt oder Datenverfaelschung bei Verwaltungsdaten des OS. (Funkblock nicht normal beendet: Interrupt zum DMA- Start hat das Puffer-Transfer-Programm unterbrochen. Indizien lassen nur bedingt auf Fehler schliessen!)",
		8,
		{
			"DKV-Nummer",
			"#Ident-Nummer#",
			"LOW-PC des letzten Prozesses",
			"HIGH-PC des letzten Prozesses",
			"Prozess-Status 1 des letzten Prozesses (LOW-TO)",
			"Prozess-Status 1 des letzten Prozesses (HIGH-TO)",
			"Prozess-Status 2 des letzten Prozesses",
			"Prozess-Status 3 des letzten Prozesses",
			"", "",
		},
	},

	{
		0x0028,
		"HW-Timer defekt oder DKO-Fehler. (Der DKO gibt nach dem DMA-Interrupt nicht rechtzeitig die Quittung an die DKV. Fehlerursache laesst sich nicht durch Indizien belegen!)",
		8,
		{
			"DKV-Nummer",
			"#Ident-Nummer#",
			"LOW-PC des letzten Prozesses",
			"HIGH-PC des letzten Prozesses",
			"Prozess-Status 1 des letzten Prozesses (LOW-TO)",
			"Prozess-Status 1 des letzten Prozesses (HIGH-TO)",
			"Prozess-Status 2 des letzten Prozesses",
			"Prozess-Status 3 des letzten Prozesses",
			"", "",
		},
	},

	{
		0x0029,
		"HW-Timer defekt oder DKO-Fehler. (DKO-DMUE-FR-L liegt an, obwohl kein Interrupt ausgegeben wurde. (Puffer und Betriebsparameter im regulaeren Betrieb). Indizien lassen nur bedingt auf Fehler schliessen!)",
		8,
		{
			"DKV-Nummer",
			"#Ident-Nummer#",
			"LOW-PC des letzten Prozesses",
			"HIGH-PC des letzten Prozesses",
			"Prozess-Status 1 des letzten Prozesses (LOW-TO)",
			"Prozess-Status 1 des letzten Prozesses (HIGH-TO)",
			"Prozess-Status 2 des letzten Prozesses",
			"Prozess-Status 3 des letzten Prozesses",
			"", "",
		},
	},

	{
		0x002a,
		"Fehler in der Taktversorgung. (Interrupt 0 kommt zu frueh Indizien lassen nur bedingt auf Fehler schliessen!)",
		8,
		{
			"DKV-Nummer",
			"#Ident-Nummer#",
			"LOW-PC des letzten Prozesses",
			"HIGH-PC des letzten Prozesses",
			"Prozess-Status 1 des letzten Prozesses (LOW-TO)",
			"Prozess-Status 1 des letzten Prozesses (HIGH-TO)",
			"Prozess-Status 2 des letzten Prozesses",
			"Prozess-Status 3 des letzten Prozesses",
			"", "",
		},
	},

	{
		0x002b,
		"Fehler zwischen dem letzten TIMER-Interrupt und Interrupt 0 (Interrupt 0 kommt zu spaet) Indizien lassen nur bedingt auf Fehler schliessen!",
		8,
		{
			"DKV-Nummer",
			"#Ident-Nummer#",
			"LOW-PC des letzten Prozesses",
			"HIGH-PC des letzten Prozesses",
			"Prozess-Status 1 des letzten Prozesses (LOW-TO)",
			"Prozess-Status 1 des letzten Prozesses (HIGH-TO)",
			"Prozess-Status 2 des letzten Prozesses",
			"Prozess-Status 3 des letzten Prozesses",
			"", "",
		},
	},

	{
		0x002c,
		"Watchdog-Flags nicht alle rechtzeitig gesetzt worden",
		7,
		{
			"DKV-Nummer",
			"#Watchdog-Flag-Byte#",
			"Prozess-Unterbrechungs-Byte (bei unterbrochenem Prozess <> 0)",
			"#Ident-Nummer#",
			"LOW-PC des letzten Prozesses",
			"HIGH-PC des letzten Prozesses",
			"Prozess-Status 3 des letzten Prozesses",
			"", "", "",
		},
	},

	{
		0x002d,
		"Betriebsmittelmangel in der DKV. (Bei Beantragung von Ausgangspuffern zur Parallel-FDS waren keine Ausgangspuffer mehr frei)",
		8,
		{
			"DKV-Nummer",
			"#Ident-Nummer#",
			"LOW-PC des letzten Prozesses",
			"HIGH-PC des letzten Prozesses",
			"Prozess-Status 1 des letzten Prozesses (LOW-TO)",
			"Prozess-Status 1 des letzten Prozesses (HIGH-TO)",
			"Prozess-Status 2 des letzten Prozesses",
			"Prozess-Status 3 des letzten Prozesses",
			"", "",
		},
	},

	{
		0x002e,
		"Betriebsmittelmangel in der DKV. (Fuer eine startende Signalisierung von der Parallel-FDS war keine Ident-Nummer mehr frei)",
		8,
		{
			"DKV-Nummer",
			"#Ident-Nummer#",
			"LOW-PC des letzten Prozesses",
			"HIGH-PC des letzten Prozesses",
			"Prozess-Status 1 des letzten Prozesses (LOW-TO)",
			"Prozess-Status 1 des letzten Prozesses (HIGH-TO)",
			"Prozess-Status 2 des letzten Prozesses",
			"Prozess-Status 3 des letzten Prozesses",
			"", "",
		},
	},

	{
		0x002f,
		"Datenverfaelschung bei Verwaltungsdaten des OS. (Fuer eine startende Signalisierung von der Parallel-FDS wurde eine Ident-Nummer vergeben, die nicht frei war. Fehler durch eine Task im letzten Funkblock oder RAM- Fehler)",
		8,
		{
			"DKV-Nummer",
			"#Ident-Nummer#",
			"LOW-PC des letzten Prozesses",
			"HIGH-PC des letzten Prozesses",
			"Prozess-Status 1 des letzten Prozesses (LOW-TO)",
			"Prozess-Status 1 des letzten Prozesses (HIGH-TO)",
			"Prozess-Status 2 des letzten Prozesses",
			"Prozess-Status 3 des letzten Prozesses",
			"", "",
		},
	},

	{
		0x0030,
		"HW-Fehler oder SW-Fehler. (Ein nicht angeschlossener Interrupt ist gekommen. Dieser Fehler kann auch durch ein fehlerhaftes Anwenderprogramm auftreten (z. B. durch einen RST-[7]- Befehl [Code = 0FFH] oder durch einen fehlerh. Sprung))",
		5,
		{
			"DKV-Nummer",
			"LOW-Adresse der Interrupteinsprungstelle",
			"LOW-Adresse des unterbrochenen Programms",
			"HIGH-Adresse des unterbrochenen Programms",
			"Speicherbanknummer des unterbrochenen Programms",
			"", "", "", "", "",
		},
	},

	{
		0x0031,
		"ACT-FDS moechte Block-DMAs (also keine Einzelsigna- lisierungs-DMA) zur STB-FDS durchfuehren. Dies ist in dieser Richtung nicht moeglich. (Erkennung in der STB-FDS).",
		1,
		{
			"DKV-Nummer",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x0032,
		"Datenverfaelschung bei Verwaltungsdaten des OS. (Eigene FDS vor dem DMA mit der Parallel-FDS nicht verfuegbar. Fehlerursache laesst sich nicht durch Indizien belegen!)",
		1,
		{
			"DKV-Nummer",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x0033,
		"Wartezeit in der Zeitschleife (DMA-Erleidende) ueber- schritten. (Warten auf die DMA-Anforderung der Parallel- FDS). Fehlerursache laesst sich nicht durch Indizien belegen!",
		1,
		{
			"DKV-Nummer",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x0034,
		"Beim DMA mit der Parallel-FDS ist der DMA-Ueberwachungs- interrupt in der Zeitschleife \"Warten auf DMA-Ende\" in der DMA-Erleidenden aufgetreten.",
		1,
		{
			"DKV-Nummer",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x0035,
		"Wartezeit in der Zeitschleife (DMA-Durchfuehrende) ueber- schritten. (Warten auf DMA-Bereitschaft der Parallel-FDS).",
		1,
		{
			"DKV-Nummer",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x0036,
		"Beim DMA mit der Parallel-FDS wurde die Wartezeit in der Zeitschleife (der DMA-Durchfuehrenden) ueberschritten. (Warten auf DMA-Ende-Erkennung).",
		1,
		{
			"DKV-Nummer",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x0037,
		"ACT-FDS moechte Block-DMAs (also keine Einzelsigna- lisierungs-DMA) zur STB-FDS durchfuehren. Dies ist in dieser Richtung nicht moeglich. (Erkennung in der ACT-FDS).",
		1,
		{
			"DKV-Nummer",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x0038,
		"DMA-HW in einer oder beiden FDSen oder Verbindungs-Folie zwischen den FDSen defekt. Beim DMA uebertragene Kontrollbytes stimmen nach dem DMA von Einzelsignalisierungen mit der Parallel-FDS nicht mehr. Fehlererkennung in der ACT-FDS (= DMA-Durchfuehrende). (Fehlerursache laesst sich nicht durch Indizien belegen!)",
		1,
		{
			"#Phys. Einr.-Nr.# der Parallel-FDS",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x0039,
		"Ueberwachungszaehler bei Aktiv-Datei-Suche hat ange- sprochen. Aktiv-Datei-Such-HW wahrscheinlich defekt.",
		1,
		{
			"DKV-Nummer",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x004d,
		"Bei der Kommunikationspruefung zwischen der aktiven FDS und einer Einrichtung quittiert eine Einrichtung im Zustand ACT 3x hintereinander nicht.",
		4,
		{
			"Phys. Einrichtungsnummer",
			"#Globaler ST-Zustand# der Einrichtung",
			"Anzahl nicht quittierter Pruefauftraege",
			"#Ident-Nummer#",
			"", "", "", "", "", "",
		},
	},

	{
		0x004e,
		"Bei der Kommunikationspruefung zwischen den beiden FDS'en sendet die FDS im Zustand ACT 3x hintereinander keinen Pruefauftrag.",
		3,
		{
			"Phys. Einrichtungsnummer",
			"#Ident-Nummer#",
			"#FDS-Status# (PORT 41H)",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x004f,
		"Bei der Kommunikationspruefung zwischen den beiden FDS'en quittiert die FDS im Zustand INA 3x hintereinander nicht.",
		4,
		{
			"Phys. Einrichtungsnummer",
			"#Ident-Nummer#",
			"Anzahl der nicht quittierten Pruefauftraege",
			"#FDS-Status# (PORT 41H)",
			"", "", "", "", "", "",
		},
	},

	{
		0x0050,
		"Der angelaufene PBR quittierte Signalisierung der DKV nicht. (T075) (Timeout fuer die PBR-Quittung YBFQB,  Auftrag wird wiederholt)",
		2,
		{
			"Phys. Einr.-Nr. (LODEAD)",
			"Timeout-Zaehler (LODATO) : #Entwickler-Info# :",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x0051,
		"Der angelaufene PBR quittiert, nach mehrmaliger Wiederholung, eine Signalisierung der DKV nicht. (T076) (Letzter zulaessiger Timeout fuer PBR-Quittung  YBFQB, Auftrag wird nicht mehr wiederholt,  Einrichtung wird nach ACT konfiguriert)",
		1,
		{
			"Phys. Einr.-Nr.     (LODEAD)",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x0053,
		"Die angegebene Einrichtung ist nicht angelaufen. Sie hat keine Anlaufsignalisierung geschickt. (T017) (Timeout fuer Quittung YKBAI ,  Einrichtung wird nach UNA konfiguriert)",
		4,
		{
			"Phys. Einr.-Nr. (LODEAD)",
			"#Einrichtungs-Typ#",
			"#Log. Einr.-Nr.#",
			"Anlaufversuchsstatistik #Entwickler-Info#",
			"", "", "", "", "", "",
		},
	},

	{
		0x005d,
		"FuPeF-Systemmeldung wurde empfangen, die Bedeutung der Indizien muss unter der uebergebenen Systemmeldungs-Nummer in einer FuPeF-Beschreibung nachgelesen werden. Je nach Indizienlaenge sind nur die entsprechenden RP-Bytes gueltig.",
		7,
		{
			"Physikalische Einrichtungs-Nummer",
			"Systemmeldungs-Nummer (High-Byte)",
			"Systemmeldungs-Nummer (Low-Byte)",
			"Indizien-Byte 1 der FuPeF-Systemmeldung",
			"Indizien-Byte 2 der FuPeF-Systemmeldung",
			"Indizien-Byte 3 der FuPeF-Systemmeldung",
			"Indizien-Byte 4 der FuPeF-Systemmeldung",
			"", "", "",
		},
	},

	{
		0x005e,
		"FuPeF-Systemmeldung wurde empfangen. Die Bedeutung der Indizien muss unter der uebergebenen Systemmeldungs-Nummer in einer FuPeF-Beschreibung nachgelesen werden. Je nach Indizienlaenge sind nur die entsprechenden RP-Bytes gueltig.",
		7,
		{
			"Physikalische Einrichtungs-Nummer",
			"Systemmeldungs-Nummer (High-Byte)",
			"Systemmeldungs-Nummer (Low-Byte)",
			"Indizien-Byte 1 der FuPeF-Systemmeldung",
			"Indizien-Byte 2 der FuPeF-Systemmeldung",
			"Indizien-Byte 3 der FuPeF-Systemmeldung",
			"Indizien-Byte 4 der FuPeF-Systemmeldung",
			"", "", "",
		},
	},

	{
		0x005f,
		"FuPeF-Systemmeldung wurde empfangen. Die Bedeutung der Indizien muss unter der uebergebenen Systemmeldungs-Nummer in einer FuPeF-Beschreibung nachgelesen werden. Je nach Indizienlaenge sind nur die entsprechenden RP-Bytes gueltig.",
		7,
		{
			"Physikalische Einrichtungs-Nummer",
			"Systemmeldungs-Nummer (High-Byte)",
			"Systemmeldungs-Nummer (Low-Byte)",
			"Indizien-Byte 1 der FuPeF-Systemmeldung",
			"Indizien-Byte 2 der FuPeF-Systemmeldung",
			"Indizien-Byte 3 der FuPeF-Systemmeldung",
			"Indizien-Byte 4 der FuPeF-Systemmeldung",
			"", "", "",
		},
	},

	{
		0x0060,
		"FuPeF-Systemmeldung wurde empfangen. Die Bedeutung der Indizien muss unter der uebergebenen Systemmeldungs-Nummer in einer FuPeF-Beschreibung nachgelesen werden. Je nach Indizienlaenge sind nur die entsprechenden RP-Bytes gueltig.",
		7,
		{
			"Physikalische Einrichtungs-Nummer",
			"Systemmeldungs-Nummer (High-Byte)",
			"Systemmeldungs-Nummer (Low-Byte)",
			"Indizien-Byte 1 der FuPeF-Systemmeldung",
			"Indizien-Byte 2 der FuPeF-Systemmeldung",
			"Indizien-Byte 3 der FuPeF-Systemmeldung",
			"Indizien-Byte 4 der FuPeF-Systemmeldung",
			"", "", "",
		},
	},

	{
		0x0061,
		"FuPeF-Systemmeldung wurde mit unzulaessigem Alarm-Gewicht empfangen.",
		8,
		{
			"Physikalische Einrichtungs-Nummer",
			"Systemmeldungs-Nummer (High-Byte)",
			"Systemmeldungs-Nummer (Low-Byte)",
			"Indizien-Byte 1 der FuPeF-Systemmeldung",
			"Indizien-Byte 2 der FuPeF-Systemmeldung",
			"Indizien-Byte 3 der FuPeF-Systemmeldung",
			"Indizien-Byte 4 der FuPeF-Systemmeldung",
			"#Alarm-Gewicht#/Indizienlaenge der FuPeF- Systemmeldunmg",
			"", "",
		},
	},

	{
		0x0062,
		"FuPeF-Systemmeldung wurde mit unzulaessiger Indizien- laenge empfangen.",
		8,
		{
			"Physikalische Einrichtungs-Nummer",
			"Systemmeldungs-Nummer (High-Byte)",
			"Systemmeldungs-Nummer (Low-Byte)",
			"Indizien-Byte 1 der FuPeF-Systemmeldung",
			"Indizien-Byte 2 der FuPeF-Systemmeldung",
			"Indizien-Byte 3 der FuPeF-Systemmeldung",
			"Indizien-Byte 4 der FuPeF-Systemmeldung",
			"#Alarm-Gewicht#/Indizienlaenge der FuPeF- Systemmeldung",
			"", "",
		},
	},

	{
		0x0063,
		"FuPeF-Systemmeldung wurde empfangen. Die Bedeutung der Indizien muss unter der uebergebenen Systemmeldungs-Nummer in einer FuPeF-Beschreibung nachgelesen werden. Je nach Indizienlaenge sind nur die entsprechenden RP-Bytes gueltig.",
		7,
		{
			"Physikalische Einrichtungs-Nummer",
			"Systemmeldungs-Nummer (High-Byte)",
			"Systemmeldungs-Nummer (Low-Byte)",
			"Indizien-Byte 1 der FuPeF-Systemmeldung",
			"Indizien-Byte 2 der FuPeF-Systemmeldung",
			"Indizien-Byte 3 der FuPeF-Systemmeldung",
			"Indizien-Byte 4 der FuPeF-Systemmeldung",
			"", "", "",
		},
	},

	{
		0x0068,
		"Der in den Indizien angegebene Fehler hat zum Erreichen des Schwellwerts der 24-Std.-Anlaufstatistik und damit zum Konfigurieren der angegebenen Einrichtung nach UNA gefuehrt .",
		9,
		{
			"Physikalische Einrichtungs-Nummer aus dem bearbeiteten RP-Element",
			"Systemmeldungs-Nummer (High-Byte), bei der Eskalation auftrat",
			"Systemmeldungs-Nummer (Low-Byte), bei der Eskalation auftrat",
			"Indizien-Byte 1 aus RP-Element zu obiger Systemmeldungs-Nummer",
			"Indizien-Byte 2 aus RP-Element zu obiger Systemmeldungs-Nummer",
			"Indizien-Byte 3 aus RP-Element zu obiger Systemmeldungs-Nummer",
			"Indizien-Byte 4 aus RP-Element zu obiger Systemmeldungs-Nummer",
			"Indizien-Byte 5 aus RP-Element zu obiger Systemmeldungs-Nummer",
			"Indizien-Byte 6 aus RP-Element zu obiger Systemmeldungs-Nummer",
			"",
		},
	},

	{
		0x0069,
		"Datenverfaelschung bei der Uebergabe der in den Indizien angegebenen Systemmeldungs-Nummer (uebergebene physikalische Einrichtungs-Nummer liegt nicht im erlaubten Wertebereich, ZIB-Eintrag)",
		10,
		{
			"Anwender-Systemmeldungs-Nummer (High-Byte), die zum Reset fuehrte",
			"Anwender-Systemmeldungs-Nummer (Low-Byte), die zum Reset fuehrte",
			"Anwender-ROM-Speicherbank-Nummer",
			"Anwender-Befehlszaehler (High-Byte)",
			"Anwender-Befehlszaehler (Low-Byte)",
			"Siehe Beschreibung zu obiger Systemmeldungs-Nummer, Indizien-Byte 0",
			"Siehe Beschreibung zu obiger Systemmeldungs-Nummer, Indizien-Byte 1",
			"Siehe Beschreibung zu obiger Systemmeldungs-Nummer, Indizien-Byte 2",
			"Siehe Beschreibung zu obiger Systemmeldungs-Nummer, Indizien-Byte 3",
			"Siehe Beschreibung zu obiger Systemmeldungs-Nummer, Indizien-Byte 4",
		},
	},

	{
		0x006b,
		"DKO-Systemmeldung wurde empfangen. Die Bedeutung der Indizien muss unter der uebergebenen Systemmeldungs- Nummer in einer DKO-Beschreibung nachgelesen werden. Je nach Indizienlaenge sind nur die entsprechenden RP-Bytes gueltig.",
		8,
		{
			"Physikalische Einrichtungs-Nummer",
			"Systemmeldungs-Nummer (High-Byte)",
			"Systemmeldungs-Nummer (Low-Byte)",
			"Indizien-Byte 1 der DKO-Systemmeldung",
			"Indizien-Byte 2 der DKO-Systemmeldung",
			"Indizien-Byte 3 der DKO-Systemmeldung",
			"Indizien-Byte 4 der DKO-Systemmeldung",
			"Indizien-Byte 5 der DKO-Systemmeldung",
			"", "",
		},
	},

	{
		0x006c,
		"DKO-Systemmeldung wurde empfangen. Die Bedeutung der Indizien muss unter der uebergebenen Systemmeldungs- Nummer in einer DKO-Beschreibung nachgelesen werden. Je nach Indizienlaenge sind nur die entsprechenden RP-Bytes gueltig.",
		8,
		{
			"Physikalische Einrichtungs-Nummer",
			"Systemmeldungs-Nummer (High-Byte)",
			"Systemmeldungs-Nummer (Low-Byte)",
			"Indizien-Byte 1 der DKO-Systemmeldung",
			"Indizien-Byte 2 der DKO-Systemmeldung",
			"Indizien-Byte 3 der DKO-Systemmeldung",
			"Indizien-Byte 4 der DKO-Systemmeldung",
			"Indizien-Byte 5 der DKO-Systemmeldung",
			"", "",
		},
	},

	{
		0x006d,
		"DKO-Systemmeldung wurde mit unzulaessigem Alarm-Gewicht empfangen.",
		9,
		{
			"Physikalische Einrichtungs-Nummer",
			"Systemmeldungs-Nummer (High-Byte)",
			"Systemmeldungs-Nummer (Low-Byte)",
			"Indizien-Byte 1 der DKO-Systemmeldung",
			"Indizien-Byte 2 der DKO-Systemmeldung",
			"Indizien-Byte 3 der DKO-Systemmeldung",
			"Indizien-Byte 4 der DKO-Systemmeldung",
			"Indizien-Byte 5 der DKO-Systemmeldung",
			"#Alarm-Gewicht#/Indizienlaenge der DKO-Systemmeldung",
			"",
		},
	},

	{
		0x006e,
		"DKO-Systemmeldung wurde mit unzulaessiger Indizienlaenge empfangen.",
		9,
		{
			"Physikalische Einrichtungs-Nummer",
			"Systemmeldungs-Nummer (High-Byte)",
			"Systemmeldungs-Nummer (Low-Byte)",
			"Indizien-Byte 1 der DKO-Systemmeldung",
			"Indizien-Byte 2 der DKO-Systemmeldung",
			"Indizien-Byte 3 der DKO-Systemmeldung",
			"Indizien-Byte 4 der DKO-Systemmeldung",
			"Indizien-Byte 5 der DKO-Systemmeldung",
			"#Alarm-Gewicht#/Indizienlaenge der DKO-Systemmeldung",
			"",
		},
	},

	{
		0x006f,
		"DKO-Systemmeldung wurde empfangen. Die Bedeutung der Indizien muss unter der uebergebenen Systemmeldungs- Nummer in einer DKO-Beschreibung nachgelesen werden. Je nach Indizienlaenge sind nur die entsprechenden RP-Bytes gueltig. Die Einrichtung wurde nach UNA konfiguriert.",
		8,
		{
			"Physikalische Einrichtungs-Nummer",
			"Systemmeldungs-Nummer (High-Byte)",
			"Systemmeldungs-Nummer (Low-Byte)",
			"Indizien-Byte 1 der DKO-Systemmeldung",
			"Indizien-Byte 2 der DKO-Systemmeldung",
			"Indizien-Byte 3 der DKO-Systemmeldung",
			"Indizien-Byte 4 der DKO-Systemmeldung",
			"Indizien-Byte 5 der DKO-Systemmeldung",
			"", "",
		},
	},

	{
		0x0070,
		"Zeitzeichen QSET fehlt.",
		5,
		{
			"Physikalische Einrichtungs-Nummer der DKV",
			"Port #DYIFDS#",
			"Port #DYIRES#",
			"Port #DYIER0#",
			"Port #DYIER1#",
			"", "", "", "", "",
		},
	},

	{
		0x0071,
		"FKM-Netzteil ausgefallen",
		5,
		{
			"Physikalische Einrichtungs-Nummer der DKV",
			"Port #DYIFDS#",
			"Port #DYIRES#",
			"Port #DYIER0#",
			"Port #DYIER1#",
			"", "", "", "", "",
		},
	},

	{
		0x0072,
		"FME-Netzteil ausgefallen",
		5,
		{
			"Physikalische Einrichtungs-Nummer der DKV",
			"Port #DYIFDS#",
			"Port #DYIRES#",
			"Port #DYIER0#",
			"Port #DYIER1#",
			"", "", "", "", "",
		},
	},

	{
		0x0073,
		"Aenderung des Batterie-Ladungs-Zustandes auf \"nicht Notstrom-Betrieb\".",
		7,
		{
			"Physikalische Einrichtungs-Nummer der DKV",
			"Port #DYIFDS#",
			"Port #DYIRES#",
			"Port #DYIER0#",
			"Port #DYIER1#",
			"#IYLBAT# (Merkzelle fuer Batterie-Ladungs-Zustand)",
			"#IYLSAP# (Merkzelle fuer vorletzten Zustand am Port #DYIER1#)",
			"", "", "",
		},
	},

	{
		0x0074,
		"Aenderung des Batterie-Ladungs-Zustandes auf \"Notstrom-Betrieb\".",
		7,
		{
			"Physikalische Einrichtungs-Nummer der DKV",
			"Port #DYIFDS#",
			"Port #DYIRES#",
			"Port #DYIER0#",
			"Port #DYIER1#",
			"#IYLBAT# (Merkzelle fuer Batterie-Ladungs-Zustand)",
			"#IYLSAP# (Merkzelle fuer vorletzten Zustand am Port #DYIER1#)",
			"", "", "",
		},
	},

	{
		0x0075,
		"Parallel-Zentralgestell-Netzteil ist ausgefallen.",
		5,
		{
			"Physikalische Einrichtungs-Nummer der DKV im Parallel-Zentralgestell",
			"Port #DYIFDS#",
			"Port #DYIRES#",
			"Port #DYIER0#",
			"Port #DYIER1#",
			"", "", "", "", "",
		},
	},

	{
		0x0076,
		"Beim Empfang einer Link-Zustands-Meldung wurde festgestellt, dass die uebergebene physikalische Einrichtungs-Nummer nicht Link1 bzw. Link2 entspricht.",
		2,
		{
			"Physikalische Einrichtungs-Nummer",
			"aktueller #Link-Zustand#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x0077,
		"Link-Zustands-Aenderung (ACTE wurde generiert)",
		3,
		{
			"Physikalische Einrichtungs-Nummer",
			"aktueller #Link-Zustand#",
			"alter #Link-Zustand#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0078,
		"Die Ueberwachung im DKO hat fuer den SAE den Zustand \"DEFEKT\" erkannt.",
		3,
		{
			"Physikalische Einrichtungs-Nummer",
			"aktueller #Link-Zustand#",
			"alter #Link-Zustand#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x007a,
		"Prozess wurde mit nicht erwartetem Ereignis-Typ aufgerufen.",
		3,
		{
			"Physikalische Einrichtungs-Nummer der DKV",
			"#Ereignis-Typ#",
			"#Opcode#, falls Ereignis-Typ = Signalisierung",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x007b,
		"Prozess wurde mit nicht erwartetem Ereignis-Typ aufgerufen.",
		3,
		{
			"Physikalische Einrichtungs-Nummer der DKV",
			"#Ereignis-Typ#",
			"#Opcode#, falls Ereignis-Typ = Signalisierung",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x007c,
		"Prozess wurde mit nicht erwartetem Ereignis-Typ aufgerufen.",
		3,
		{
			"Physikalische Einrichtungs-Nummer der DKV",
			"#Ereignis-Typ#",
			"#Opcode#, falls Ereignis-Typ = Signalisierung",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x007e,
		"Prozess wurde mit nicht erwartetem Ereignis-Typ aufgerufen.",
		3,
		{
			"Physikalische Einrichtungs-Nummer der DKV",
			"#Ereignis-Typ#",
			"#Opcode#, falls Ereignis-Typ = Signalisierung",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0081,
		"Datenverfaelschung bei der Uebergabe der in den Indizien angegebenen Systemmeldungs-Nummer. (Massnahmen-Bits haben unzulaessigen Wert, SW-Reset, in den ZIB werden Anwender-Systemmeldungs-Nummer und Anwender-Indizien uebernommen, kein RP-Element mit dieser Systemmeldungs-Nummer)",
		10,
		{
			"Anwender-Systemmeldungs-Nummer (High-Byte)",
			"Anwender-Systemmeldungs-Nummer (Low-Byte)",
			"Anwender-ROM-Speicherbank-Nummer",
			"Anwender-Befehlszaehler (High-Byte)",
			"Anwender-Befehlszaehler (Low-Byte)",
			"Siehe Beschreibung zu obiger Systemmeldungs-Nummer, Indizien-Byte 0",
			"Siehe Beschreibung zu obiger Systemmeldungs-Nummer, Indizien-Byte 1",
			"Siehe Beschreibung zu obiger Systemmeldungs-Nummer, Indizien-Byte 2",
			"Siehe Beschreibung zu obiger Systemmeldungs-Nummer, Indizien-Byte 3",
			"Siehe Beschreibung zu obiger Systemmeldungs-Nummer, Indizien-Byte 4",
		},
	},

	{
		0x0088,
		"Datenverfaelschung bei der Uebergabe der in den Indizien angegebenen Systemmeldungs-Nummer. (Indizienspeicher-Ende wurde ueberschrieben, in den ZIB werden Anwender-Systemmeldungs-Nummer und Anwender- Indizien uebernommen)",
		10,
		{
			"Anwender-Systemmeldungs-Nummer (High-Byte)",
			"Anwender-Systemmeldungs-Nummer (Low-Byte)",
			"Anwender-ROM-Speicherbank-Nummer",
			"Anwender-Befehlszaehler (High-Byte)",
			"Anwender-Befehlszaehler (Low-Byte)",
			"Siehe Beschreibung zu obiger Systemmeldungs-Nummer, Indizien-Byte 0",
			"Siehe Beschreibung zu obiger Systemmeldungs-Nummer, Indizien-Byte 1",
			"Siehe Beschreibung zu obiger Systemmeldungs-Nummer, Indizien-Byte 2",
			"Siehe Beschreibung zu obiger Systemmeldungs-Nummer, Indizien-Byte 3",
			"Siehe Beschreibung zu obiger Systemmeldungs-Nummer, Indizien-Byte 4",
		},
	},

	{
		0x0089,
		"Ueberlauf des Systemmeldungs-Speichers (Ringpuffer ist voll, SW-Reset, in den ZIB werden die Anwender-Systemmeldungs-Nummer und Anwender-Indizien uebernommen)",
		10,
		{
			"Anwender-Systemmeldungs-Nummer (High-Byte)",
			"Anwender-Systemmeldungs-Nummer (Low-Byte)",
			"Anwender-ROM-Speicherbank-Nummer",
			"Anwender-Befehlszaehler (High-Byte)",
			"Anwender-Befehlszaehler (Low-Byte)",
			"Siehe Beschreibung zu obiger Systemmeldungs-Nummer, Indizien-Byte 0",
			"Siehe Beschreibung zu obiger Systemmeldungs-Nummer, Indizien-Byte 1",
			"Siehe Beschreibung zu obiger Systemmeldungs-Nummer, Indizien-Byte 2",
			"Siehe Beschreibung zu obiger Systemmeldungs-Nummer, Indizien-Byte 3",
			"Siehe Beschreibung zu obiger Systemmeldungs-Nummer, Indizien-Byte 4",
		},
	},

	{
		0x008a,
		"Datenverfaelschung bei der Uebergabe der in den Indizien angegebenen Systemmeldungs-Nummer. (uebergebene Systemmeldungs-Nummer ist groesser als maximale Systemmeldungs-Nummer, SW-Reset, in den ZIB werden Anwender-Systemmeldungs-Nummer und Anwender-Indizien ueber- nommen)",
		10,
		{
			"Anwender-Systemmeldungs-Nummer (High-Byte)",
			"Anwender-Systemmeldungs-Nummer (Low-Byte)",
			"Anwender-ROM-Speicherbank-Nummer",
			"Anwender-Befehlszaehler (High-Byte)",
			"Anwender-Befehlszaehler (Low-Byte)",
			"Siehe Beschreibung zu obiger Systemmeldungs-Nummer, Indizien-Byte 0",
			"Siehe Beschreibung zu obiger Systemmeldungs-Nummer, Indizien-Byte 1",
			"Siehe Beschreibung zu obiger Systemmeldungs-Nummer, Indizien-Byte 2",
			"Siehe Beschreibung zu obiger Systemmeldungs-Nummer, Indizien-Byte 3",
			"Siehe Beschreibung zu obiger Systemmeldungs-Nummer, Indizien-Byte 4",
		},
	},

	{
		0x008b,
		"Datenverfaelschung bei der Uebergabe der in den Indizien angegebenen Systemmeldungs-Nummer. (Bei der Behandlung der Systemmeldung wird eine unzulaessige Indizienlaenge im RP-Element erkannt)",
		10,
		{
			"Anwender-Systemmeldungs-Nummer (High-Byte)",
			"Anwender-Systemmeldungs-Nummer (Low-Byte)",
			"Anwender-ROM-Speicherbank-Nummer",
			"Anwender-Befehlszaehler (High-Byte)",
			"Anwender-Befehlszaehler (Low-Byte)",
			"Siehe Beschreibung zu obiger Systemmeldungs-Nummer, Indizien-Byte 0",
			"Siehe Beschreibung zu obiger Systemmeldungs-Nummer, Indizien-Byte 1",
			"Siehe Beschreibung zu obiger Systemmeldungs-Nummer, Indizien-Byte 2",
			"Siehe Beschreibung zu obiger Systemmeldungs-Nummer, Indizien-Byte 3",
			"Siehe Beschreibung zu obiger Systemmeldungs-Nummer, Indizien-Byte 4",
		},
	},

	{
		0x009e,
		"Datenverfaelschung in der DKV. (T001) (Erstanstoss des MUK im BS-Anlauf mit Signalisierung YSTOK  statt mit Signalisierung YSTAK)",
		1,
		{
			"Phys. Einr.-Nr. der FDS",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x009f,
		"Die angegebene Einrichtung quittiert Signalisierung der DKV nicht. (T140) (Wiederholter Timeout statt Uhrzeit-Quittung YUZQE oder YUDQB)",
		1,
		{
			"Phys. Einr.-Nr",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x00a7,
		"Timeout: Ausbleiben einer Signalisierung von der MSC fuehrt zu Verbindungsabbruch. Der Prozess beendet sich ueber die Enderoutine der VT",
		8,
		{
			"Physikalische Einrichtungs-Nummer der FDS",
			"#Opcode# der ausgebliebenen Signalisierung",
			"Ident-Nummer",
			"Nummer des vom Verbindungsabbruch betroffenen Teilnehmers .low  Byte",
			"Nummer des vom Verbindungsabbruch betroffenen Teilnehmers .high Byte",
			"Nationalitaet und MSC - Nummer des betroffenen Teilnehmers",
			"Prozess-Adresse      high Byte",
			"Prozess-Adresse      low  Byte",
			"", "",
		},
	},

	{
		0x00a8,
		"Timeout: Die Enderoutine der VT wurde durch das DKV-OS wegen dem Ausbleiben einer erwarteten Signalisierung aktiviert.",
		8,
		{
			"Physikalische Einrichtungs-Nummer der FDS",
			"#Opcode# der ausgebliebenen Signalisierung",
			"Ident-Nummer",
			"Nummer des vom Verbindungsabbruch betroffenen Teilnehmers .low  Byte",
			"Nummer des vom Verbindungsabbruch betroffenen Teilnehmers .high Byte",
			"Nationalitaet und MSC - Nummer des betroffenen Teilnehmers",
			"Prozess-Adresse      high Byte",
			"Prozess-Adresse      low  Byte",
			"", "",
		},
	},

	{
		0x00a9,
		"Timeout: Ausbleiben einer Signalisierung vom FME. fuehrt zu Verbindungsabbruch",
		8,
		{
			"Physikalische Einrichtungs-Nummer des FME",
			"#Opcode# der ausgebliebenen Signalisierung",
			"Ident-Nummer",
			"Nummer des vom Verbindungsabbruch betroffenen Teilnehmers .low  Byte",
			"Nummer des vom Verbindungsabbruch betroffenen Teilnehmers .high Byte",
			"Nationalitaet und MSC - Nummer des betroffenen Teilnehmers",
			"Prozess-Adresse      high Byte",
			"Prozess-Adresse      low  Byte",
			"", "",
		},
	},

	{
		0x00aa,
		"Timeout: Ausbleiben einer Signalisierung \"Wahlbestaetigung-Positiv\" vom OGK fuehrt zu Verbindungsabbruch",
		8,
		{
			"Physikalische Einrichtungs-Nummer der FDS",
			"#Opcode# der ausgebliebenen Signalisierung",
			"Ident-Nummer",
			"Nummer des vom Verbindungsabbruch betroffenen Teilnehmers .low  Byte",
			"Nummer des vom Verbindungsabbruch betroffenen Teilnehmers .high Byte",
			"Nationalitaet und MSC - Nummer des betroffenen Teilnehmers",
			"Prozess-Adresse      high Byte",
			"Prozess-Adresse      low  Byte",
			"", "",
		},
	},

	{
		0x00ab,
		"Timeout: Ausbleiben einer Signalisierung vom SPK. fuehrt zu Verbindungsabbruch",
		8,
		{
			"Physikalische Einrichtungs-Nummer des SPK",
			"#Opcode# der ausgebliebenen Signalisierung",
			"Ident-Nummer",
			"Nummer des vom Verbindungsabbruch betroffenen Teilnehmers .low  Byte",
			"Nummer des vom Verbindungsabbruch betroffenen Teilnehmers .high Byte",
			"Nationalitaet und MSC - Nummer des betroffenen Teilnehmers",
			"Prozess-Adresse      high Byte",
			"Prozess-Adresse      low  Byte",
			"", "",
		},
	},

	{
		0x00ac,
		"Timeout: Ausbleiben einer Signalisierung vom MSC kein Uebergang auf die Enderoutine verlangt, der. zustaendige Prozess wird auf normalen Weg beendet ( Verbindungsabbruch )",
		8,
		{
			"Physikalische Einrichtungs-Nummer der FDS",
			"#Opcode# der ausgebliebenen Signalisierung",
			"Ident-Nummer",
			"Nummer des vom Verbindungsabbruch betroffenen Teilnehmers .low  Byte",
			"Nummer des vom Verbindungsabbruch betroffenen Teilnehmers .high Byte",
			"Nationalitaet und MSC - Nummer des betroffenen Teilnehmers",
			"Prozess-Adresse      high Byte",
			"Prozess-Adresse      low  Byte",
			"", "",
		},
	},

	{
		0x00ad,
		"Der WSV-Prozess ZWS100 wurde bereits 10 S lang nicht aufgerufen (normal = 600 mS)",
		2,
		{
			"Physikalische Einrichtungs-Nummer der FDS",
			"Frei",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x00ae,
		"Falscher Opcode oder Ereignis-Typ beim Signalisierungsempfang (Empfang eines Auftrages) vom PHE",
		5,
		{
			"FDS-Nr",
			"#Ereignis-Typ#",
			"FKS-Nummer",
			"#Ident-Nummer#",
			"#Opcode#",
			"", "", "", "", "",
		},
	},

	{
		0x00af,
		"Interne Signalisierung 'TFGAI' konnte nicht eingetragen werden",
		1,
		{
			"FDS-Nr",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x00b0,
		"Falscher Opcode oder Ereignis-Typ beim Signal- isierungsempfang (Empfang eines Auftrages) vom PHE",
		5,
		{
			"FDS-Nr",
			"#Ereignis-Typ#",
			"FKS-Nr",
			"#Ident-Nummer#",
			"#Opcode#",
			"", "", "", "", "",
		},
	},

	{
		0x00b1,
		"Falscher Opocde oder Ereignistyp beim Empfang einer DKV-internen Signalisierung",
		5,
		{
			"FDS-Nr",
			"#Ereignis-Typ#",
			"FKS-Nummer",
			"#Ident-Nummer#",
			"#Opcode#",
			"", "", "", "", "",
		},
	},

	{
		0x00b2,
		"Falscher Opcode oder Ereignis-Typ beim Empfang einer DKV-internen Signalisierung",
		5,
		{
			"FDS-Nr",
			"#Ereignis-Typ#",
			"FKS-Nummer",
			"#Ident-Nummer#",
			"#Opcode#",
			"", "", "", "", "",
		},
	},

	{
		0x00b3,
		"Falscher Opcode beim Empfang der Quittung von einer peripheren Einrichtung",
		3,
		{
			"FDS-Nr",
			"FKS-Nummer",
			"#Opcode#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x00b4,
		"Falscher Ereignis-Typ beim Empfang der Quittung von einer peripheren Einrichtung",
		5,
		{
			"FDS-Nr",
			"#Ereignis-Typ#",
			"FKS-Nummer",
			"#Ident-Nummer#",
			"#Opcode#",
			"", "", "", "", "",
		},
	},

	{
		0x00b5,
		"Falscher Opcode oder Ereignis-Typ beim Signal- isierungsempfang (Empgang eines Auftrages) vom PHE",
		5,
		{
			"FDS-Nr",
			"#Ereignis-Typ#",
			"FKS-Nummer",
			"#Ident-Nummer#",
			"#Opcode#",
			"", "", "", "", "",
		},
	},

	{
		0x00b6,
		"Falscher Einrichtuns-Typ bei Auftrags-Aussendung an eine periphere Einrichtung",
		2,
		{
			"FDS-Nr",
			"#Einrichtungs-Typ#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x00b7,
		"Unzulaessiger Return-Code aus einem Unterprogramm (Systemmeldungs-Nummer wird in diesem Modul mehrfach  benutzt ! )",
		4,
		{
			"FDS-Nr",
			"Prozedur-Identifikation",
			"Prozedur-Identifikation",
			"#Return-Code der FT#",
			"", "", "", "", "", "",
		},
	},

	{
		0x00b8,
		"Einrichtungsfehler, Quittung von falscher Einrichtung",
		4,
		{
			"#Phys. Einr.-Nr.# aus Signalisierung",
			"erwartete #Phys. Einr.-Nr.#",
			"Prozess-Zustand (#LDTSTA#)",
			"#Ident-Nummer# aus Signalisierung",
			"", "", "", "", "", "",
		},
	},

	{
		0x00b9,
		"Falscher Aufruf ohne Signalisierung: es wurde ein fuer den betreffenden Prozesszustand un- erlaubter Ereignis-Typ erkannt, zu dem es keine Signalisierung gibt (Timeout oder Ready)",
		4,
		{
			"FDS-Nr",
			"Prozess-Zustand (#LDTSTA#)",
			"#Ereignis-Typ#",
			"#Ident-Nummer# der Task",
			"", "", "", "", "", "",
		},
	},

	{
		0x00ba,
		"Falscher Aufruf mit Signalisierung es wurde ein fuer den betreffenden Prozesszustand unerlaubter Ereignis-Typ erkannt, zu dem es eine Signalisierung gibt",
		6,
		{
			"FDS-Nr",
			"Prozess-Zustand (#LDTSTA#)",
			"#Ereignis-Typ#",
			"#Ident-Nummer# der Task",
			"#Opcode#",
			"#Ident-Nummer# aus Signalisierung",
			"", "", "", "",
		},
	},

	{
		0x00bb,
		"Zustandsfehler ohne Signalisierung: es wurde ein unerlaubter Prozesszustand erkannt, bei dem keine Signalisierung existiert (Prozesszustand \"Signalisierung aussenden\")",
		4,
		{
			"FDS-Nr",
			"Prozesszustand (#LDTSTA#)",
			"#Ereignis-Typ#",
			"#Ident-Nummer# der Task",
			"", "", "", "", "", "",
		},
	},

	{
		0x00bc,
		"Zustandsfehler mit Signalisierung: es wurde ein unerlaubter Prozesszustand erkannt, bei dem eine Signalisierung existiert (Prozesszustand \"Quittung bearbeiten\")",
		6,
		{
			"FDS-Nr",
			"Prozesszustand (#LDTSTA#)",
			"#Ereignis-Typ#",
			"#Ident-Nummer# der Task",
			"#Opcode#",
			"#Ident-Nummer# aus Signalisierung",
			"", "", "", "",
		},
	},

	{
		0x00bd,
		"Falscher Ereignis-Typ oder Opcode beim Empfang der Quittung von der STBFDS",
		5,
		{
			"FDS-Nr",
			"#Ereignis-Typ#",
			"FKS-Nr.",
			"#Ident-Nummer#",
			"#Opcode#",
			"", "", "", "", "",
		},
	},

	{
		0x00be,
		"Ausbleiben der Quittung von der STBFDS",
		3,
		{
			"FDS-Nr",
			"#Ereignis-Typ#",
			"#Globaler ST-Zustand# der anderen FDS",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x00bf,
		"Falscher Ereignis-Typ oder Opcode beim Signali- sierungsempfang in der STBFDS",
		5,
		{
			"FDS-Nr",
			"#Ereignis-Typ#",
			"FKS-Nr.",
			"#Ident-Nummer#",
			"#Opcode#",
			"", "", "", "", "",
		},
	},

	{
		0x00c3,
		"Die Signalisierung an die STBFDS konnte nicht gesendet werden. (Negativer Return-Code von Makro WMESEP) DMA-Sperre gesetzt",
		2,
		{
			"FDS-Nr",
			"#Return-Code von WMESEP#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x00c4,
		"Feldelement nicht leer (Ident-Nummer-  Zurueckgabe). Fehlerursache laesst sich nicht durch Indizien belegen!",
		1,
		{
			"DKV-Nummer",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x00c5,
		"Prozess ist schon in die Prozess-Liste  eingetragen. Fehlerursache laesst sich nicht durch Indizien belegen!",
		1,
		{
			"DKV-Nummer",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x00c6,
		"Prozess ist nicht in die Prozess-Liste eingetragen,  obwohl eine Signalisierung fuer die Ident-Nummer  gekommen ist.",
		4,
		{
			"DKV-Nummer",
			"#Ereignis-Typ#",
			"#Ident-Nummer#",
			"#Opcode#",
			"", "", "", "", "", "",
		},
	},

	{
		0x00c7,
		"Stackpointer stimmt nach Ablauf des Prozesses nicht mehr.",
		7,
		{
			"DKV-Nummer",
			"LOW-PC des letzten Prozesses",
			"HIGH-PC des letzten Prozesses",
			"Prozess-Status 1 des letzten Prozesses (LOW-TO)",
			"Prozess-Status 1 des letzten Prozesses (HIGH-TO)",
			"Prozess-Status 2 des letzten Prozesses",
			"Prozess-Status 3 des letzten Prozesses",
			"", "", "",
		},
	},

	{
		0x00c8,
		"Frequenz-Nr. zu gross. Fehler durch einen Prozess im letzten Funkblock oder RAM-Fehler",
		7,
		{
			"DKV-Nummer",
			"LOW-PC des letzten Prozesses",
			"HIGH-PC des letzten Prozesses",
			"Prozess-Status 1 des letzten Prozesses (LOW-TO)",
			"Prozess-Status 1 des letzten Prozesses (HIGH-TO)",
			"Prozess-Status 2 des letzten Prozesses",
			"Prozess-Status 3 des letzten Prozesses",
			"", "", "",
		},
	},

	{
		0x00c9,
		"Frequenz-Nr. im Unterprogramm ist zu gross. Fehler durch einen Prozess im letzten Funkblock oder RAM-Fehler",
		1,
		{
			"DKV-Nummer",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x00ca,
		"Frequenz-Nr. stimmt nicht Fehler durch einen Prozess im letzten Funkblock oder RAM-Fehler",
		1,
		{
			"DKV-Nummer",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x00cb,
		"Kein MSC-Ausgabe-Puffer mehr frei, obwohl  vom Prozess beantragt.",
		7,
		{
			"DKV-Nummer",
			"LOW-PC des letzten Prozesses",
			"HIGH-PC des letzten Prozesses",
			"Prozess-Status 1 des letzten Prozesses (LOW-TO)",
			"Prozess-Status 1 des letzten Prozesses (HIGH-TO)",
			"Prozess-Status 2 des letzten Prozesses",
			"Prozess-Status 3 des letzten Prozesses",
			"", "", "",
		},
	},

	{
		0x00cc,
		"Kein OGK-Ausg.-Puffer mehr frei, obwohl  von Prozess beantragt (auch SPK-Ausg.-Puffer  in Verbindung mit OGK-Ausg.-Puffer). Indizien lassen nur bedingt auf Fehler schliessen!",
		7,
		{
			"DKV-Nummer",
			"LOW-PC des letzten Prozesses",
			"HIGH-PC des letzten Prozesses",
			"Prozess-Status 1 des letzten Prozesses (LOW-TO)",
			"Prozess-Status 1 des letzten Prozesses (HIGH-TO)",
			"Prozess-Status 2 des letzten Prozesses",
			"Prozess-Status 3 des letzten Prozesses",
			"", "", "",
		},
	},

	{
		0x00cd,
		"Kein PGS-Ausg.-Puffer mehr frei, obwohl  vom Prozess beantragt. Indizien lassen nur bedingt auf Fehler schliessen!",
		7,
		{
			"DKV-Nummer",
			"LOW-PC des letzten Prozesses",
			"HIGH-PC des letzten Prozesses",
			"Status-Byte 1 des letzten Prozesses (LOW-TO)",
			"Status-Byte 1 des letzten Prozesses (HIGH-TO)",
			"Status-Byte 2 des letzten Prozesses",
			"Status-Byte 3 des letzten Prozesses",
			"", "", "",
		},
	},

	{
		0x00ce,
		"Signalisierung ist nicht initiierend.",
		4,
		{
			"DKV-Nummer",
			"#Ereignis-Typ#",
			"#Ident-Nummer#",
			"#Opcode#",
			"", "", "", "", "", "",
		},
	},

	{
		0x00cf,
		"Keine Ident-Nummer mehr frei.",
		1,
		{
			"DKV-Nummer",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x00d0,
		"Es wurde vom Prozess als TO-Wert ans OS  null oder eins uebergeben.",
		7,
		{
			"DKV-Nummer",
			"LOW-PC des letzten Prozesses",
			"HIGH-PC des letzten Prozesses",
			"Prozess-Status 1 des letzten Prozesses (LOW-TO)",
			"Prozess-Status 1 des letzten Prozesses (HIGH-TO)",
			"Prozess-Status 2 des letzten Prozesses",
			"Prozess-Status 3 des letzten Prozesses",
			"", "", "",
		},
	},

	{
		0x00d1,
		"Es wurde versucht, Ident-Nummer 0 oder eine  feste Ident-Nummer zurueckzugeben .",
		8,
		{
			"DKV-Nummer",
			"LOW-PC des letzten Prozesses",
			"HIGH-PC des letzten Prozesses",
			"Prozess-Status 1 des letzten Prozesses (LOW-TO)",
			"Prozess-Status 1 des letzten Prozesses (HIGH-TO)",
			"Prozess-Status 2 des letzten Prozesses",
			"Prozess-Status 3 des letzten Prozesses",
			"#Ident-Nummer# des letzten Prozesses",
			"", "",
		},
	},

	{
		0x00d2,
		"Unbekannter NSTATE-Typ. (Beim Aufruf des NSTATE-Makros wurde eine falsche oder zu grosse CASE-Nr. uebergeben.).",
		7,
		{
			"DKV-Nummer",
			"LOW-PC des letzten Prozesses",
			"HIGH-PC des letzten Prozesses",
			"Prozess-Status 1 des letzten Prozesses (LOW-TO)",
			"Prozess-Status 1 des letzten Prozesses (HIGH-TO)",
			"Prozess-Status 2 des letzten Prozesses",
			"Prozess-Status 3 des letzten Prozesses",
			"", "", "",
		},
	},

	{
		0x00d3,
		"Bei der Prozesskommunikationspufferabarbeitung  war keine Ident-Nummer mehr frei.",
		1,
		{
			"DKV-Nummer",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x00d4,
		"Prozess ist schon in der Prozessliste eingetragen  (Prozesskommunikationspufferabarbeitung). Ein Fehler in der Ident-Nummern-Verwaltung liegt vor (RAM ueberschrieben). Fehlerursache laesst sich nicht durch Indizien belegen!",
		1,
		{
			"DKV-Nummer",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x00d5,
		"Falscher Ereignis-Typ Indizien lassen nur bedingt auf Fehler schliessen!",
		4,
		{
			"DKV-Nummer",
			"#Ereignis-Typ#",
			"#Ident-Nummer#",
			"#Opcode#",
			"", "", "", "", "", "",
		},
	},

	{
		0x00d6,
		"Prozesskommunikationspuffer voll.",
		7,
		{
			"DKV-Nummer",
			"LOW-PC des letzten Prozesses",
			"HIGH-PC des letzten Prozesses",
			"Prozess-Status 1 des letzten Prozesses (LOW-TO)",
			"Prozess-Status 1 des letzten Prozesses (HIGH-TO)",
			"Prozess-Status 2 des letzten Prozesses",
			"Prozess-Status 3 des letzten Prozesses",
			"", "", "",
		},
	},

	{
		0x00d7,
		"Datenverfaelschung in der DKV. (T001) (Erhaltene Signalisierung stimmt nicht mit erwarteter Signalisierung ueberein. ( Opcode falsch ))",
		6,
		{
			"Phys. Einr.-Nr. der FDS",
			"Prozedur-Identifikation des betreffenden Moduls : #Entwickler-Info#",
			"#Ereignis-Typ#",
			"Byte B der erhaltenen Signalisierung : #Entwickler-Info#",
			"Byte A der erhaltenen Signalisierung : #Entwickler-Info#",
			"#Opcode# der erhaltenen Signalisierung",
			"", "", "", "",
		},
	},

	{
		0x00d8,
		"Datenverfaelschung in der DKV. (T001) (Prozessanstoss mit nicht erwartetem Ereignis-Typ  MZTO oder MREADY)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"Prozedur-Identifikation des betreffenden Moduls : #Entwickler-Info#",
			"#Ereignis-Typ#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x00d9,
		"Datenverfaelschung in der DKV. (T001) (Prozessanstoss mit nicht erwartetem Ereignis-Typ)",
		6,
		{
			"Phys. Einr.-Nr. der FDS",
			"Prozedur-ID (zur Identifikation des betreffenden Moduls)",
			"#Ereignis-Typ#",
			"Byte B der erhaltenen Signalisierung : #Entwickler-Info#",
			"Byte A der erhaltenen Signalisierung : #Entwickler-Info#",
			"#Opcode# der erhaltenen Signalisierung",
			"", "", "", "",
		},
	},

	{
		0x00da,
		"Datenverfaelschung in der DKV. (T001) (Erhaltene Signalisierung stimmt nicht mit erwarteter  Signalisierung ueberein (Opcode oder Absender, d.h.  physikalische Einrichtungsnummer, falsch). )",
		6,
		{
			"Phys. Einr.-Nr. der FDS",
			"Prozedur-Identifikation des betreffenden Moduls : #Entwickler-Info#",
			"#Ereignis-Typ#",
			"Byte B der erhaltenen Signalisierung : #Entwickler-Info#",
			"Byte A der erhaltenen Signalisierung : #Entwickler-Info#",
			"#Opcode# der erhaltenen Signalisierung",
			"", "", "", "",
		},
	},

	{
		0x00dc,
		"Datenverfaelschung in der DKV. (T001) (Erhaltener Ereignis-Typ stimmt nicht mit dem erwarteten ueberein. ( Ereignis-Typ oder Opcode der Signalisierung falsch ))",
		6,
		{
			"Phys. Einr.-Nr. der FDS",
			"Prozedur-Identifikation des betreffenden Moduls : #Entwickler-Info#",
			"#Ereignis-Typ#",
			"Byte B der erhaltenen Signalisierung : #Entwickler-Info#",
			"Byte A der erhaltenen Signalisierung : #Entwickler-Info#",
			"#Opcode# der erhaltenen Signalisierung",
			"", "", "", "",
		},
	},

	{
		0x00de,
		"Datenverfaelschung bei der Datenuebertragung von der aktiven in die passive FDS beim Aendern der Anlagen- liste. (T064) (Die bei Transfer der AL-Aenderungen an die STBFDS er- haltene Signalisierungs-Folgenummer einer Signalisierung stimmt nicht mit der erwarteten ueberein.)",
		4,
		{
			"Phys. Einr.-Nr. der FDS",
			"Prozedur-Identifikation : #Entwickler-Info#",
			"Erhaltene Signalisierungs-Folgenummer (Istwert)",
			"Erwartete Signalisierungs-Folgenummer (Sollwert)",
			"", "", "", "", "", "",
		},
	},

	{
		0x00df,
		"Datenverfaelschung bei der Datenuebertragung von der FUPEF an die FDS bei der Software-Objektnamensausgabe aus der FUPEF. (T065) (Die von der FUPEF erhaltene Signalisierungs-Folgenummer stimmt nicht mit der erwarteten ueberein. )",
		5,
		{
			"Phys. Einr.-Nr. der FDS",
			"Prozedur-Identifikation : #Entwickler-Info#",
			"#Ereignis-Typ#",
			"Erhaltene Signalisierungs-Folgenummer (Istwert)",
			"Erwartete Signalisierungs-Folgenummer (Sollwert)",
			"", "", "", "", "",
		},
	},

	{
		0x00e0,
		"Datenverfaelschung bei der Ausgabe von AL-Parametern. (T114) (Absender der Auftragssignalisierung XEPAB (Ausgeben AL- Parameter an PBR) falsch ( d. h. Phys. Einr.-Nr. falsch))",
		5,
		{
			"Phys. Einr.-Nr. der FDS",
			"Prozedur-Identifikation : #Entwickler-Info#",
			"#Ereignis-Typ#",
			"#Opcode#",
			"Phys. Einr.-Nr",
			"", "", "", "", "",
		},
	},

	{
		0x00e1,
		"SW-Fehler in der DKV. (T002) (negativer Return-Code vom OS-UP WTAKOM bei Pufferanforderung fuer eine DKV-interne Signalisierung )",
		5,
		{
			"Phys. Einr.-Nr. der FDS",
			"Prozedur-Identifikation des betreffenden Moduls : #Entwickler-Info#",
			"#Return-Code von WTAKOM#",
			"",
			"",
			"", "", "", "", "",
		},
	},

	{
		0x00e3,
		"Bei Parameteraenderung wird BS-Anlauf durchgefuehrt. (T113)",
		2,
		{
			"Phys. Einr.-Nr. der FDS",
			"Prozedur-Identifikation des betreffenden Moduls : #Entwickler-Info#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x00e4,
		"Datenverfaelschung in der DKV. (T001) (negativer Return-Code vom OS-UP WTAKOM bei Pufferanforderung fuer eine DKV-interne Signalisierung )",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"Prozedur-Identifikation des betreffenden Moduls : #Entwickler-Info#",
			"#Return-Code von WTAKOM#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x00e5,
		"Datenverfaelschung bei der DKV-internen Prozesskommunikation (T066) (Die im ACT-Auftrag an die KON uebergebene log. Einr.-Nr. und der Einrichtungs-Typ stimmen nicht mit denen in der KON-Quittung ueberein.)",
		6,
		{
			"Phys. Einr.-Nr. der FDS",
			"Prozedur-Identifikation des betreffenden Moduls : #Entwickler-Info#",
			"#Ereignis-Typ#",
			"#Opcode#",
			"#Einrichtungs-Typ# aus KON-Quittung",
			"#Log. Einr.-Nr.#   aus KON-Quittung",
			"", "", "", "",
		},
	},

	{
		0x00e6,
		"Datenverfaelschung bei der Ausgabe von Softwareobjektnamen. (T115) (Bei Ueberpruefung der Signalisierungs-Folgenummer wurde ein nicht plausibler Wert festgestellt)",
		4,
		{
			"Phys. Einr.-Nr. der FDS",
			"Prozedur-Identifikation : #Entwickler-Info#",
			"#Ereignis-Typ#",
			"Erhaltene Signalisierungs-Folgenummer (Istwert)",
			"", "", "", "", "", "",
		},
	},

	{
		0x00e7,
		"Datenverfaelschung bei der DKV-internen Prozesskommunikation (T066) (Nach dreimaligem ACT-Auftrag an die KON bleibt die Quittung der KON aus.)",
		4,
		{
			"Phys. Einr.-Nr. der FDS",
			"Prozedur-Identifikation des betreffenden Moduls : #Entwickler-Info#",
			"#Einrichtungs-Typ#",
			"#Log. Einr.-Nr.#",
			"", "", "", "", "", "",
		},
	},

	{
		0x00e8,
		"Datenverfaelschung bei der DKV-internen Prozesskommunikation (T066) (ACT-Quittung der KON enthaelt Return-Code 'Para-  meterfehler')",
		7,
		{
			"Phys. Einr.-Nr. der FDS",
			"Prozedur-Identifikation des betreffenden Moduls : #Entwickler-Info#",
			"#Ereignis-Typ#",
			"#Opcode#",
			"Ident-Nr. des Absenders",
			"#Einrichtungs-Typ# der Einrichtung aus KON-Quittung",
			"#Log. Einr.-Nr.# der Einrichtung aus KON-Quittung",
			"", "", "",
		},
	},

	{
		0x00e9,
		"Bei der Durchfuehrung  eines Anlagenlisten- aenderungsauftrags (PHE-Anlauf) sind auch nach einer Wartezeit von 1 Minute nicht beide PHEs funktechnisch verfuegbar.(T067)",
		4,
		{
			"Phys. Einr.-Nr. der FDS",
			"Prozedur-Identifikation des betreffenden Moduls : #Entwickler-Info#",
			"#Globaler ST-Zustand# des PHE01",
			"#Globaler ST-Zustand# des PHE02",
			"", "", "", "", "", "",
		},
	},

	{
		0x00fa,
		"Datenverfaelschung  im  Anlauf  mit der  angegebenen Einrichtung. (T044) (Prozessanstoss mit unzulaessigem Ereignis-Typ)",
		4,
		{
			"Phys. Einr.-Nr. aus Signalisierung",
			"#Ereignis-Typ#",
			"#Opcode#",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x00fb,
		"Datenverfaelschung  im  Anlauf  mit der  angegebenen Einrichtung. (T044) (PBR-Anlauf-Auftrag (YAAB): falscher Einrichtungs-Typ)",
		3,
		{
			"Phys. Einr.-Nr. aus Signalisierung",
			"#Einrichtungs-Typ#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x00fc,
		"Datenverfaelschung  im  Anlauf  mit der  angegebenen Einrichtung. (T044) (Anlauf mit dem PBR: Prozessanstoss mit  unzulaessigem Opcode (/=OYAAB))",
		4,
		{
			"Phys. Einr.-Nr. aus Signalisierung",
			"#Ereignis-Typ#",
			"#Opcode#",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x00fd,
		"Datenverfaelschung  im  Anlauf  mit der  angegebenen Einrichtung. (T044) (Anlauf mit dem PBR:  Betriebs-Pararameter-Quittung (YPBQB) kam von falscher  Einrichtung)",
		4,
		{
			"Phys. Einr.-Nr. aus Signalisierung (Istwert)",
			"Phys. Einr.-Nr. (LODEAD)   (Sollwert)",
			"#Einrichtungs-Typ# (LODETY)",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x00fe,
		"Datenverfaelschung  im  Anlauf  mit der  angegebenen Einrichtung. (T044) (Anlauf mit dem PBR:  Betriebs-Pararameter-Quittung (YPBQB) kam von falscher  Einrichtung)",
		4,
		{
			"Phys. Einr.-Nr. aus Signalisierung  (Istwert)",
			"Phys. Einr.-Nr. (LODEAD)    (Sollwert)",
			"#Einrichtungs-Typ# (LODETY)",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x00ff,
		"Datenverfaelschung  im  Anlauf  mit der  angegebenen Einrichtung. (T044) (Anlauf mit dem PBR: Prozessanstoss mit unzulaessigem Ereignis-Typ fuer laufenden Prozesszustand)",
		4,
		{
			"Phys. Einr.-Nr. (LODEAD)",
			"#Ereignis-Typ#",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x0100,
		"Datenverfaelschung  im  Anlauf  mit der  angegebenen Einrichtung. (T044) (Anlauf mit dem PBR: Prozessanstoss mit unzulaessigem  Ereignis-Typ fuer laufenden Prozesszustand)",
		4,
		{
			"Phys. Einr.-Nr. (LODEAD)",
			"#Ereignis-Typ#",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x0101,
		"Datenverfaelschung im Anlauf mit der im BYTE 1 angegebenen Einrichtung. (T041) (Anlauf mit dem PBR: Prozessanstoss bei unzulaessigem Prozesszustand und beliebigem Ereignis-Typ)",
		5,
		{
			"Phys. Einr.-Nr. der FDS",
			"Phys. Einr.-Nr. (LODEAD)",
			"#Ereignis-Typ#",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"eigene Ident-Nr",
			"", "", "", "", "",
		},
	},

	{
		0x0103,
		"Datenverfaelschung bei der Datenuebertragung vom MSC. (T005) (Prozessanstoss mit nicht erwarteter Ereignis-Typ)",
		5,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Ereignis-Typ#",
			"Ident-Nr. des Absenders aus Signalisierung",
			"#Opcode#",
			"eigene Ident-Nr",
			"", "", "", "", "",
		},
	},

	{
		0x0105,
		"Datenverfaelschung  im  Anlauf  mit der  angegebenen Einrichtung. (T044) (Anlauf mit dem PHE: Prozessanstoss mit unzulaessigem Ereignis-Typ )",
		4,
		{
			"Phys. Einr.-Nr. aus Signalisierung",
			"#Ereignis-Typ#",
			"#Opcode#",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x0106,
		"Datenverfaelschung  im  Anlauf  mit der  angegebenen Einrichtung. (T044) (PHE-Anlauf-Auftrag (YAAE): falscher Einrichtungs-Typ)",
		3,
		{
			"Phys. Einr.-Nr. aus Signalisierung",
			"#Einrichtungs-Typ#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0107,
		"Datenverfaelschung  im  Anlauf  mit der  angegebenen Einrichtung. (T044) (Anlauf mit dem PHE: Prozessanstoss mit unzulaessigem Opcode)",
		4,
		{
			"Phys. Einr.-Nr. aus Signalisierung",
			"#Ereignis-Typ#",
			"#Opcode#",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x0108,
		"Datenverfaelschung  im  Anlauf  mit der  angegebenen Einrichtung. (T044) (Anlauf mit dem PHE: Betriebs-Parameter-Quittung (YPBQV) kam von falscher Einrichtung)",
		4,
		{
			"Phys. Einr.-Nr. aus Signalisierung  (Istwert)",
			"Phys. Einr.-Nr. (LODEAD)    (Sollwert)",
			"#Einrichtungs-Typ#",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x0109,
		"Datenverfaelschung  im  Anlauf  mit der  angegebenen Einrichtung. (T044) (Anlauf mit dem PHE: Betriebs-Parameter-Quittung (YPBQV) kam von falscher Einrichtung)",
		4,
		{
			"Phys. Einr.-Nr. aus Signalisierung  (Istwert)",
			"Phys. Einr.-Nr. (LODEAD)    (Sollwert)",
			"#Einrichtungs-Typ#",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x010a,
		"Datenverfaelschung  im  Anlauf  mit der  angegebenen Einrichtung. (T044) (Prozessanstoss mit unzulaessigem Ereignis-Typ fuer laufenden Prozesszustand)",
		4,
		{
			"Phys. Einr.-Nr. (LODEAD)",
			"#Ereignis-Typ#",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x010b,
		"Datenverfaelschung  im  Anlauf  mit der  angegebenen Einrichtung. (T044) (Anlauf mit dem PHE: Prozessanstoss mit unzulaessigem Ereignis-Typ fuer laufenden Prozesszustand)",
		4,
		{
			"Phys. Einr.-Nr. (LODEAD)",
			"#Ereignis-Typ#",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x010c,
		"Datenverfaelschung im Anlauf mit der im BYTE 1 angegebenen Einrichtung. (T041) (Anlauf mit dem PHE: Prozessanstoss bei unzulaessigem Prozesszustand mit beliebigem Ereignis-Typ)",
		5,
		{
			"Phys. Einr.-Nr. der FDS",
			"Phys. Einr.-Nr. (LODEAD)",
			"#Ereignis-Typ#",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"eigene Ident-Nr",
			"", "", "", "", "",
		},
	},

	{
		0x010e,
		"Datenverfaelschung im BS-Anlauf. (T055) (unzulaessiger Return-Code vom OS-UP WTAKOM bei Pufferanforderung fuer die DKV-interne Signalisierung YKOAI)",
		2,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code von WTAKOM#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x010f,
		"Datenverfaelschung im BS-Anlauf. (T055) (unzulaessiger Return-Code vom OS-UP WTAKOM bei Pufferanforderung fuer die DKV-interne Signalisierung YKAAI)",
		4,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code von WTAKOM#",
			"(LODZFQ): Zaehler fehlender Quittungen fuer gegebene ACT-Auftraege : #Entwickler-Info#",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x0111,
		"Datenverfaelschung im BS-Anlauf. (T055) (unzulaessiger Return-Code vom OS-UP WTAKOM bei Pufferanforderung fuer die DKV-interne Signalisierung YIAAI)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code von WTAKOM#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0113,
		"Datenverfaelschung im BS-Anlauf. (T055) (Unzulaessiger Prozesszustand)",
		6,
		{
			"Phys. Einr.-Nr. der FDS",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"eigene Ident-Nr",
			"#Ereignis-Typ#",
			"#Opcode#",
			"Ident-Nr. des Absenders",
			"", "", "", "",
		},
	},

	{
		0x0114,
		"Datenverfaelschung im BS-Anlauf. (T055) (unzulaessiger Prozesszustand)",
		4,
		{
			"Phys. Einr.-Nr. der FDS",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"eigene Ident-Nr",
			"#Ereignis-Typ#",
			"", "", "", "", "", "",
		},
	},

	{
		0x0117,
		"Datenverfaelschung im BS-Anlauf. (T055) (Prozessanstoss mit falschem Ereignis-Typ bei Anstoss des ANL(ANK)-Steuermoduls YANKIS)",
		4,
		{
			"Phys. Einr.-Nr. der FDS",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"eigene Ident-Nr",
			"#Ereignis-Typ#",
			"", "", "", "", "", "",
		},
	},

	{
		0x0118,
		"Datenverfaelschung im BS-Anlauf. (T055) (Prozessanstoss mit falschem Ereignis-Typ oder Opcode bei Anstoss des ANL(ANK)-Steuermoduls YANKIS)",
		6,
		{
			"Phys. Einr.-Nr. der FDS",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"eigene Ident-Nr",
			"#Ereignis-Typ#",
			"#Opcode#",
			"Ident-Nr. des Absenders",
			"", "", "", "",
		},
	},

	{
		0x0119,
		"Datenverfaelschung im BS-Anlauf. (T055) (Prozessanstoss mit falschem Ereignis-Typ oder Opcode im Prozesszustand \"Warten-auf-Quittungen\")",
		6,
		{
			"Phys. Einr.-Nr. der FDS",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"eigene Ident-Nr",
			"#Ereignis-Typ#",
			"#Opcode#",
			"Ident-Nr. des absendenden Prozesses",
			"", "", "", "",
		},
	},

	{
		0x011a,
		"Datenverfaelschung im BS-Anlauf. (T055) (Prozessanstoss mit falschem Ereignis-Typ)",
		4,
		{
			"Phys. Einr.-Nr. der FDS",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"eigene Ident-Nr",
			"#Ereignis-Typ#",
			"", "", "", "", "", "",
		},
	},

	{
		0x011b,
		"Datenverfaelschung im BS-Anlauf. (T055) (Prozessanstoss mit falschem Ereignis-Typ oder Opcode im Prozesszustand \"Warten-auf-Quittungen\")",
		6,
		{
			"Phys. Einr.-Nr. der FDS",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"eigene Ident-Nr",
			"#Ereignis-Typ#",
			"#Opcode#",
			"Ident-Nr. des absendenden Prozesses",
			"", "", "", "",
		},
	},

	{
		0x011c,
		"Datenverfaelschung im BS-Anlauf. (T055) (Aus ANL-UP YPAKAU kommt unzulaessiger Return-Code zurueck.)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code ANL#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x011d,
		"Datenverfaelschung im BS-Anlauf. (T055) (Aus ANL-UP YPAKAE kommt unzulaessiger Return-Code zurueck)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code ANL#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x011e,
		"Datenverfaelschung im BS-Anlauf. (T055) (Aus ANL-UP YPAKQU kommt unzulaessiger Return-Code zurueck)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code ANL#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x011f,
		"Datenverfaelschung im BS-Anlauf. (T055) (Aus ANL-UP YPAKAE kommt unzulaessiger Return-Code zurueck)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code ANL#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0121,
		"Datenverfaelschung im BS-Anlauf. (T055) (Aus ANL-UP YPAKQS kommt unzulaessiger Return-Code zurueck)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code ANL#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0124,
		"Datenverfaelschung im BS-Anlauf. (T055) (Aus ANL-UP YPAKEF kommt unzulaessiger Return-Code zurueck)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code ANL#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0129,
		"Datenverfaelschung im BS-Anlauf. (T055) (Aus ANL-UP YPAKNE kommt unzulaessiger Return-Code zurueck)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code ANL#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x012a,
		"Datenverfaelschung im BS-Anlauf. (T055) (Aus ANL-UP YPAKNE kommt unzulaessiger Return-Code zurueck)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code ANL#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x012b,
		"Datenverfaelschung im BS-Anlauf. (T055) (Aus ANL-UP YPAKNE kommt unzulaessiger Return-Code zurueck)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code ANL#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x012c,
		"Datenverfaelschung im BS-Anlauf. (T055) (Aus ANL-UP YPAKQU kommt unzulaessiger Return-Code zurueck)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code ANL#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x012d,
		"Datenverfaelschung im BS-Anlauf. (T055) (Aus ANL-UP YPAKKQ kommt unzulaessiger Return-Code zurueck)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code ANL#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x012e,
		"Datenverfaelschung im BS-Anlauf. (T055) (Aus ANL-UP YPAKKQ kommt unzulaessiger Return-Code zurueck)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code ANL#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x012f,
		"Datenverfaelschung im BS-Anlauf. (T055) (Aus ANL-UP YPAKOP kommt unzulaessiger Return-Code zurueck)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code ANL#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0130,
		"Datenverfaelschung im BS-Anlauf. (T055) (Aus ANL-UP YPAKMQ kommt unzulaessiger Return-Code zurueck)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code ANL#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0131,
		"Datenverfaelschung im BS-Anlauf. (T055) (Aus ANL-UP YPAKNE kommt unzulaessiger Return-Code zurueck)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code ANL#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0132,
		"BS-Anlauf  nicht  erfolgreich wegen Datenverfaelschung oder SW-Fehlers. (T061) (ANK-interne Anlauf-Ueberwachung ist abgelaufen nach ca. 17 Minuten)",
		5,
		{
			"Phys. Einr.-Nr. der FDS",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"#Ereignis-Typ#",
			"#Globaler ST-Zustand# des MSC",
			"eigene Ident-Nr",
			"", "", "", "", "",
		},
	},

	{
		0x0133,
		"Datenverfaelschung im BS-Anlauf. (T055) (Aus ANL-UP YPAKNE kommt unzulaessiger Return-Code zurueck)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code ANL#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0136,
		"Datenverfaelschung im BS-Anlauf. (T055) (Fuer eine Einrichtung ungleich SPK ist der Globale ST-Zustand nicht ACT, PLA, MBL, SEZ oder UNA vor Erteilung eines Konfigurations-Auftrags nach ACT.)",
		5,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Einrichtungs-Typ#",
			"#Log. Einr.-Nr.#",
			"#Globaler ST-Zustand#",
			"eigene Ident-Nr",
			"", "", "", "", "",
		},
	},

	{
		0x0139,
		"Datenverfaelschung im BS-Anlauf. (T055) (Falscher Einrichtungs-Typ ist aufgetreten bei: \"Ermitteln naechste Einrichtung ungleich SPK\")",
		4,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Einrichtungs-Typ#",
			"#Log. Einr.-Nr.#",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x013d,
		"Datenverfaelschung im BS-Anlauf. (T055) (Negative ACT-Quittung YKAQI von der KON eingetroffen.)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code von YKAQI#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x013e,
		"BS-Anlauf  nicht  erfolgreich wegen Datenverfaelschung oder SW-Fehlers. (T061) (ANL(ANK)-Timer fuer Initialisierungsphase abgelaufen)",
		6,
		{
			"Phys. Einr.-Nr. der FDS",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"#Globaler ST-Zustand# des MSC",
			"#Detaillierter ST-Zustand# des MSC",
			"#ANK-Situationsanzeige#",
			"eigene Ident-Nr",
			"", "", "", "",
		},
	},

	{
		0x013f,
		"Datenverfaelschung im BS-Anlauf. (T055) (KON-Quittung YKAQI fuer eine Einrichtung, fuer die laut KOORDLISTE keine Quittung erwartet wurde.)",
		4,
		{
			"Phys. Einr.-Nr. der FDS",
			"Phys. Einr.-Nr",
			"#Eintrag aus KOORDLISTE#",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x0141,
		"Datenverfaelschung im BS-Anlauf. (T055) (KON-Quittung YKAQI fuer Einrichtung, fuer die laut KOORDLISTE keine Quittung erwartet  wurde; laut KON-Quittung ist fuer die Einrichtung die Konfigurationszeit abgelaufen)",
		4,
		{
			"Phys. Einr.-Nr. der FDS",
			"Phys. Einr.-Nr. aus der Signalisierung",
			"#Eintrag aus KOORDLISTE#",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x0142,
		"Der BS-Anlauf konnte nicht zu Ende gefuehrt werden, weil der Anlauf mit dem MSC entweder aufgrund ausblei- bender Signalisierungen vom MSC nicht abgeschlossen werden konnte oder wegen eines Beziehungsausfalls zum MSC. (T071) (negative KON-Quittung auf ACT-Auftrag fuer MSC, KON-Timeout abwarten)",
		5,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code von YKAQI#",
			"#Globaler ST-Zustand# des MSC",
			"#Detaillierter ST-Zustand# des MSC",
			"eigene Ident-Nr",
			"", "", "", "", "",
		},
	},

	{
		0x0143,
		"Datenverfaelschung im BS-Anlauf. (T055) (Eintreffen der KON-Quittung YKAQI fuer  die Einrichtung MSC, fuer die  laut KOORDLISTE keine Quittung erwartet wurde)",
		6,
		{
			"Phys. Einr.-Nr. der FDS",
			"Phys. Einr.-Nr",
			"#Eintrag aus KOORDLISTE#",
			"#Globaler ST-Zustand# des MSC",
			"#Detaillierter ST-Zustand# des MSC",
			"eigene Ident-Nr",
			"", "", "", "",
		},
	},

	{
		0x0144,
		"Im BS-Anlauf wird ein Konfigurationsauftrag nach ACT fuer eine Einrichtung negativ quittiert. (T072) (Eintreffen einer Quittung YKAQI mit dem Return-Code KYKARB : ACT-Auftrag wird bereits bearbeitet KYKABU : Abruch wegen UNA-Auftrag oder KYKUNA : ACT-Auftrag endet in UNA)",
		4,
		{
			"Phys. Einr.-Nr. der FDS",
			"Phys. Einr.-Nr. aus Signalisierung",
			"#Return-Code von YKAQI#",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x0145,
		"BS-Anlauf  nicht  erfolgreich wegen Datenverfaelschung oder SW-Fehlers. (T061) (ANL(ANK)-Timer fuer Anlauf der FUPEF-Einrichtungen und des PBR ist abgelaufen)",
		7,
		{
			"Phys. Einr.-Nr. der FDS",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"#Globaler ST-Zustand# des MSC",
			"#Detaillierter ST-Zustand# des MSC",
			"LOD(LODZFQ): Zaehler fehlender Quittungen fuer gegebene ACT-Auftraege : #Entwickler-Info#",
			"#ANK-Situationsanzeige#",
			"eigene Ident-Nr",
			"", "", "",
		},
	},

	{
		0x0146,
		"Datenverfaelschung  im  Anlauf  mit der  angegebenen Einrichtung. (T044) (Anlauf mit dem FME: Prozessanstoss mit unzulaessigem Ereignis-Typ)",
		4,
		{
			"Phys. Einr.-Nr. aus Signalisierung",
			"#Ereignis-Typ#",
			"#Opcode#",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x0147,
		"Datenverfaelschung  im  Anlauf  mit der  angegebenen Einrichtung. (T044) (FME-Anlauf-Auftrag (YAAM): falscher Einrichtungs-Typ)",
		3,
		{
			"Phys. Einr.-Nr. aus Signalisierung",
			"#Einrichtungs-Typ#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0148,
		"Datenverfaelschung  im  Anlauf  mit der  angegebenen Einrichtung. (T044) (FME-Anlauf-Auftrag (YAAM): Prozessanstoss mit unzulaessigem Opcode (/=OYAAM))",
		4,
		{
			"Phys. Einr.-Nr. aus Signalisierung",
			"#Ereignis-Typ#",
			"#Opcode#",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x0149,
		"Datenverfaelschung  im  Anlauf  mit der  angegebenen Einrichtung. (T044) (Anlauf mit dem FME: Betriebs-Parameter-Quittung YPBQV kam von falscher Einrichtung)",
		4,
		{
			"Phys. Einr.-Nr. aus Signalisierung  (Istwert)",
			"Phys. Einr.-Nr. (LODEAD)          (Sollwert)",
			"#Einrichtungs-Typ#",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x014a,
		"Datenverfaelschung  im  Anlauf  mit der  angegebenen Einrichtung. (T044) (Anlauf mit dem FME: Betriebs-Parameter-Quittung YPBQV kam von falscher Einrichtung)",
		4,
		{
			"Phys. Einr.-Nr. aus Signalisierung  (Istwert)",
			"Phys. Einr.-Nr. (LODEAD)          (Sollwert)",
			"#Einrichtungs-Typ#",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x014b,
		"Datenverfaelschung  im  Anlauf  mit der  angegebenen Einrichtung. (T044) (Anlauf mit dem FME: Prozessanstoss mit unzulaessigem Ereignis-Typ fuer laufenden Prozesszustand)",
		4,
		{
			"Phys. Einr.-Nr. (LODEAD)",
			"#Ereignis-Typ#",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x014c,
		"Datenverfaelschung  im  Anlauf  mit der  angegebenen Einrichtung. (T044) (Anlauf mit dem FME: Prozessanstoss mit unzulaessigem Ereignis-Typ fuer laufenden Prozesszustand)",
		4,
		{
			"Phys. Einr.-Nr. (LODEAD)",
			"#Ereignis-Typ#",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x014d,
		"Datenverfaelschung im Anlauf mit der im BYTE 1 angegebenen Einrichtung. (T041) (Anlauf mit dem FME: Unzulaessiger Prozesszustand bei Prozessanstoss mit beliebigem Ereignis)",
		5,
		{
			"Phys. Einr.-Nr. der FDS",
			"Phys. Einr.-Nr. (LODEAD)",
			"#Ereignis-Typ#",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"eigene Ident-Nr",
			"", "", "", "", "",
		},
	},

	{
		0x014e,
		"Datenverfaelschung  im  Anlauf  mit der  angegebenen Einrichtung. (T044) (Anlauf mit dem OGK: Prozessanstoss mit unzulaessigem Ereignis-Typ)",
		4,
		{
			"Phys. Einr.-Nr. aus Signalisierung",
			"#Ereignis-Typ#",
			"#Opcode#",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x014f,
		"Datenverfaelschung  im  Anlauf  mit der  angegebenen Einrichtung. (T044) (OGK-Anlauf-Auftrag (YAAO): falscher Einrichtungs-Typ)",
		3,
		{
			"Phys. Einr.-Nr. aus Signalisierung",
			"#Einrichtungs-Typ#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0150,
		"Datenverfaelschung  im  Anlauf  mit der  angegebenen Einrichtung. (T044) (Anlauf mit dem OGK: Prozessanstoss mit unzulaessigem Opcode)",
		4,
		{
			"Phys. Einr.-Nr. aus Signalisierung",
			"#Ereignis-Typ#",
			"#Opcode#",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x0151,
		"Datenverfaelschung  im  Anlauf  mit der  angegebenen Einrichtung. (T044) (Anlauf mit dem OGK: Betriebs-Parameter-Quittung YPBQV kam von falscher Einrichtung)",
		4,
		{
			"Phys. Einr.-Nr. aus Signalisierung",
			"Phys. Einr.-Nr. (LODEAD)",
			"#Einrichtungs-Typ#",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x0152,
		"Datenverfaelschung  im  Anlauf  mit der  angegebenen Einrichtung. (T044) (Anlauf mit dem OGK: Betriebs-Parameter-Quittung YPBQV kam von falscher Einrichtung)",
		4,
		{
			"Phys. Einr.-Nr. aus Signalisierung (Sollwert)",
			"Phys. Einr.-Nr. (LODEAD)         (Istwert)",
			"#Einrichtungs-Typ#",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x0153,
		"Datenverfaelschung  im  Anlauf  mit der  angegebenen Einrichtung. (T044) (Anlauf mit dem OGK: Prozessanstoss mit unzulaessigem Ereignis-Typ fuer laufenden Prozesszustand)",
		4,
		{
			"Phys. Einr.-Nr. (LODEAD)",
			"#Ereignis-Typ#",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x0154,
		"Datenverfaelschung  im  Anlauf  mit der  angegebenen Einrichtung. (T044) (Anlauf mit dem OGK: Prozessanstoss mit unzulaessigem Ereignis-Typ fuer laufenden Prozesszustand)",
		4,
		{
			"Phys. Einr.-Nr. (LODEAD)",
			"#Ereignis-Typ#",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x0155,
		"Datenverfaelschung im Anlauf mit der im BYTE 1 angegebenen Einrichtung. (T041) (Anlauf mit dem OGK: Unzulaessiger Prozesszustand bei Prozessanstoss mit beliebigem Ereignis-Typ)",
		5,
		{
			"Phys. Einr.-Nr. der FDS",
			"Phys. Einr.-Nr. (LODEAD)",
			"#Ereignis-Typ#",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"eigene Ident-Nr",
			"", "", "", "", "",
		},
	},

	{
		0x0156,
		"Datenverfaelschung  im  Anlauf  mit der  angegebenen Einrichtung. (T044) (Anlauf mit dem PFG: Prozessanstoss mit unzulaessigem Ereignis-Typ)",
		4,
		{
			"Phys. Einr.-Nr. aus Signalisierung",
			"#Ereignis-Typ#",
			"#Opcode#",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x0157,
		"Datenverfaelschung  im  Anlauf  mit der  angegebenen Einrichtung. (T044) (PFG-Anlauf-Auftrag (YAAP): falscher Einrichtungs-Typ)",
		3,
		{
			"Phys. Einr.-Nr. aus Signalisierung",
			"#Einrichtungs-Typ#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0158,
		"Datenverfaelschung  im  Anlauf  mit der  angegebenen Einrichtung. (T044) (Anlauf mit dem PFG: Prozessanstoss mit unzulaessigem Opcode (/= YAAP))",
		4,
		{
			"Phys. Einr.-Nr. aus Signalisierung",
			"#Ereignis-Typ#",
			"#Opcode#",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x0159,
		"Datenverfaelschung  im  Anlauf  mit der  angegebenen Einrichtung. (T044) (Anlauf mit dem PFG: Betriebs-Parameter-Quittung YPBQV kam von falscher Einrichtung)",
		4,
		{
			"Phys. Einr.-Nr. aus Signalisierung  (Sollwert)",
			"Phys. Einr.-Nr. (LODEAD)          (Istwert)",
			"#Einrichtungs-Typ#",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x015a,
		"Datenverfaelschung  im  Anlauf  mit der  angegebenen Einrichtung. (T044) (Anlauf mit dem PFG: Betriebs-Parameter-Quittung YPBQV kam von falscher Einrichtung)",
		4,
		{
			"Phys. Einr.-Nr. aus Signalisierung",
			"Phys. Einr.-Nr. (LODEAD)",
			"#Einrichtungs-Typ#",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x015b,
		"Datenverfaelschung  im  Anlauf  mit der  angegebenen Einrichtung. (T044) (Anlauf mit dem PFG: Prozessanstoss mit unzulaessigem Ereignis-Typ fuer laufenden Prozesszustand)",
		4,
		{
			"Phys. Einr.-Nr. (LODEAD)",
			"#Ereignis-Typ#",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x015c,
		"Datenverfaelschung  im  Anlauf  mit der  angegebenen Einrichtung. (T044) (Anlauf mit dem PFG: Prozessanstoss mit unzulaessigem Ereignis-Typ fuer laufenden Prozesszustand)",
		4,
		{
			"Phys. Einr.-Nr. (LODEAD)",
			"#Ereignis-Typ#",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x015d,
		"Datenverfaelschung im Anlauf mit der im BYTE 1 angegebenen Einrichtung. (T041) (Anlauf mit dem PFG: Unzulaessiger Prozesszustand bei Prozessanstoss mit beliebigem Ereignis)",
		5,
		{
			"Phys. Einr.-Nr. der FDS",
			"Phys. Einr.-Nr. (LODEAD)",
			"#Ereignis-Typ#",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"eigene Ident-Nr",
			"", "", "", "", "",
		},
	},

	{
		0x015e,
		"Datenverfaelschung im Anlauf mit der im BYTE 1 angegebenen Einrichtung. (T041) (unzulaessiger Globaler ST-Zustand im DKV fuer den PBR)",
		4,
		{
			"Phys. Einr.-Nr. der eigenen FDS",
			"Phys. Einr.-Nr.  (LODEAD)",
			"#Globaler ST-Zustand# des PBR",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x015f,
		"Datenverfaelschung im Anlauf mit der im BYTE 1 angegebenen Einrichtung. (T041) (unzulaessiger Detaillierter ST-Zustand im DKV fuer den PBR)",
		5,
		{
			"Phys. Einr.-Nr. der eigenen FDS",
			"Phys. Einr.-Nr.  (LODEAD)",
			"#Globaler ST-Zustand# des PBR",
			"#Detaillierter ST-Zustand# des PBR",
			"eigene Ident-Nr",
			"", "", "", "", "",
		},
	},

	{
		0x0160,
		"Datenverfaelschung im Anlauf mit der im BYTE 1 angegebenen Einrichtung. (T041) (unzulaessiger Globaler ST-Zustand im DKV fuer den PHE)",
		4,
		{
			"Phys. Einr.-Nr. der eigenen FDS",
			"Phys. Einr.-Nr. (LODEAD)",
			"#Globaler ST-Zustand# des PHE",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x0161,
		"Datenverfaelschung im Anlauf mit der im BYTE 1 angegebenen Einrichtung. (T041) (unzulaessiger Detaillierter ST-Zustand im DKV fuer den PHE)",
		5,
		{
			"Phys. Einr.-Nr. der eigenen FDS",
			"Phys. Einr.-Nr. (LODEAD)",
			"#Globaler ST-Zustand# des PHE",
			"#Detaillierter ST-Zustand# des PHE",
			"eigene Ident-Nr",
			"", "", "", "", "",
		},
	},

	{
		0x0162,
		"Datenverfaelschung im Anlauf mit der im BYTE 1 angegebenen Einrichtung. (T041) (unzulaessiger Globaler ST-Zustand im DKV fuer den FME)",
		4,
		{
			"Phys. Einr.-Nr. der eigenen FDS",
			"Phys. Einr.-Nr. (LODEAD)",
			"#Globaler ST-Zustand# des FME",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x0163,
		"Datenverfaelschung im Anlauf mit der im BYTE 1 angegebenen Einrichtung. (T041) (unzulaessiger Detaillierter ST-Zustand im DKV fuer den FME)",
		5,
		{
			"Phys. Einr.-Nr. der eigenen FDS",
			"Phys. Einr.-Nr. (LODEAD)",
			"#Globaler ST-Zustand# des FME",
			"#Detaillierter ST-Zustand# des FME",
			"eigene Ident-Nr",
			"", "", "", "", "",
		},
	},

	{
		0x0164,
		"Datenverfaelschung im Anlauf mit der im BYTE 1 angegebenen Einrichtung. (T041) (unzulaessiger Globaler ST-Zustand im DKV fuer den OGK)",
		4,
		{
			"Phys. Einr.-Nr. der eigenen FDS",
			"Phys. Einr.-Nr. (LODEAD)",
			"#Globaler ST-Zustand# des OGK",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x0165,
		"Datenverfaelschung im Anlauf mit der im BYTE 1 angegebenen Einrichtung. (T041) (unzulaessiger Detaillierter ST-Zustand im DKV fuer den OGK)",
		5,
		{
			"Phys. Einr.-Nr. der eigenen FDS",
			"Phys. Einr.-Nr. (LODEAD)",
			"#Globaler ST-Zustand# des OGK",
			"#Detaillierter ST-Zustand# des OGK",
			"eigene Ident-Nr",
			"", "", "", "", "",
		},
	},

	{
		0x0166,
		"Datenverfaelschung im Anlauf mit der im BYTE 1 angegebenen Einrichtung. (T041) (unzulaessiger Globaler ST-Zustand im DKV fuer das PFG)",
		4,
		{
			"Phys. Einr.-Nr. der eigenen FDS",
			"Phys. Einr.-Nr. (LODEAD)",
			"#Globaler ST-Zustand# des PFG",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x0167,
		"Datenverfaelschung im Anlauf mit der im BYTE 1 angegebenen Einrichtung. (T041) (unzulaessiger Detaillierter ST-Zustand im DKV fuer das PFG)",
		5,
		{
			"Phys. Einr.-Nr. der eigenen FDS",
			"Phys. Einr.-Nr. (LODEAD)",
			"#Globaler ST-Zustand# des PFG",
			"#Detaillierter ST-Zustand# des PFG",
			"eigene Ident-Nr",
			"", "", "", "", "",
		},
	},

	{
		0x0168,
		"Datenverfaelschung im Anlauf mit der im BYTE 1 angegebenen Einrichtung. (T041) (unzulaessiger Globaler ST-Zustand im DKV fuer den SPK)",
		4,
		{
			"Phys. Einr.-Nr. der eigenen FDS",
			"Phys. Einr.-Nr. (LODEAD)",
			"#Globaler ST-Zustand# des SPK",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x0169,
		"Datenverfaelschung im Anlauf mit der im BYTE 1 angegebenen Einrichtung. (T041) (unzulaessiger Detaillierter ST-Zustand im DKV fuer den SPK)",
		5,
		{
			"Phys. Einr.-Nr. der eigenen FDS",
			"Phys. Einr.-Nr. (LODEAD)",
			"#Globaler ST-Zustand# des SPK",
			"#Detaillierter ST-Zustand# des SPK",
			"eigene Ident-Nr",
			"", "", "", "", "",
		},
	},

	{
		0x016a,
		"Datenverfaelschung im Anlauf mit der im BYTE 1 angegebenen Einrichtung. (T041) (unzulaessiger Globaler ST-Zustand im DKV fuer die STBFDS)",
		4,
		{
			"Phys. Einr.-Nr. der eigenen FDS",
			"Phys. Einr.-Nr. (LODEAD)",
			"#Globaler ST-Zustand# der STBFDS",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x016b,
		"Datenverfaelschung im Anlauf mit der im BYTE 1 angegebenen Einrichtung. (T041) (unzulaessiger Detaillierter ST-Zustand im DKV fuer die STBFDS)",
		5,
		{
			"Phys. Einr.-Nr. der eigenen FDS",
			"Phys. Einr.-Nr. (LODEAD)",
			"#Globaler ST-Zustand# der STBFDS",
			"#Detaillierter ST-Zustand# der STBFDS",
			"eigene Ident-Nr",
			"", "", "", "", "",
		},
	},

	{
		0x016c,
		"Datenverfaelschung im Anlauf mit der im BYTE 1 angegebenen Einrichtung. (T041) (Globaler ST-Zustand UNA im DKV fuer den PHE)",
		4,
		{
			"Phys. Einr.-Nr. der eigenen FDS",
			"Phys. Einr.-Nr. (LODEAD)",
			"#Globaler ST-Zustand# des PHE",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x016d,
		"Datenverfaelschung im Anlauf mit der im BYTE 1 angegebenen Einrichtung. (T041) (Globaler ST-Zustand UNA im DKV fuer den FME)",
		4,
		{
			"Phys. Einr.-Nr. der eigenen FDS",
			"Phys. Einr.-Nr. (LODEAD)",
			"#Globaler ST-Zustand# des FME",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x016e,
		"Datenverfaelschung im Anlauf mit der im BYTE 1 angegebenen Einrichtung. (T041) (Globaler ST-Zustand UNA im DKV fuer den OGK)",
		4,
		{
			"Phys. Einr.-Nr. der eigenen FDS",
			"Phys. Einr.-Nr. (LODEAD)",
			"#Globaler ST-Zustand# des OGK",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x016f,
		"Datenverfaelschung im Anlauf mit der im BYTE 1 angegebenen Einrichtung. (T041) (Globaler ST-Zustand UNA im DKV fuer das PFG)",
		4,
		{
			"Phys. Einr.-Nr. der eigenen FDS",
			"Phys. Einr.-Nr. (LODEAD)",
			"#Globaler ST-Zustand# des PFG",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x0170,
		"Datenverfaelschung im Anlauf mit der im BYTE 1 angegebenen Einrichtung. (T041) (Globaler ST-Zustand UNA im DKV fuer den SPK)",
		4,
		{
			"Phys. Einr.-Nr. der eigenen FDS",
			"Phys. Einr.-Nr. (LODEAD)",
			"#Globaler ST-Zustand# des SPK",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x0171,
		"Datenverfaelschung  im  Anlauf  mit der  angegebenen Einrichtung. (T044) (unzulaessiger Einrichtungs-Typ bei Betriebsparameter-Ueberpruefung)",
		3,
		{
			"Phys. Einr.-Nr. (LODEAD)",
			"#Einrichtungs-Typ#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0172,
		"Die angegebene Einrichtung wurde waehrend ihres Anlaufs nach UNA konfiguriert. (T045) (Es existiert kein zugehoeriger KON-Prozess; evtl. kam UNA-Auftrag dazwischen)",
		4,
		{
			"Phys. Einr.-Nr. (LODEAD)",
			"Time-Out-Zaehler (fuer unquittierte Signalisierung)",
			"Wiederholzaehler (fuer BP- und TD-Uebertragung)",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x0173,
		"Datenverfaelschung  im  Anlauf  mit der  angegebenen Einrichtung. (T044) (unzulaessiger Return-Code vom OS-UP WTAKOM bei Pufferanforderung fuer die DKV-interne Signalisierung YKBAI)",
		3,
		{
			"Phys. Einr.-Nr. (LODEAD)",
			"#Return-Code von WTAKOM#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0174,
		"Die angegebene Einrichtung wurde waehrend ihres Anlaufs nach UNA konfiguriert. (T045) (es existiert kein zugehoeriger KON-Prozess; evtl. kam UNA-Auftrag dazwischen)",
		4,
		{
			"Phys. Einr.-Nr. (LODEAD)",
			"Time-Out-Zaehler",
			"Wiederholzaehler",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x0175,
		"Datenverfaelschung  im  Anlauf  mit der  angegebenen Einrichtung. (T044) (Return-Code 'Ident-Nr. existiert nicht' vom OS-UP WTAKOM bei Pufferanforderung fuer die DKV-interne Signalisierung YKBAI, obwohl diese Ident-Nr. noch im (SYKAUF) eingetragen ist)",
		3,
		{
			"Phys. Einr.-Nr. (LODEAD)",
			"#Return-Code von WTAKOM#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0178,
		"Die angegebene Einrichtung hat ohne Veranlassung der FDS eine Anlaufsignalisierung abgeschickt. (T042) (partieller Anlauf)",
		4,
		{
			"Phys. Einr.-Nr. der eigenen FDS",
			"Phys. Einr.-Nr. (LODEAD)",
			"Prozesstypidentifikation : #Entwickler-Info#",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x0179,
		"Datenverfaelschung im Anlauf mit der im BYTE 1 angegebenen Einrichtung. (T041) (unzulaessiger Return-Code vom OS-UP WTAKOM bei Pufferanforderung fuer die DKV-interne Signalisierung YKRAI (partieller Anlauf))",
		4,
		{
			"Phys. Einr.-Nr. der eigenen FDS",
			"Phys. Einr.-Nr. (LODEAD)",
			"#Return-Code von WTAKOM#",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x017a,
		"Datenverfaelschung  im  Anlauf  mit der  angegebenen Einrichtung. (T044) (Anlauf mit dem SPK: Prozessanstoss mit unzulaessigem Ereignis-Typ)",
		4,
		{
			"Phys. Einr.-Nr. aus Signalisierung",
			"#Ereignis-Typ#",
			"#Opcode#",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x017b,
		"Datenverfaelschung  im  Anlauf  mit der  angegebenen Einrichtung. (T044) (SPK-Anlauf-Auftrag (YAAS): falscher Einrichtungs-Typ)",
		3,
		{
			"Phys. Einr.-Nr. aus Signalisierung",
			"#Einrichtungs-Typ#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x017c,
		"Datenverfaelschung  im  Anlauf  mit der  angegebenen Einrichtung. (T044) (Anlauf mit dem SPK: Prozessaanstoss mit unzulaessigem Opcode (/=OYAAS))",
		4,
		{
			"Phys. Einr.-Nr. aus Signalisierung",
			"#Ereignis-Typ#",
			"#Opcode#",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x017d,
		"Datenverfaelschung  im  Anlauf  mit der  angegebenen Einrichtung. (T044) (Anlauf mit dem SPK: Betriebs-Parameter-Quittung YPBQV kam von falscher Einrichtung)",
		4,
		{
			"Phys. Einr.-Nr. aus Signalisierung  (Sollwert)",
			"Phys. Einr.-Nr. (LODEAD)          (Istwert)",
			"#Einrichtungs-Typ#",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x017e,
		"Datenverfaelschung  im  Anlauf  mit der  angegebenen Einrichtung. (T044) (anlauf mit bem SPK: Betriebs-Parameter-Quittung YPBQV kam von falscher Einrichtung)",
		4,
		{
			"Phys. Einr.-Nr. aus Signalisierung (Sollwert)",
			"Phys. Einr.-Nr. (LODEAD)         (Istwert)",
			"#Einrichtungs-Typ#",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x017f,
		"Datenverfaelschung  im  Anlauf  mit der  angegebenen Einrichtung. (T044) (Anlauf mit dem SPK: Prozessanstoss mit unzulaessigem Ereignis-Typ fuer laufenden Prozesszustand)",
		4,
		{
			"Phys. Einr.-Nr. (LODEAD)",
			"#Ereignis-Typ#",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x0180,
		"Datenverfaelschung  im  Anlauf  mit der  angegebenen Einrichtung. (T044) (Anlauf mit dem SPK: Prozessanstoss mit unzulaessigem Ereignis-Typ fuer laufenden Prozesszustand)",
		4,
		{
			"Phys. Einr.-Nr. (LODEAD)",
			"#Ereignis-Typ#",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x0181,
		"Datenverfaelschung  im  Anlauf  mit der  angegebenen Einrichtung. (T044) (Anlauf mit dem SPK: Tarif-Daten-Quittung YTDQS kam von falscher Einrichtung)",
		4,
		{
			"Phys. Einr.-Nr. aus Signalisierung  (Istwert)",
			"Phys. Einr.-Nr. (LODEAD)          (Sollwert)",
			"#Einrichtungs-Typ# (LODETY)",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x0182,
		"Datenverfaelschung  im  Anlauf  mit der  angegebenen Einrichtung. (T044) (Anlauf mit dem SPK: Tarif-Daten-Quittung YTDQS kam von falscher Einrichtung)",
		4,
		{
			"Phys. Einr.-Nr. aus Signalisierung (Istwert)",
			"Phys. Einr.-Nr. (LODEAD)         (Sollwert)",
			"#Einrichtungs-Typ# (LODETY)",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x0183,
		"Datenverfaelschung im Anlauf mit der im BYTE 1 angegebenen Einrichtung. (T041) (Anlauf mit dem SPK: Unzulaessiger Prozesszustand bei Prozessanstoss mit beliebigem Ereignis)",
		5,
		{
			"Phys. Einr.-Nr. der FDS",
			"Phys. Einr.-Nr. (LODEAD)",
			"#Ereignis-Typ#",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"eigene Ident-Nr",
			"", "", "", "", "",
		},
	},

	{
		0x0194,
		"Datenverfaelschung in der DKV. (T001) (Signalisierung von nicht erwarteter Einrichtung)",
		6,
		{
			"Phys. Einr.-Nr. der FDS",
			"Phys. Einr.-Nr. aus Signalisierung",
			"erwartete Phys. Einr.-Nr",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"Ident-Nr. aus Signalisierung",
			"eigene Ident-Nr",
			"", "", "", "",
		},
	},

	{
		0x0195,
		"Datenverfaelschung in der DKV. (T001) (unzulaessiger Return-Code vom OS-UP WTAKOM  bei Pufferanforderung fuer die DKV-interne  Signalisierung YUAAI an UHR)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code von WTAKOM#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0196,
		"Datenverfaelschung in der DKV. (T001) (Unzulaessiger Return-Code vom OS-UP WTAKOM bei Pufferanforderung fuer die DKV-interne Signalisierung YNZAI)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code von WTAKOM#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0197,
		"Datenverfaelschung in der DKV. (T001) (unzulaessiger Return-Code aus ANL-UP YPAKVT)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code KON#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0198,
		"Datenverfaelschung in der DKV. (T001) (Prozessanstoss mit unzulaessigem Ereignis-Typ)",
		4,
		{
			"Phys. Einr.-Nr. der FDS",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"#Ereignis-Typ#",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x0199,
		"Datenverfaelschung in der DKV. (T001) (Prozessanstoss mit unzulaessigem Ereignis-Typ)",
		6,
		{
			"Phys. Einr.-Nr. der FDS",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"#Ereignis-Typ#",
			"eigene Ident-Nr",
			"#Opcode#",
			"Ident-Nr. aus Signalisierung",
			"", "", "", "",
		},
	},

	{
		0x01db,
		"Datenfehler. (Start des Prozesses mit ungueltigem oder falschem #Opcode#.)",
		4,
		{
			"Phys. Einrichtungsnummer",
			"#Ereignis-Typ#",
			"#Ident-Nummer#",
			"#Opcode# der Signalisierung",
			"", "", "", "", "", "",
		},
	},

	{
		0x01dc,
		"Datenfehler. (Start des Prozesses mit ungueltigem oder falschem #Ereignis-Typ#.)",
		3,
		{
			"Phys. Einrichtungsnummer",
			"#Ereignis-Typ#",
			"#Ident-Nummer#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x01dd,
		"Datenfehler. (Start des Prozesses mit ungueltigem oder falschem #Ereignis-Typ#.)",
		3,
		{
			"Phys. Einrichtungsnummer",
			"#Ereignis-Typ#",
			"#Ident-Nummer#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x01de,
		"Datenfehler. (Start des Prozesses mit unzulaessigem oder falschen #Ereignis-Typ#.)",
		3,
		{
			"Phys. Einrichtungsnummer",
			"#Ereignis-Typ#",
			"#Ident-Nummer#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x01df,
		"Datenfehler. (Start des Prozesses durch eine Signalisierung mit unzulaessigem oder falschem #Opcode#.)",
		4,
		{
			"Phys. Einrichtungsnummer",
			"#Opcode# der Signalisierung",
			"#Ereignis-Typ#",
			"#Ident-Nummer#",
			"", "", "", "", "", "",
		},
	},

	{
		0x01e0,
		"Datenfehler. (Start des Prozesses mit unzulaessigem oder falschem #Ereignis-Typ#.)",
		3,
		{
			"Phys. Einrichtungsnummer",
			"#Ereignis-Typ#",
			"#Ident-Nummer#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x01e5,
		"Datenfehler. (Start des Prozesses mit ungueltigem oder falschem #Ereignis-Typ# oder mit falschem #Opcode#.)",
		5,
		{
			"Phys. Einrichtungsnummer",
			"#Ereignis-Typ#",
			"#Ident-Nummer#",
			"#Opcode# der Signalisierung (= 0 bei falschem Ereignis-Typ)",
			"#FDS-Status# (PORT 41H)",
			"", "", "", "", "",
		},
	},

	{
		0x01e8,
		"Datenfehler. (Start des Prozesses mit unzulaessigem oder falschem #Opcode#.)",
		5,
		{
			"Phys. Einrichtungsnummer",
			"#Ereignis-Typ#",
			"#Ident-Nummer#",
			"#Opcode# der Signalisierung (=0 bei falschem Ereignis-Typ)",
			"#FDS-Status# (PORT 41H)",
			"", "", "", "", "",
		},
	},

	{
		0x01ea,
		"Datenverfaelschung beim Anstoss eines Konfigurations- auftrags fuer einen OSK. (T099) (Parameterfehler: Log. Einr.-Nr. fuer OSK  ist unzulaessig, zulaessige Werte: 1 oder 2)",
		5,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Einrichtungs-Typ# des OSK",
			"#Log. Einr.-Nr.# des OSK aus KON-Auftrag",
			"eigene Ident-Nr",
			"Ident-Nr. des Auftraggebers",
			"", "", "", "", "",
		},
	},

	{
		0x01eb,
		"Datenverfaelschung beim Anstoss eines Konfigurations- auftrags. (T093) (Prozessanstoss mit unzulaessigem Ereignis-Typ)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Ereignis-Typ#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x01ec,
		"Datenverfaelschung beim Anstoss eines Konfigurations- auftrags. (T093) (Prozessanstoss mit falschem Ereignis-Typ oder nicht erwarteter Signalisierung)",
		5,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Ereignis-Typ#",
			"eigene Ident-Nr",
			"#Opcode# aus nicht erwarteter Signalisierung",
			"Ident-Nr. des Auftraggebers",
			"", "", "", "", "",
		},
	},

	{
		0x01ed,
		"Test- und Traceinformation beim Konfigurieren einer Einrichtung. (T101) (Bei der Quittung war der Auftraggeber-Prozess nicht  mehr vorhanden)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"eigene Ident-Nr",
			"Ident-Nr. des Auftraggebers",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x01f0,
		"Datenverfaelschung beim Konfigurieren eines SAE nach ACT. (T087) (Parameterfehler- falsche Log. Einr.-Nr.)",
		5,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Einrichtungs-Typ# des SAE",
			"unzulaessige #Log. Einr.-Nr.# des SAE",
			"eigene Ident-Nr",
			"Ident-Nr. des Auftraggebers",
			"", "", "", "", "",
		},
	},

	{
		0x01f1,
		"Datenverfaelschung beim Konfigurieren eines SAE nach ACT. (T087) (Prozessanstoss mit unzulaessigem Ereignis-Typ)",
		3,
		{
			"Phys. Einr.-Nr. (LODEAD)",
			"#Ereignis-Typ#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x01f3,
		"Datenverfaelschung beim Konfigurieren des PBR nach ACT. (T085) (Parameterfehler- unzulaessige Log. Einr.-Nr.)",
		5,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Einrichtungs-Typ# des PBR",
			"#Log. Einr.-Nr.# des PBR",
			"eigene Ident-Nr",
			"Ident-Nr. des Auftraggebers",
			"", "", "", "", "",
		},
	},

	{
		0x01f5,
		"Datenverfaelschung beim Konfigurieren des PBR nach ACT. (T085) (Unzulaessiger Return-Code aus KON-UP YPKCBZ)",
		8,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Globaler ST-Zustand# des PBR",
			"#Detaillierter ST-Zustand# des PBR",
			"#Opcode# des ACT-Auftrags",
			"#Einrichtungs-Typ# des PBR",
			"#Log. Einr.-Nr.# des PBR",
			"eigene Ident-Nr",
			"#Return-Code von YPKCBZ#",
			"", "",
		},
	},

	{
		0x01f6,
		"Datenverfaelschung beim Anstoss eines Konfigurations- auftrags fuer die FDS. (T089) (Parameterfehler: unzulaessige Log. Einr.-Nr. im ACT-Auftrag)",
		5,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Einrichtungs-Typ# aus ACT-Auftrag",
			"#Log. Einr.-Nr.# aus ACT-Auftrag",
			"eigene Ident-Nr",
			"Ident-Nr. des Auftraggebers",
			"", "", "", "", "",
		},
	},

	{
		0x01f8,
		"Datenverfaelschung beim Anstoss eines Konfigurations- auftrags fuer die FDS. (T089) (Prozessanstoss mit unzulaessigem Ereignis-Typ)",
		3,
		{
			"Phys. Einr.-Nr. (LODEAD)",
			"#Ereignis-Typ#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x01fa,
		"Datenverfaelschung beim Anstoss eines Konfigurations- auftrags fuer einen PHE. (T092) (Parameterfehler: Log. Einr.-Nr. aus ACT-Auftrag nicht zulaessig)",
		5,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Einrichtungs-Typ# aus ACT-Auftrag",
			"#Log. Einr.-Nr.# aus ACT-Auftrag",
			"eigene Ident-Nr",
			"Ident-Nr. des Auftraggebers",
			"", "", "", "", "",
		},
	},

	{
		0x01fc,
		"Datenverfaelschung beim Anstoss eines Konfigurations- auftrags fuer einen PHE. (T092) (Prozessanstoss mit unzulaessigem Ereignis-Typ)",
		3,
		{
			"Phys. Einr.-Nr. (LODEAD)",
			"#Ereignis-Typ#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x01fe,
		"Datenverfaelschung beim Anstoss eines Konfigurations- auftrags fuer die BS. (T090) (Parameterfehler: Log. Einr.-Nr. im ACT-Auftrag unzulaessig)",
		5,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Einrichtungs-Typ# aus ACT-Auftrag",
			"#Log. Einr.-Nr.# aus ACT-Auftrag",
			"eigene Ident-Nr",
			"Ident-Nr. des Auftraggebers",
			"", "", "", "", "",
		},
	},

	{
		0x01ff,
		"Datenverfaelschung beim Anstoss eines Konfigurations- auftrags fuer die BS. (T090) (Prozessanstoss mit unzulaessigem Ereignis-Typ)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Ereignis-Typ#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0201,
		"Datenverfaelschung beim Konfigurieren eines FME nach ACT. (T086) (Parameterfehler: Log. Einr.-Nr. unzulaessig)",
		5,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Einrichtungs-Typ# aus ACT-Auftrag",
			"#Log. Einr.-Nr.# aus ACT-Auftrag",
			"eigene Ident-Nr",
			"Ident-Nr. des Auftraggebers",
			"", "", "", "", "",
		},
	},

	{
		0x0203,
		"Datenverfaelschung beim Konfigurieren eines FME nach ACT. (T086) (Prozessanstoss mit unzulaessigem Ereignis-Typ)",
		3,
		{
			"Phys. Einr.-Nr. (LODEAD)",
			"#Ereignis-Typ#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0205,
		"Datenverfaelschung beim Anstoss eines Konfigurations- auftrags fuer einen als OGK arbeitenden OSK. (T097) (Parameterfehler: Log. Einr.-Nr. im ACT-Auftrag unzulaessig)",
		5,
		{
			"Phys. Einr.-Nr. (LODEAD)",
			"#Einrichtungs-Typ# aus ACT-Auftrag",
			"unzulaessige #Log. Einr.-Nr.# aus ACT-Auftrag",
			"eigene Ident-Nr",
			"Ident-Nr. des Auftraggebers",
			"", "", "", "", "",
		},
	},

	{
		0x0207,
		"Datenverfaelschung beim Anstoss eines Konfigurations- auftrags fuer einen als OGK arbeitenden OSK. (T097) (Falscher Ereignis-Typ-  der ACT-Funktionsmodul des OGK wurde nicht  mit Ereignis-Typ MREADY angestossen)",
		3,
		{
			"Phys. Einr.-Nr. (LODEAD)",
			"#Ereignis-Typ#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x020b,
		"Datenverfaelschung beim Anstoss eines Konfigurations- auftrags fuer einen als OGK arbeitenden OSK. (T097) (Unzulaessiger Return-Code aus Unterprogramm YPKCBZ bei der  Behandlung eines ACT-Auftrags)",
		8,
		{
			"Phys. Einr.-Nr. (LODEAD)",
			"#Globaler ST-Zustand# des zu konfigurierenden OGK",
			"#Detaillierter ST-Zustand# des zu konfigurierenden OGK",
			"#Opcode# des ACT-Auftrags",
			"#Einrichtungs-Typ# des zu konfigurierenden OGK",
			"#Log. Einr.-Nr.# des zu konfigurierenden OGK",
			"eigene Ident-Nr",
			"#Return-Code von YPKCBZ#",
			"", "",
		},
	},

	{
		0x020c,
		"Datenverfaelschung beim Anstoss eines Konfigurations- auftrags fuer das PFG. (T091) (Parameterfehler: Log. Einr.-Nr. unzulaessig)",
		5,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Einrichtungs-Typ# des zu konfigurierenden PFG",
			"unzulaessige #Log. Einr.-Nr.# des zu konfigurierenden PFG",
			"eigene Ident-Nr",
			"Ident-Nr. des Auftraggebers",
			"", "", "", "", "",
		},
	},

	{
		0x020e,
		"Datenverfaelschung beim Anstoss eines Konfigurations- auftrags fuer das PFG. (T091) (Prozessanstoss mit unzulaessigem Ereignis-Typ)",
		3,
		{
			"Phys. Einr.-Nr. (LODEAD)",
			"#Ereignis-Typ#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0210,
		"Datenverfaelschung beim Anstoss eines Konfigurations- auftrags fuer einen SPK. (T096) (Parameterfehler: Log. Einr.-Nr. fuer SPK im ACT-Auftrag nicht  zulaessig)",
		5,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Einrichtungs-Typ# des zu konfigurierenden SPK",
			"unzulaessige #Log. Einr.-Nr.# des zu konfigurierenden SPK",
			"eigene Ident-Nr",
			"Ident-Nr. des Auftraggebers",
			"", "", "", "", "",
		},
	},

	{
		0x0212,
		"Datenverfaelschung beim Anstoss eines Konfigurations- auftrags fuer einen SPK. (T096) (Prozessanstoss mit unzulaessigem Ereignis-Typ)",
		3,
		{
			"Phys. Einr.-Nr. (LODEAD)",
			"#Ereignis-Typ#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0215,
		"Datenverfaelschung beim Anstoss eines Konfigurations- auftrags fuer einen SPK. (T096) (Der Zaehler fuer SPK's im ST-Zustand MBL/PLA befindet  sich in einem undefinierten Zustand, d. h. der Wert  ist groesser als die Maximalzahl SPK)",
		7,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Globaler ST-Zustand# des zu konfigurierenden SPK",
			"#Detaillierter ST-Zustand# des zu konfigurierenden SPK",
			"#Opcode# des ACT-Auftrags",
			"#Einrichtungs-Typ# des zu konfigurierenden SPK",
			"#Log. Einr.-Nr.# des zu konfigurierenden SPK",
			"eigene Ident-Nr",
			"", "", "",
		},
	},

	{
		0x0217,
		"Datenverfaelschung beim Anlauf mit dem MSC. (T088) (Prozessanstoss mit unzulaessigem Ereignis-Typ)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Ereignis-Typ#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0219,
		"Datenverfaelschung beim Konfigurieren eines SAE nach ACT. (T087) (Prozessanstoss mit unzulaessigem Ereignis-Typ)",
		3,
		{
			"Phys. Einr.-Nr. (LODEAD)",
			"#Ereignis-Typ#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x021a,
		"Datenverfaelschung in der DKV. (T001) (Prozesszustand (LODSTA) unzulaessig)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"Phys. Einr.-Nr. (LODEAD)",
			"Prozesszustand (LODSTA)  #Entwickler-Info#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x021b,
		"Datenverfaelschung in der DKV. (T001) (Prozessanstoss mit unzulaessigem Ereignis-Typ)",
		3,
		{
			"Phys. Einr.-Nr. (LODEAD)",
			"#Ereignis-Typ#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x021c,
		"Datenverfaelschung beim Konfigurieren einer Einrichtung nach ACT. (T095) (Signalisierung YNFAI an den festen Prozess MUK konnte nicht  abgesetzt werden, da er nicht mehr existiert )",
		1,
		{
			"Phys. Einr.-Nr. der FDS",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x021d,
		"Test- und Traceinformation- Beim Konfigurieren einer Einrichtung hat sich der Prozess, der die Konfiguration veranlasst hat, vor Beendigung der Konfiguration beendet. (T100) (ACT-Quittung an den Auftraggeber konnte nicht abgesetzt  werden, da er nicht mehr existiert)",
		1,
		{
			"Phys. Einr.-Nr. der FDS",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x021e,
		"Test- und Traceinformation beim Konfigurieren einer Einrichtung. (T101) (Signalisierung an einen Prozess konnte nicht abgesetzt  werden, da er nicht mehr existiert)",
		2,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Opcode# der Signalisierung",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x021f,
		"Datenverfaelschung bei der DKV-internen Prozesskommunikation (T066) (unzulaessiger Return-Code vom OS-UP WTAKOM bei Puffer-  anforderung fuer DKV-interne Signalisierung)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code von WTAKOM#",
			"#Opcode#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0220,
		"Datenverfaelschung beim Konfigurieren einer Einrichtung nach ACT. (T095) (Einrichtungsdaten im LOD-Bereich stimmen mit  den in der Signalisierung nicht ueberein)",
		6,
		{
			"Phys. Einr.-Nr. der FDS",
			"Phys. Einr.-Nr. (LODEAD)",
			"Ident-Nr. des Absenders",
			"#Opcode# aus Signalisierung",
			"#Einrichtungs-Typ#",
			"#Log. Einr.-Nr.#",
			"", "", "", "",
		},
	},

	{
		0x0221,
		"Datenverfaelschung beim Konfigurieren einer Einrichtung nach ACT. (T095) (Prozessanstoss mit unzulaessigem Ereignis-Typ)",
		3,
		{
			"Phys. Einr.-Nr. (LODEAD)",
			"#Ereignis-Typ#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0222,
		"Datenverfaelschung beim Konfigurieren der angegebenen Einrichtung. (T102) (Prozessanstoss mit unzulaessigem Ereignis-Typ)",
		3,
		{
			"Phys. Einr.-Nr. (LODEAD)",
			"#Ereignis-Typ#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0223,
		"Datenverfaelschung beim Konfigurieren des angegebenen PHE. (T103) (Prozesszustand (LODSTA) : unzulaessig)",
		2,
		{
			"Phys. Einr.-Nr. (LODEAD)",
			"Prozesszustand (LODSTA) #Entwickler-Info#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x0224,
		"Datenverfaelschung beim Konfigurieren des angegebenen PHE. (T103) (Prozessanstoss mit unzulaessigem Ereignis-Typ)",
		3,
		{
			"Phys. Einr.-Nr. (LODEAD)",
			"#Ereignis-Typ#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0225,
		"Datenverfaelschung im BS-Anlauf. (T055) (Prozessanstoss mit unzulaessigem Ereignis-Typ)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Ereignis-Typ#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0226,
		"Datenverfaelschung beim Konfigurieren eines FME nach ACT. (T086) (Prozesszustand (LODSTA) unzulaessig)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"Phys. Einr.-Nr. (LODEAD)",
			"Prozesszustand (LODSTA) #Entwickler-Info#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0227,
		"Datenverfaelschung beim Konfigurieren eines FME nach ACT. (T086) (Prozessanstoss mit unzulaessigem Ereignis-Typ)",
		3,
		{
			"Phys. Einr.-Nr. (LODEAD)",
			"#Ereignis-Typ#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0228,
		"Datenverfaelschung beim Konfigurieren eines OGK nach ACT. (T104) (Prozesszustand (LODSTA) unzulaessig)",
		2,
		{
			"Phys. Einr.-Nr. (LODEAD)",
			"Prozesszustand (LODSTA) #Entwickler-Info#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x0229,
		"Datenverfaelschung beim Konfigurieren eines OGK nach ACT. (T104) (Prozessanstoss mit unzulaessigem Ereignis-Typ)",
		3,
		{
			"Phys. Einr.-Nr. (LODEAD)",
			"#Ereignis-Typ#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x022a,
		"Datenverfaelschung beim Konfigurieren des PFG nach ACT. (T105) (Prozesszustand (LODSTA) unzulaessig)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"Phys. Einr.-Nr. (LODEAD)",
			"Prozesszustand (LODSTA) #Entwickler-Info#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x022b,
		"Datenverfaelschung beim Konfigurieren des PFG nach ACT. (T105) (Prozessanstoss mit unzulaessigem Ereignis-Typ)",
		3,
		{
			"Phys. Einr.-Nr. (LODEAD)",
			"#Ereignis-Typ#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x022c,
		"Datenverfaelschung beim Konfigurieren eines SPK nach ACT. (T106) (Prozesszustand (LODSTA) unzulaessig)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"Phys. Einr.-Nr. (LODEAD)",
			"Prozesszustand (LODSTA) #Entwickler-Info#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x022d,
		"Datenverfaelschung beim Konfigurieren eines SPK nach ACT. (T106) (Prozessanstoss mit unzulaessigem Ereignis-Typ)",
		3,
		{
			"Phys. Einr.-Nr. (LODEAD)",
			"#Ereignis-Typ#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x022e,
		"Datenverfaelschung beim Anlauf mit dem MSC. (T088) (Prozessanstoss mit unzulaessigem Prozesszustand)",
		2,
		{
			"Phys. Einr.-Nr. der FDS",
			"Prozesszustand (LODSTA) #Entwickler-Info#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x022f,
		"Datenverfaelschung. (T003) (Unzulaessiger Konfigurations-Auftrag nach MBL: falsche  Einrichtung)",
		4,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Einrichtungs-Typ# aus MBL-Auftrag",
			"#Log. Einr.-Nr.# aus MBL-Auftrag",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x0231,
		"Datenverfaelschung in der DKV. (T001) (Prozessanstoss mit unzulaessigem Ereignis-Typ)",
		4,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Ereignis-Typ#",
			"eigene Ident-Nr",
			"#Opcode# aus der Signalisierung",
			"", "", "", "", "", "",
		},
	},

	{
		0x0232,
		"Datenverfaelschung in der DKV. (T001) ( Zaehler FYKAUN verfaelscht,   kein SPK mehr in UNA )",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"Zaehler fuer Anzahl SPK's in UNA (FYKAUN) (Istwert)",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0233,
		"Datenverfaelschung in der DKV. (T001) ( Zaehler FYKAPM verfaelscht oder   alle SPK's in PLA/MBL )",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"Zaehler SPK in PLA oder MBL (FYKAPM)",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0234,
		"Datenverfaelschung in der DKV. (T001) ( nicht erwartete globale ST-Zustaende,   ST-Zustand (Einrichtung) /= MBL/UNA   bei MBL-Auftrag )",
		6,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Einrichtungs-Typ#",
			"#Log. Einr.-Nr.#",
			"#Globaler ST-Zustand# der Einrichtung",
			"#Auftrags-Typ#",
			"eigene Ident-Nr",
			"", "", "", "",
		},
	},

	{
		0x0240,
		"Datenverfaelschung in der DKV. (T001) (unzulaessiger Return-Code vom OS-UP WTAKOM bei Puffer-  anforderung fuer DKV-interne Signalisierung ZAAI)",
		5,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code von WTAKOM#",
			"Ident-Nr. des Ziels",
			"#Opcode#",
			"eigene Ident-Nr",
			"", "", "", "", "",
		},
	},

	{
		0x0241,
		"Datenverfaelschung in der DKV. (T001) (Unzulaessiger Return-Code vom OS-UP WTAKOM bei Puffer-  anforderung fuer DKV-interne Signalisierung ZAAI)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code von WTAKOM#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0245,
		"Datenverfaelschung  im  Anlauf  mit der  angegebenen Einrichtung. (T044) (Aufruf der Prozedur YPKOSK fuer Einrichtungs-Typ  ungleich OSK (Parameterfehler) )",
		4,
		{
			"Phys. Einr.-Nr. der anlaufenden Einrichtung",
			"#Einrichtungs-Typ#",
			"#Log. Einr.-Nr.#",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x0249,
		"Datenverfaelschung. (T003) ( Signalisierung von nicht erwarteter Einrichtung )",
		4,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Einrichtungs-Typ#",
			"#Log. Einr.-Nr.#",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x024a,
		"Datenverfaelschung in der DKV. (T001) ( UNA-Auftrag fuer ACTFDS )",
		4,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Ereignis-Typ#",
			"#Opcode# des UNA-Auftrags",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x024b,
		"Ein UNA-Auftrag fuer den Einrichtungstyp 'BS' wurde gestellt. (T142) ( Unzulaessige Konfiguration )",
		2,
		{
			"Phys. Einr.-Nr. der FDS",
			"Ident-Nr. des Auftraggebers",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x024d,
		"Datenverfaelschung in der DKV. (T001) (Prozessanstoss mit unzulaessiger Signalisierung )",
		4,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Ereignis-Typ#",
			"eigene Ident-Nr",
			"#Opcode# aus der Signalisierung",
			"", "", "", "", "", "",
		},
	},

	{
		0x024f,
		"Kein Vermittlungsbetrieb mehr moeglich; kein SPK mehr verfuegbar;   die  angegebene  Einrichtung  wurde abgeschaltet. (T033) ( UNA-Auftrag fuer letzten aktiven SPK fuehrt   zum VTB-Verlust )",
		2,
		{
			"Phys. Einr.-Nr. der FDS",
			"Phys. Einr.-Nr des letzten SPK",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x0253,
		"Datenverfaelschung in der DKV. (T001) ( unzulaessiger alter Konfigurations-Auftrag besteht )",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Opcode# (alter Auftrag /= ACT )",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0255,
		"Verlust der BS-VTB bzw. Verlust der Bakenfunktion (je nach BS-Typ), da kein OSK mehr verfuegbar; die angegebene Einrichtung wurde abgeschaltet. (T153) ( UNA-Auftrag fuer letzten aktiven OGK fuehrt zum   VTB-Verlust bzw. Verlust der Bakenfunktion (je nach   BS-Typ), da korrespondierender OSK nicht verfuegbar )",
		4,
		{
			"Phys. Einr.-Nr. des OSK(SPK)",
			"#Globaler ST-Zustand# des OSK(SPK)",
			"#Detaillierter ST-Zustand# des OSK(SPK)",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x0257,
		"Datenverfaelschung in der DKV. (T001) ( Bei UNA-Auftrag fuer OSK(OGK) , unzulaessiger ST-Zustand   des korrespondierenden OSK(SPK) )",
		4,
		{
			"Phys. Einr.-Nr. des OSK(SPK)",
			"#Globaler ST-Zustand# des OSK(SPK)",
			"#Detaillierter ST-Zustand# des OSK(SPK)",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x0258,
		"Datenverfaelschung in der DKV. (T001) ( Zaehler FYKAUN verfaelscht,   alle SPK's in UNA )",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"Zaehler Anzahl SPK in UNA (FYKAUN)",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x025c,
		"SW-Fehler in der DKV. (T002) (Bereinigungsprozess der VT noch aktiv)",
		1,
		{
			"Phys. Einr.-Nr. der FDS",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x025e,
		"Datenverfaelschung in der DKV. (T001) (Prozessanstoss mit unzulaessigem Ereignis)",
		6,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Ereignis-Typ#",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"eigene Ident-Nr",
			"#Opcode#",
			"Ident-Nr. aus Signalisierung",
			"", "", "", "",
		},
	},

	{
		0x025f,
		"Datenverfaelschung in der DKV. (T001) (unzulaessiger Returncode aus KON-UP YPKPVT)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code KON#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0260,
		"BS-VTB erreicht. (T136) (Prozess \"Melden der VTB an MSC\" erkennt das Erreichen der BS-VTB)",
		2,
		{
			"Phys. Einr.-Nr. der FDS",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x0263,
		"Datenverfaelschung bei der Datenuebertragung von der FUPEF. (T025) (unzulaessiger Globaler ST-Zustand des PHE in der Signalisierung YSTAE (Status-Meldung))",
		2,
		{
			"Phys. Einr.-Nr. des PHE",
			"#Globaler ST-Zustand# aus Signalisierung",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x0266,
		"Datenverfaelschung in der DKV. (T001) (Prozessanstoss mit unzulaessigem Ereignis)",
		4,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Ereignis-Typ#",
			"eigene Ident-Nr",
			"#Opcode#",
			"", "", "", "", "", "",
		},
	},

	{
		0x0268,
		"Datenverfaelschung in der DKV. (T001) (Unzulaessiger Return-Code vom OS-UP WTAKOM bei Pufferanforderung fuer die DKV-interne Signalisierung YUBAI)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code von WTAKOM#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0269,
		"Datenverfaelschung in der DKV. (T001) (Prozessanstoss mit unzulaessigem Ereignis)",
		6,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Ereignis-Typ#",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"eigene Ident-Nr",
			"#Opcode#",
			"Ident-Nr. aus Signalisierung",
			"", "", "", "",
		},
	},

	{
		0x026b,
		"Datenverfaelschung in der DKV. (T001) (Unzulaessiger Return-Code vom OS-UP WTAKOM bei Pufferanforderung fuer die DKV-interne Signalisierung ZAAI)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code von WTAKOM#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x026d,
		"Datenverfaelschung in der DKV. (T001) (Prozessanstoss mit unzulaessigem Ereignis)",
		5,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Ereignis-Typ#",
			"eigene Ident-Nr",
			"#Opcode#",
			"Ident-Nr. aus Signalisierung",
			"", "", "", "", "",
		},
	},

	{
		0x0271,
		"Datenverfaelschung in der DKV. (T001) (Unzulaessiger Return-Code vom OS-UP WTAKOM bei Puffer-  anforderung fuer DKV-interne Signalisierung YVAAI)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code von WTAKOM#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0273,
		"Datenverfaelschung in der DKV. (T001) (Prozessanstoss mit unzulaessigem Ereignis)",
		5,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Ereignis-Typ#",
			"eigene Ident-Nr",
			"#Opcode#",
			"Ident-Nr. aus Signalisierung",
			"", "", "", "", "",
		},
	},

	{
		0x0275,
		"Datenverfaelschung in der DKV. (T001) (unzulaessiger Return-Code aus KON-UP YPKPVT)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code KON#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0278,
		"Datenverfaelschung in der DKV. (T001) (Prozessanstoss mit unzulaessigem Ereignis)",
		5,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Ereignis-Typ#",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"eigene Ident-Nr",
			"#Opcode#",
			"", "", "", "", "",
		},
	},

	{
		0x0284,
		"SW-Fehler in der DKV. (T002) (Dritter Timeout bei Bearbeitung der Signalisierungen SSSAU bzw. SNSAU, KON quittiert nicht)",
		2,
		{
			"Phys. Einr.-Nr. der FDS",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x0289,
		"Eintreffen einer nicht erwarteten Signalisierung im momentanen Prozesszustand (T119) (Prozessanstoss mit unzulaessigem Ereignis)",
		4,
		{
			"Phys. Einr.-Nr. der FDS",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"#Ereignis-Typ#",
			"#Opcode#",
			"", "", "", "", "", "",
		},
	},

	{
		0x028a,
		"LTG im MSC antwortet  nicht  bei Austausch der Sprech- kreissperren. (T011) (Dritter Timeout im Prozess zur Bearbeitung der Signalisierung YNFAI)",
		2,
		{
			"Phys. Einr.-Nr. der FDS",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x028b,
		"LTG im MSC antwortet  nicht  bei Austausch der Sprech- kreissperren. (T011) (Dritter Timeout im Prozess zur Bearbeitung der Signalisierung YNSAI)",
		2,
		{
			"Phys. Einr.-Nr. der FDS",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x028e,
		"MSC antwortet  nicht  bei Austausch der Sprechkreissperren. (T008) (Dritter Timeout im Prozess zur Bearbeitung der Signalisierung YNZAI)",
		2,
		{
			"Phys. Einr.-Nr. der FDS",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x0290,
		"MSC antwortet  nicht  waehrend des Anlaufs. (T007) (Dritter Timeout bei Bearbeitung der Signalisierung YSTAK)",
		2,
		{
			"Phys. Einr.-Nr. der FDS",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x0291,
		"Datenverfaelschung in der DKV. (T001) (unzulaessiger Return-Code des MUK-UP YPNAA2)",
		2,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code MUK#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x0292,
		"Datenverfaelschung in der DKV. (T001) (Unzulaessiger Return-Code des MUK-UP YPNOK2)",
		2,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code MUK#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x0293,
		"Datenverfaelschung in der DKV. (T001) (unzulaessiger Return-Code des MUK-UP YPNUN2)",
		2,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code MUK#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x0294,
		"Datenverfaelschung in der DKV. (T001) (unzulaessiger Return-Code des MUK-UP YPNUZ2)",
		2,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code MUK#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x0295,
		"Datenverfaelschung in der DKV. (T001) (Unzulaessiger Return-Code des MUK-UP YPNIF2)",
		2,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code MUK#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x0296,
		"Datenverfaelschung in der DKV. (T001) (Unzulaessiger Return-Code des MUK-UP YPNIS2)",
		2,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code MUK#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x029a,
		"Datenverfaelschung in der DKV. (T001) (Unzulaessiger Return-Code des MUK-UP YPNIZ2)",
		2,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code MUK#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x029b,
		"Datenverfaelschung in der DKV. (T001) (Unzulaessiger Return-Code des MUK-UP YPNUN1)",
		2,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code MUK#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x029c,
		"Datenverfaelschung in der DKV. (T001) (Unzulaessiger Return-Codedes MUK-UP YPNUN2)",
		2,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code MUK#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x029d,
		"Datenverfaelschung in der DKV. (T001) (unzulaessiger Return-Code des MUK-UP YPNUZ1)",
		2,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code MUK#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x029e,
		"Datenverfaelschung in der DKV. (T001) (unzulaessiger Return-Code von MUK-UP YPNUZ2)",
		2,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code MUK#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x029f,
		"Datenverfaelschung in der DKV. (T001) (unzulaessiger Return-Code des MUK-UP YPNUA3)",
		2,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code MUK#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x02a0,
		"Datenverfaelschung in der DKV. (T001)  (Unzulaessige Return-Code vom OS-UP WTAKOM bei Puffer-   anforderung fuer DKV-interne Signalisierung YKRAI)",
		2,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code von WTAKOM#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x02a1,
		"Datenverfaelschung in der DKV. (T001) (Unzulaessiger Return-Code vom OS-UP WTAKOM bei Puffer-  anforderung fuer eine DKV-interne Signalisierung)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code von WTAKOM#",
			"#Opcode# der Quittung",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x02a2,
		"Datenverfaelschung in der DKV. (T001)  (Unzulaessiger Return-Code vom OS-UP WTAKOM bei  Pufferanforderung fuer DKV-interne Signalsierung YKKAI)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code von WTAKOM#",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x02a3,
		"Datenverfaelschung in der DKV. (T001) (Unzulaessiger Prozesszustand)",
		2,
		{
			"Phys. Einr.-Nr. der FDS",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x02a4,
		"Datenverfaelschung in der DKV. (T001) (Unzulaessiger Return-Code vom MUK-UP YPNS02)",
		2,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code MUK#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x02a5,
		"Datenverfaelschung bei der Datenuebertragung vom MSC. (T005) (MSC-Signalisierung SFQU fuer falschen SK)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"erwartete #SK-Nr.#    (Sollwert)",
			"empfangene #SK-Nr.#   (Istwert)",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x02a6,
		"Datenverfaelschung bei der Datenuebertragung vom MSC. (T005) (MSC-Signalisierung SSQU fuer falschen SK)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"erwartete #SK-Nr.#    (Sollwert)",
			"empfangene #SK-Nr.#   (Istwert)",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x02a7,
		"Datenverfaelschung bei der Datenuebertragung vom MSC. (T005) (Falscher Bereich fuer MSC-Signalisierung SNSAU)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"empfangene 1 #SK-Nr.#",
			"empfangene letzte #SK-Nr.#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x02a8,
		"Datenverfaelschung bei der Datenuebertragung vom MSC. (T005) (Falscher SK-Nummern-Bereich fuer MSC-Signalsierung SSSAU)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"empfangene 1. #SK-Nr.#",
			"empfangene letzte #SK-Nr.#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x02a9,
		"Datenverfaelschung bei der Datenuebertragung vom MSC. (T005) (Falscher SK-Nummern-Bereich in MSC-Signalisierung SSSQU)",
		5,
		{
			"Phys. Einr.-Nr. der FDS",
			"erwartete 1. #SK-Nr.#         (Sollwert)",
			"erwartete letzte #SK-Nr.#     (Sollwert)",
			"empfangene 1. #SK-Nr.#        (Istwert)",
			"empfangene letzte #SK-Nr.#    (Istwert)",
			"", "", "", "", "",
		},
	},

	{
		0x02aa,
		"Datenverfaelschung bei der Datenuebertragung vom MSC. (T005) (Falscher SK-Nummern-Bereich in MSC-Signalisierung  SSSQU)",
		5,
		{
			"Phys. Einr.-Nr. der FDS",
			"erwartete 1. #SK-Nr.#         (Sollwert)",
			"erwartete letzte #SK-Nr.#     (Sollwert)",
			"empfangene 1. #SK-Nr.#        (Istwert)",
			"empfangene letzte #SK-Nr.#    (Istwert)",
			"", "", "", "", "",
		},
	},

	{
		0x02ab,
		"Datenverfaelschung bei der Datenuebertragung vom MSC. (T005) (SK-Nummer ausserhalb des zulaessigen Bereichs)",
		2,
		{
			"Phys. Einr.-Nr. der FDS",
			"empfangene 1. #SK-Nr.#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x02ac,
		"Datenverfaelschung bei der Datenuebertragung vom MSC. (T005) (SK-Nummer ausserhalb des zulaessigen Bereichs)",
		2,
		{
			"Phys. Einr.-Nr. der FDS",
			"empfangene 1. #SK-Nr.#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x02ae,
		"Datenverfaelschung bei der Datenuebertragung vom MSC. (T005) (Falscher SK-Nummern-Bereich in der Signalisierung SSSQU)",
		5,
		{
			"Phys. Einr.-Nr. der FDS",
			"erwartete 1. #SK-Nr.#         (Sollwert)",
			"erwartete letzte #SK-Nr.#     (Sollwert)",
			"empfangene 1. #SK-Nr.#        (Istwert)",
			"empfangene letzte #SK-Nr.#    (Istwert)",
			"", "", "", "", "",
		},
	},

	{
		0x02af,
		"Datenverfaelschung bei der Datenuebertragung vom CCNC. (T006) (SK-Nummer ausserhalb des zulaessigen Bereichs)",
		2,
		{
			"Phys. Einr.-Nr. der FDS",
			"empfangene  #SK-Nr.#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x02b0,
		"Datenverfaelschung bei der Datenuebertragung vom MSC. (T005) (SK-Nummer ausserhalb des zulaessigen Bereichs)",
		2,
		{
			"Phys. Einr.-Nr. der FDS",
			"empfangene #SK-Nr.#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x02b1,
		"Datenverfaelschung bei der Datenuebertragung vom MSC. (T005) (Falscher SK-Nummern-Bereich in der MSC-Signalisierung SSSQU)",
		5,
		{
			"Phys. Einr.-Nr. der FDS",
			"erwartete 1. #SK-Nr.#         (Sollwert)",
			"erwartete letzte #SK-Nr.#     (Sollwert)",
			"empfangene 1. #SK-Nr.#        (Istwert)",
			"empfangene letzte #SK-Nr.#    (Istwert)",
			"", "", "", "", "",
		},
	},

	{
		0x02c3,
		"Datenverfaelschung in der DKV. (T001) (Prozessanstoss mit unzulaessigem Ereignis-Typ)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Ereignis-Typ#",
			"#Opcode#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x02c4,
		"Datenverfaelschung. (T003) (Unerwarteter Einrichtungs-Typ in der  DKV-internen Signalisierung YUBAI)",
		2,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Einrichtungs-Typ#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x02c5,
		"Datenverfaelschung. (T003) (Signalisierung von unerwarteter Einrichtung)",
		2,
		{
			"Phys. Einr.-Nr. aus Signalisierung (Istwert)",
			"Phys. Einr.-Nr. aus tasklokalen Daten (Sollwert)",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x02c6,
		"Datenverfaelschung. (T003) (Prozessanstoss mit unzulaessigem Opcode)",
		2,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Opcode#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x02c7,
		"Datenverfaelschung in der DKV. (T001) (Prozessanstoss mit unerwartetem Ereignis-Typ)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Ereignis-Typ#",
			"#Opcode#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x02c8,
		"Datenverfaelschung in der DKV. (T001) (Prozessanstoss mit unzulaessigem Ereignis-Typ)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Ereignis-Typ#",
			"#Opcode#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x02c9,
		"Datenverfaelschung in der DKV. (T001) (Prozessanstoss mit unzulaessigem Ereignis-Typ)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Ereignis-Typ#",
			"#Opcode#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x02cd,
		"Datenverfaelschung bei der Datenuebertragung vom MSC. (T005) (Verfaelschung der Uhrzeitdaten vor Einstellen des  HW-Timers (unzulaessiger Wertebereich))",
		8,
		{
			"Phys. Einr.-Nr. der FDS",
			"Monatstag (BCD-Code)",
			"Wochentag (BCD-Code)",
			"Monat     (BCD-Code)",
			"Stunde    (BCD-Code)",
			"Minute    (BCD-Code)",
			"Sekunde   (BCD-Code)",
			"Jahreszahl (Hexa-Dezimal)",
			"", "",
		},
	},

	{
		0x02cf,
		"Datenverfaelschung. (T003) (Prozessanstoss mit unzulaessigem Ereignis-Typ)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Ereignis-Typ#",
			"#Opcode#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x02d0,
		"Datenverfaelschung in der DKV. (T001) (Unbekannte Einrichtung in den tasklokalen Daten (LODEAD))",
		2,
		{
			"Phys. Einr.-Nr. der FDS",
			"Phys. Einr.-Nr. aus LODEAD",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x02d1,
		"Datenverfaelschung in der DKV. (T001)  (unzulaessiger Return-Code vom OS-UP WTAKOM bei  Pufferanforderung fuer DKV-interne Signalisierung YUBAI)",
		2,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code von WTAKOM#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x02d2,
		"Steuerparameter (#Opcode#) in der Enderoutine der VT unzulaessig, die lokalen Daten enthalten im Byte 15 keinen zugelassenen Steuerparameter, der Prozess wird beendet",
		8,
		{
			"Physikalische Einrichtungs-Nummer der FDS",
			"Falscher #Opcode#",
			"#Ident-Nummer#",
			"Nummer des vom Verbindungsabbruch betroffenen Teilnehmers .low  Byte",
			"Nummer des vom Verbindungsabbruch betroffenen Teilnehmers .high Byte",
			"Nationalitaet und MSC - Nummer des betroffenen Teilnehmers",
			"Prozess-Adresse      high Byte",
			"Prozess-Adresse      low  Byte",
			"", "",
		},
	},

	{
		0x02d3,
		"die BS ist nicht im Warteschlangenzustand, aber trotzdem ist kein SPK fuer die VT verfuegbar, fuer die BS wird der Zustand \"Warteschlange\" eingetragen",
		8,
		{
			"Physikalische Einrichtungs-Nummer der FDS",
			"Frei",
			"#Ident-Nummer#",
			"Frei",
			"Frei",
			"Frei",
			"Prozess-Adresse      high Byte",
			"Prozess-Adresse      low  Byte",
			"", "",
		},
	},

	{
		0x02d6,
		"Fuer einen Prozess ist eine nicht zugelassene Signalisierung eingetroffen",
		6,
		{
			"Physikalische Einrichtungs-Nummer der FDS",
			"#Ereignis-Typ#",
			"#Opcode#",
			"#Ident-Nummer#",
			"Prozess-Adresse      high Byte",
			"Prozess-Adresse      low  Byte",
			"", "", "", "",
		},
	},

	{
		0x02d7,
		"Ein einzubuchender Teilnehmer wird wegen Ungleichheit der Teilnehmerdaten in den lokalen Daten des Prozesses und der Aktivdatei nicht eingebucht",
		8,
		{
			"Physikalische Einrichtungs-Nummer der FDS",
			"Frei",
			"#Ident-Nummer#",
			"Nummer des vom Verbindungsabbruch betroffenen Teilnehmers           low  Byte",
			"Nummer des vom Verbindungsabbruch betroffenen Teilnehmers           high Byte",
			"Nationalitaet und MSC - Nummer des betroffenen Teilnehmers",
			"Prozess-Adresse      high Byte",
			"Prozess-Adresse      low  Byte",
			"", "",
		},
	},

	{
		0x02e4,
		"WSV-Datenfehler: BS mit WS-Betrieb, aber keine Zuteillisten eingerichtet. Die BS arbeitet deswegen ohne WS-Betrieb.",
		2,
		{
			"Physikalische Einrichtungs-Nummer der FDS",
			"Frei",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x02e5,
		"WSV-Datenfehler: Bei der Streichung eines Eintrags im Vorhof fuer gehende Verbindungen war die Anzahl der Vorhofeintraege bereits 0.",
		1,
		{
			"Physikalische Einrichtungsnummer der FDS",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x02e6,
		"WSV-Datenfehler: Eintrag in der Zuteilliste nicht moeglich. Der Eintrag wird nicht weiter bearbeitet.",
		2,
		{
			"Physikalische Einrichtungs-Nummer der FDS",
			"#Verbindungsart# des abgelehnten Eintrags",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x02e7,
		"WSV-Datenfehler: Alle Plaetze dieser Verbindungsart belegt. Der Eintragswunsch wird nicht weiter bearbeitet.",
		2,
		{
			"Physikalische Einrichtungs-Nummer der FDS",
			"#Verbindungsart# des abgelehnten Eintrags",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x02ee,
		"Datenverfaelschung im BS-Anlauf. (T055) (Unzulaessiger Return-Code aus DKV-interner Signalisierung XTAQI)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code von XTAQI#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x02ef,
		"Datenverfaelschung bzw. SW-Fehler in der DKV. (T004) (Fuer die angegebene Einrichtung existiert kein  Zwillings-OSK laut Return-Code des Macro WMZWLO)",
		5,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Einrichtungs-Typ#",
			"#Log. Einr.-Nr.#",
			"#Return-Code von WMZWLO#",
			"Eigene Ident-Nr",
			"", "", "", "", "",
		},
	},

	{
		0x02f2,
		"SW-Fehler in der DKV. (T002)  (Beim Absenden einer DKV-internen Signalisierung meldet  das OS-UP WTAKOM : Ident-Nr. nicht existent)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"Ident-Nr. aus ausgesandter Signalisierung",
			"#Opcode# aus ausgesandter Signalisierung",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x02f3,
		"DMA-HW in einer oder beiden FDSen oder Verbindungs-Folie zwischen den FDSen defekt. Beim DMA uebertragene Kontrollbytes stimmen nach dem DMA von Einzelsignalisierungen mit der Parallel-FDS nicht mehr. Fehlererkennung in der STB-FDS (= DMA-Durchfuehrende). (Fehlerursache laesst sich nicht durch Indizien belegen!)",
		1,
		{
			"DKV-Nummer",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x02f4,
		"Datenverfaelschung bei Verwaltungsdaten des OS. (Das Kontrollbyte am Anfang des DMA-Uebertragungsfeldes fuer Einzelsignalisierungen zur Parallel-FDS stimmt vor der Uebertragung nicht. (DMA-Erleidende). Indizien lassen nur bedingt auf Fehler schliessen!)",
		8,
		{
			"DKV-Nummer",
			"#Ident-Nummer#",
			"LOW-PC des letzten Prozesses",
			"HIGH-PC des letzten Prozesses",
			"Prozess-Status 1 des letzten Prozesses (LOW-TO)",
			"Prozess-Status 1 des letzten Prozesses (HIGH-TO)",
			"Prozess-Status 2  des letzten Prozesses",
			"Prozess-Status 3 des letzten Prozesses",
			"", "",
		},
	},

	{
		0x02f5,
		"Datenverfaelschung bei Verwaltungsdaten des OS. (Das Kontrollbyte am Ende des DMA-Uebertragungsfeldes fuer Einzelsignalisierungen zur Parallel-FDS stimmt vor der Uebertragung nicht. (DMA-Erleidende). Indizien lassen nur bedingt auf Fehler schliessen!)",
		8,
		{
			"DKV-Nummer",
			"#Ident-Nummer#",
			"LOW-PC des letzten Prozesses",
			"HIGH-PC des letzten Prozesses",
			"Prozess-Status 1 des letzten Prozesses (LOW-TO)",
			"Prozess-Status 1 des letzten Prozesses (HIGH-TO)",
			"Prozess-Status 2 des letzten Prozesses",
			"Prozess-Status 3 des letzten Prozesses",
			"", "",
		},
	},

	{
		0x02f6,
		"Datenverfaelschung bei der Uebergabe der in den Indizien angegebenen Systemmeldungs-Nummer. (uebergebene Systemmeldungs-Nummer ist kleiner als die niedrigste Systemmeldungs-Nummer, SW-Reset, in den ZIB werden Anwender-Systemmeldungs-Nummer und Anwender- Indizien uebernommen)",
		10,
		{
			"Anwender-Systemmeldungs-Nummer (High-Byte)",
			"Anwender-Systemmeldungs-Nummer (Low-Byte)",
			"Anwender-ROM-Speicherbank-Nummer",
			"Anwender-Befehlszaehler (High-Byte)",
			"Anwender-Befehlszaehler (Low-Byte)",
			"Siehe Beschreibung zu obiger Systemmeldungs-Nummer, Indizien-Byte 0",
			"Siehe Beschreibung zu obiger Systemmeldungs-Nummer, Indizien-Byte 1",
			"Siehe Beschreibung zu obiger Systemmeldungs-Nummer, Indizien-Byte 2",
			"Siehe Beschreibung zu obiger Systemmeldungs-Nummer, Indizien-Byte 3",
			"Siehe Beschreibung zu obiger Systemmeldungs-Nummer, Indizien-Byte 4",
		},
	},

	{
		0x02f7,
		"Datenverfaelschung bei der Uebergabe der in den Indizien angegebenen Systemmeldungs-Nummer. (uebergebene Systemmeldungs-Nummer ist die Nummer einer Systemmeldungs-Leiche)",
		10,
		{
			"Anwender-Systemmeldungs-Nummer (High-Byte)",
			"Anwender-Systemmeldungs-Nummer (Low-Byte)",
			"Anwender-ROM-Speicherbank-Nummer",
			"Anwender-Befehlszaehler (High-Byte)",
			"Anwender-Befehlszaehler (Low-Byte)",
			"Siehe Beschreibung zu obiger Systemmeldungs-Nummer, Indizien-Byte 0",
			"Siehe Beschreibung zu obiger Systemmeldungs-Nummer, Indizien-Byte 1",
			"Siehe Beschreibung zu obiger Systemmeldungs-Nummer, Indizien-Byte 2",
			"Siehe Beschreibung zu obiger Systemmeldungs-Nummer, Indizien-Byte 3",
			"Siehe Beschreibung zu obiger Systemmeldungs-Nummer, Indizien-Byte 4",
		},
	},

	{
		0x02f8,
		"Meldezyklus-Ausgabepuffer fuer Frequenz 2 wird beantragt, obwohl keiner mehr frei ist.",
		8,
		{
			"DKV-Nummer",
			"#Ident-Nummer#",
			"LOW-PC des letzten Prozesses",
			"HIGH-PC des letzten Prozesses",
			"Prozess-Status 1 des letzten Prozesses (LOW-TO)",
			"Prozess-Status 1 des letzten Prozesses (HIGH-TO)",
			"Prozess-Status 2 des letzten Prozesses",
			"Prozess-Status 3 des letzten Prozesses",
			"", "",
		},
	},

	{
		0x02f9,
		"Meldezyklus-Ausgabepuffer fuer Frequenz 3 wird beantragt, obwohl keiner mehr frei ist.",
		8,
		{
			"DKV-Nummer",
			"#Ident-Nummer#",
			"LOW-PC des letzten Prozesses",
			"HIGH-PC des letzten Prozesses",
			"Prozess-Status 1 des letzten Prozesses (LOW-TO)",
			"Prozess-Status 1 des letzten Prozesses (HIGH-TO)",
			"Prozess-Status 2 des letzten Prozesses",
			"Prozess-Status 3 des letzten Prozesses",
			"", "",
		},
	},

	{
		0x02fa,
		"Bei der Abfrage, ob Meldezyklusausgabepuffer frei sind, wurde eine falsche Frequenznummer angegeben.",
		8,
		{
			"DKV-Nummer",
			"#Ident-Nummer#",
			"LOW-PC des letzten Prozesses",
			"HIGH-PC des letzten Prozesses",
			"Prozess-Status 1 des letzten Prozesses (LOW-TO)",
			"Prozess-Status 1 des letzten Prozesses (HIGH-TO)",
			"Prozess-Status 2 des letzten Prozesses",
			"Prozess-Status 3 des letzten Prozesses",
			"", "",
		},
	},

	{
		0x02fb,
		"Beim Austragen eines Meldeszyklus-Puffers wurde eine falsche Frequenznummer uebergeben.",
		8,
		{
			"DKV-Nummer",
			"#Ident-Nummer#",
			"LOW-PC des letzten Prozesses",
			"HIGH-PC des letzten Prozesses",
			"Prozess-Status 1 des letzten Prozesses (LOW-TO)",
			"Prozess-Status 1 des letzten Prozesses (HIGH-TO)",
			"Prozess-Status 2 des letzten Prozesses",
			"Prozess-Status 3 des letzten Prozesses",
			"", "",
		},
	},

	{
		0x02fc,
		"Fehler bei DMA-Uebertragung von ACT-FDS nach STB-FDS (AD + AL)",
		7,
		{
			"DKV-Nummer",
			"Zustand der FDSen (Port #DYIFDS#)",
			"SB-Block-DMA-Uebertragungszaehler (Byte #IADUEZ#)",
			"4-Draht-Schnittstelle zur Parallel-FDS (Port #DPP1C#)",
			"DMA-Zeitueberwachungs-Bit der Parallel-FDS (Port #DYIER0#)",
			"60V-Ueberwachung (Byte #VW60VI#)",
			"DMA-Sperre (Byte #SYSDMP#)",
			"", "", "",
		},
	},

	{
		0x02fd,
		"Beim Austragen eines Meldeszyklus-Ausgangspuffers wurde eine falsche Frequenznummer uebergeben.",
		8,
		{
			"DKV-Nummer",
			"#Ident-Nummer#",
			"LOW-PC des letzten Prozesses",
			"HIGH-PC des letzten Prozesses",
			"Prozess-Status 1 des letzten Prozesses (LOW-TO)",
			"Prozess-Status 1 des letzten Prozesses (HIGH-TO)",
			"Prozess-Status 2 des letzten Prozesses",
			"Prozess-Status 3 des letzten Prozesses",
			"", "",
		},
	},

	{
		0x02fe,
		"Meldezyklus-Ausgabepuffer fuer Frequenz 0 wird beantragt, obwohl keiner mehr frei ist.",
		8,
		{
			"DKV-Nummer",
			"#Ident-Nummer#",
			"LOW-PC des letzten Prozesses",
			"HIGH-PC des letzten Prozesses",
			"Prozess-Status 1 des letzten Prozesses (LOW-TO)",
			"Prozess-Status 1 des letzten Prozesses (HIGH-TO)",
			"Prozess-Status 2 des letzten Prozesses",
			"Prozess-Status 3 des letzten Prozesses",
			"", "",
		},
	},

	{
		0x02ff,
		"Ein Meldezyklus-Ausgabepuffer (Frequenznummer 1) wurde beantragt, obwohl keiner mehr frei war.",
		8,
		{
			"DKV-Nummer",
			"#Ident-Nummer#",
			"LOW-PC des letzten Prozesses",
			"HIGH-PC des letzten Prozesses",
			"Prozess-Status 1 des letzten Prozesses (LOW-TO)",
			"Prozess-Status 1 des letzten Prozesses (HIGH-TO)",
			"Prozess-Status 2 des letzten Prozesses",
			"Prozess-Status 3 des letzten Prozesses",
			"", "",
		},
	},

	{
		0x0301,
		"HW-Test Timer 8254 neg. Ausgang (Nur Eintrag im ZIB)",
		3,
		{
			"#Testnummer#",
			"#Testfehlernummer#",
			"#Anlaufverfolger#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0302,
		"HW-Test Interrupt-Controller neg. Ausgang (Nur Eintrag in ZIB).",
		3,
		{
			"#Testnummer#",
			"#Testfehlernummer#",
			"#Anlaufverfolger#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0303,
		"Test Aktiv-Datei-Suche neg. Ausgang (Nur Eintrag in ZIB)",
		3,
		{
			"#Testnummer#",
			"#Testfehlernummer#",
			"#Anlaufverfolger#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0304,
		"Test DMA Baustein 8237 neg. Ausgang (Nur Eintrag in ZIB)",
		3,
		{
			"#Testnummer#",
			"#Testfehlernummer#",
			"#Anlaufverfolger#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0305,
		"DMA-Test DKV intern neg. Ausgang (Nur Eintrag in ZIB)",
		3,
		{
			"#Testnummer#",
			"#Testfehlernummer#",
			"#Anlaufverfolger#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0306,
		"DMA-Test DKO-DKV (Nur Eintrag in ZIB)",
		3,
		{
			"#Testnummer#",
			"#Testfehlernummer#",
			"#Anlaufverfolger#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0307,
		"Zeitueberlauf und Zeitschleife beim Warten auf DKO-. Quittung bei verschiedenen Handshakes (Nur Eintrag in ZIB)",
		3,
		{
			"#Testnummer#",
			"#Testfehlernummer#",
			"#Anlaufverfolger#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0308,
		"DKO-DMUE-FR-L liegt an, obwohl kein Interrupt. ausgegeben wurde (Nur Eintrag in ZIB)",
		4,
		{
			"#Testnummer#",
			"#Testfehlernummer#",
			"#Anlaufverfolger#",
			"Speicherbank",
			"", "", "", "", "", "",
		},
	},

	{
		0x0309,
		"WD-Test neg. Ausgang, DKV-WD-Zeit zu lang (Nur Eintrag in ZIB)",
		3,
		{
			"#Testnummer#",
			"#Testfehlernummer#",
			"#Anlaufverfolger#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x030a,
		"WD-Test neg. Ausgang, bei DKO-WD-Test hat DKO WD. retriggert (Nur Eintrag in ZIB).",
		3,
		{
			"#Testnummer#",
			"#Testfehlernummer#",
			"#Anlaufverfolger#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x030b,
		"WD-Test neg. Ausgang, Steuerbyte enthaelt ungueltigen Wert (Nur Eintrag in ZIB)",
		3,
		{
			"#Testnummer#",
			"#Testfehlernummer#",
			"#Anlaufverfolger#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x030c,
		"WD-Test neg. Ausgang, DKO-WD-Zeit zu lang (Nur Eintrag in ZIB)",
		3,
		{
			"#Testnummer#",
			"#Testfehlernummer#",
			"#Anlaufverfolger#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x030d,
		"Resetverursacher WD / ausser WD-Test. (Nur Eintrag in ZIB)",
		9,
		{
			"FDS-Statusport #DYIFDS#",
			"RESET-Register #DYIRES#",
			"Fehlerregister 1 #DYIER0#",
			"Fehlerregister 2 #DYIER1#",
			"#Anlaufverfolger#",
			"PC-Low der letzten gelaufener Task",
			"PC-High der letzten gelaufenen Task",
			"Prozess-Status 3 (Aktivbit, Readybit, Bank) der letzten gelaufenen Task",
			"Abgelaufener WD: DKO = 0FFH, DKV = 00",
			"",
		},
	},

	{
		0x030e,
		"Hardware meldete: \"Taste RESET wurde betaetigt\" (Nur Eintrag in ZIB)",
		10,
		{
			"FDS-Statusport #DYIFDS#",
			"RESET-Register #DYIRES#",
			"Fehlerregister 1 #DYIER0#",
			"Fehlerregister 2 #DYIER1#",
			"#Anlaufverfolger#",
			"Bei gleichzeitigem SW-RESET FYSIND + 0",
			"Bei gleichzeitigem SW-RESET FYSIND + 1",
			"Bei gleichzeitigem SW-RESET FYSIND + 2",
			"Bei gleichzeitigem SW-RESET FYSIND + 3",
			"Bei gleichzeitigem SW-RESET FYSIND + 4",
		},
	},

	{
		0x030f,
		"ST-HW-Test negativer Ausgang (Am KAN-Ende ist die DKV weder \"Aktiv und Verfuegbar\" noch \"Passiv und Verfuegbar\") (Nur Eintrag in ZIB)",
		3,
		{
			"#Testnummer#",
			"#Testfehlernummer#",
			"#Anlaufverfolger#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0310,
		"Test RAM-Speicherbankumschaltung neg. Ausgang. (Nur Eintrag in ZIB)",
		3,
		{
			"#Testnummer#",
			"#Testfehlernummer#",
			"#Anlaufverfolger#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0311,
		"Test ROM-Speicherbankumschaltung neg. Ausgang. (Nur Eintrag in ZIB)",
		3,
		{
			"#Testnummer#",
			"#Testfehlernummer#",
			"#Anlaufverfolger#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0312,
		"ROM-Summentest neg. Ausgang (Grundblock) (Nur Eintrag in ZIB)",
		3,
		{
			"#Testnummer#",
			"#Testfehlernummer#",
			"#Anlaufverfolger#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0313,
		"ROM-Summentest neg. Ausgang (Speicherbaenke) (Nur Eintrag in ZIB)",
		4,
		{
			"#Testnummer#",
			"#Testfehlernummer#",
			"#Anlaufverfolger#",
			"Speicherbanknummer der geprueften ROM-SB",
			"", "", "", "", "", "",
		},
	},

	{
		0x0314,
		"ROM-Summentest Anlagenliste neg. Ausgang (Nur Eintrag in ZIB)",
		3,
		{
			"#Testnummer#",
			"#Testfehlernummer#",
			"#Anlaufverfolger#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0315,
		"Nicht inhaltzerstoerender RAM-Test (Teile des GB) (Nur Eintrag in ZIB)",
		3,
		{
			"#Testnummer#",
			"#Testfehlernummer#",
			"#Anlaufverfolger#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0316,
		"Inhaltzerstoerender RAM-Test (Teile des GB) (Nur Eintrag in ZIB)",
		3,
		{
			"#Testnummer#",
			"#Testfehlernummer#",
			"#Anlaufverfolger#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0317,
		"Inhaltzerstoerender RAM-Test (Speicherbank 0). neg Ausgang (Nur Eintrag in ZIB)",
		3,
		{
			"#Testnummer#",
			"#Testfehlernummer#",
			"#Anlaufverfolger#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0318,
		"Inhaltzerstoerender RAM-Test (Speicherbank 1). neg Ausgang (Nur Eintrag in ZIB)",
		3,
		{
			"#Testnummer#",
			"#Testfehlernummer#",
			"#Anlaufverfolger#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0319,
		"Inhaltzerstoerender RAM-Test (Speicherbank 2). neg Ausgang (Nur Eintrag in ZIB)",
		3,
		{
			"#Testnummer#",
			"#Testfehlernummer#",
			"#Anlaufverfolger#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x031a,
		"Inhaltzerstoerender RAM-Test (Speicherbank 3). neg Ausgang (Nur Eintrag in ZIB)",
		3,
		{
			"#Testnummer#",
			"#Testfehlernummer#",
			"#Anlaufverfolger#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x031b,
		"Inhaltzerstoerender RAM-Test (Speicherbank 4). neg Ausgang (Nur Eintrag in ZIB)",
		3,
		{
			"#Testnummer#",
			"#Testfehlernummer#",
			"#Anlaufverfolger#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x031c,
		"Inhaltzerstoerender RAM-Test (Speicherbank 5). neg Ausgang (Nur Eintrag in ZIB)",
		3,
		{
			"#Testnummer#",
			"#Testfehlernummer#",
			"#Anlaufverfolger#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x031d,
		"Nichtinhaltzerstoerender RAM-Test (Speicherbank 6) neg Ausgang (Nur Eintrag in ZIB)",
		3,
		{
			"#Testnummer#",
			"#Testfehlernummer#",
			"#Anlaufverfolger#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x031e,
		"RESET-Register laesst sich nicht ruecksetzen. (Nur Eintrag in ZIB)",
		3,
		{
			"#Testnummer#",
			"RESET-Register #DYIRES#",
			"#Anlaufverfolger#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x031f,
		"ST-HW-Test negativer Ausgang (Nach RESET war Verfuegbar gesetzt oder es war weder Fehlerbehandlung noch Defekt gesetzt) (Nur Eintrag in ZIB)",
		3,
		{
			"#Testnummer#",
			"FDS-Statusport #DYIFDS#",
			"#Anlaufverfolger#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0320,
		"ST-HW-Test (Fehlerbehandlung oder Defekt lassen sich nicht rueck-. setzen) (Nur Eintrag in ZIB)",
		3,
		{
			"#Testnummer#",
			"FDS-Statusport #DYIFDS#",
			"#Anlaufverfolger#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0321,
		"ST-HW-Test (Defekt laesst sich nicht ruecksetzen) (Nur Eintrag in ZIB)",
		3,
		{
			"#Testnummer#",
			"FDS-Statusport #DYIFDS#",
			"#Anlaufverfolger#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0322,
		"Die Versionen der Speicherbaenke sind unterschiedlich (Nur Eintrag in ZIB)",
		3,
		{
			"#Testnummer#",
			"Speicherbanknummer der ersten Bank deren Version sich von der Grundblockversion unterscheidet",
			"#Anlaufverfolger#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0323,
		"SW-Produktion benachrichtigen (Ueberschneidungen von RAM-Bereichen) (Nur Eintrag in ZIB)",
		3,
		{
			"#Testnummer#",
			"#Testfehlernummer#",
			"#Anlaufverfolger#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0324,
		"Fehler bei ST-HW-Test (YPST2V) aufgetreten. (Nur Eintrag in ZIB)",
		3,
		{
			"#Testnummer#",
			"#Testfehlernummer#",
			"#Anlaufverfolger#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0325,
		"Nichtinhaltzerstoerender RAM-Test (Speicherbank 7) neg Ausgang (Nur Eintrag in ZIB)",
		3,
		{
			"#Testnummer#",
			"#Testfehlernummer#",
			"#Anlaufverfolger#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0326,
		"Inhaltzerstoerender RAM-Test (Speicherbank 8). neg Ausgang (Nur Eintrag in ZIB)",
		3,
		{
			"#Testnummer#",
			"#Testfehlernummer#",
			"#Anlaufverfolger#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0327,
		"Inhaltzerstoerender RAM-Test (Speicherbank 9). neg Ausgang (Nur Eintrag in ZIB)",
		3,
		{
			"#Testnummer#",
			"#Testfehlernummer#",
			"#Anlaufverfolger#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0328,
		"Inhaltzerstoerender RAM-Test (Speicherbank 10) neg Ausgang (Nur Eintrag in ZIB)",
		3,
		{
			"#Testnummer#",
			"#Testfehlernummer#",
			"#Anlaufverfolger#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0329,
		"Inhaltzerstoerender RAM-Test (Speicherbank 11) neg Ausgang (Nur Eintrag in ZIB)",
		3,
		{
			"#Testnummer#",
			"#Testfehlernummer#",
			"#Anlaufverfolger#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x032a,
		"Inhaltzerstoerender RAM-Test (Speicherbank 12) neg Ausgang (Nur Eintrag in ZIB)",
		3,
		{
			"#Testnummer#",
			"#Testfehlernummer#",
			"#Anlaufverfolger#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x032b,
		"Inhaltzerstoerender RAM-Test (Speicherbank 13) neg Ausgang (Nur Eintrag in ZIB)",
		3,
		{
			"#Testnummer#",
			"#Testfehlernummer#",
			"#Anlaufverfolger#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x032c,
		"Inhaltzerstoerender RAM-Test (Speicherbank 14) neg Ausgang (Nur Eintrag in ZIB)",
		3,
		{
			"#Testnummer#",
			"#Testfehlernummer#",
			"#Anlaufverfolger#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x032d,
		"Inhaltzerstoerender RAM-Test (Speicherbank 15) neg Ausgang (Nur Eintrag in ZIB)",
		3,
		{
			"#Testnummer#",
			"#Testfehlernummer#",
			"#Anlaufverfolger#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x032e,
		"Hardware meldete: \"5V Ausfall\" (Nur Eintrag in ZIB)",
		10,
		{
			"FDS-Statusport #DYIFDS#",
			"RESET-Register #DYIRES#",
			"Fehlerregister 1 #DYIER0#",
			"Fehlerregister 2 #DYIER1#",
			"#Anlaufverfolger#",
			"Bei gleichzeitigem SW-RESET FYSIND + 0",
			"Bei gleichzeitigem SW-RESET FYSIND + 1",
			"Bei gleichzeitigem SW-RESET FYSIND + 2",
			"Bei gleichzeitigem SW-RESET FYSIND + 3",
			"Bei gleichzeitigem SW-RESET FYSIND + 4",
		},
	},

	{
		0x032f,
		"Hardware meldete: \"6.4 MHz Ausfall\" (Nur Eintrag in ZIB)",
		10,
		{
			"FDS-Statusport #DYIFDS#",
			"RESET-Register #DYIRES#",
			"Fehlerregister 1 #DYIER0#",
			"Fehlerregister 2 #DYIER1#",
			"#Anlaufverfolger#",
			"Bei gleichzeitigem SW-RESET FYSIND + 0",
			"Bei gleichzeitigem SW-RESET FYSIND + 1",
			"Bei gleichzeitigem SW-RESET FYSIND + 2",
			"Bei gleichzeitigem SW-RESET FYSIND + 3",
			"Bei gleichzeitigem SW-RESET FYSIND + 4",
		},
	},

	{
		0x0330,
		"Hardware meldete: \"DKO-Teilerkettenausfall\" (Nur Eintrag in ZIB)",
		10,
		{
			"FDS-Statusport #DYIFDS#",
			"RESET-Register #DYIRES#",
			"Fehlerregister 1 #DYIER0#",
			"Fehlerregister 2 #DYIER1#",
			"#Anlaufverfolger#",
			"Bei gleichzeitigem SW-RESET FYSIND + 0",
			"Bei gleichzeitigem SW-RESET FYSIND + 1",
			"Bei gleichzeitigem SW-RESET FYSIND + 2",
			"Bei gleichzeitigem SW-RESET FYSIND + 3",
			"Bei gleichzeitigem SW-RESET FYSIND + 4",
		},
	},

	{
		0x0331,
		"Hardware meldete: \"DMA-Zeitueberwachung hat angesprochen\" (Nur Eintrag in ZIB)",
		10,
		{
			"FDS-Statusport #DYIFDS#",
			"RESET-Register #DYIRES#",
			"Fehlerregister 1 #DYIER0#",
			"Fehlerregister 2 #DYIER1#",
			"#Anlaufverfolger#",
			"Bei gleichzeitigem SW-RESET FYSIND + 0",
			"Bei gleichzeitigem SW-RESET FYSIND + 1",
			"Bei gleichzeitigem SW-RESET FYSIND + 2",
			"Bei gleichzeitigem SW-RESET FYSIND + 3",
			"Bei gleichzeitigem SW-RESET FYSIND + 4",
		},
	},

	{
		0x0332,
		"Hardware meldete: \"Taste Dauertest wurde gedrueckt\" (Nur Eintrag in ZIB)",
		10,
		{
			"FDS-Statusport #DYIFDS#",
			"RESET-Register #DYIRES#",
			"Fehlerregister 1 #DYIER0#",
			"Fehlerregister 2 #DYIER1#",
			"#Anlaufverfolger#",
			"Bei gleichzeitigem SW-RESET FYSIND + 0",
			"Bei gleichzeitigem SW-RESET FYSIND + 1",
			"Bei gleichzeitigem SW-RESET FYSIND + 2",
			"Bei gleichzeitigem SW-RESET FYSIND + 3",
			"Bei gleichzeitigem SW-RESET FYSIND + 4",
		},
	},

	{
		0x0351,
		"Der Anlauf mit der im BYTE 1 angegebenen Einrichtung wird von der FDS in eine Warteschlange eingereiht wegen der anzahlmaessigen Begrenzung parallel laufender Anlaeufe von FUPEF-Einrichtungen. (T043) (Einsetzen der Anlaufbegrenzung)",
		4,
		{
			"Phys. Einr.-Nr. der eigenen FDS",
			"Phys. Einr.-Nr. aus LODEAD",
			"#Return-Code KON# (Prozedur YPKPPA)",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x0352,
		"Datenverfaelschung beim Nachbehandeln eines in eine Warteschlange eingereihten Anlaufs mit einer peripheren Einrichtung. (T052) (Einrichtungs-Typ in LOD-Daten verfaelscht;  erwartete Werte:  KYTSPK (2), KYTPFG (4), KYTPBR (5), KYTFME (6))",
		5,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Einrichtungs-Typ#",
			"#Log. Einr.-Nr.#",
			"eigene Ident-Nr",
			"Ident.-Nr. des Auftraggebers des letzten nicht aus der Warteschlange geholten Konfigurationsauftrages",
			"", "", "", "", "",
		},
	},

	{
		0x0353,
		"Datenverfaelschung beim Nachbehandeln eines in eine Warteschlange eingereihten Anlaufs mit einer peripheren Einrichtung. (T052) (Prozessanstoss mit nicht erwartetem Ereignis-Typ)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Ereignis-Typ#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0354,
		"Datenverfaelschung in der DKV. (T001) (Negativer Return-Code vom OS-UP WTAKOM bei Pufferanforderung fuer die DKV-interne Signalisierung YKAQI)",
		5,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Einrichtungs-Typ#",
			"#Log. Einr.-Nr.#",
			"eigene Ident-Nr",
			"Ident-Nr. des Auftraggebers des Konfigurations- auftrags",
			"", "", "", "", "",
		},
	},

	{
		0x0355,
		"Datenverfaelschung beim Anlauf mit dem MSC. (T088) (Parameterfehler: unzulaessige Log. Einr.-Nr. im ACT-Auftrag)",
		5,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Einrichtungs-Typ# aus ACT-Auftrag",
			"#Log. Einr.-Nr.# aus ACT-Auftrag",
			"eigene Ident-Nr",
			"Ident-Nr. des Auftraggebers",
			"", "", "", "", "",
		},
	},

	{
		0x035b,
		"Datenverfaelschung beim Anlauf mit dem MSC. (T088) (Beim Absenden der DKV-internen Signalisierung YKAAI meldet  das OS-UP WTAKOM: Ident-Nummer nicht existent)",
		1,
		{
			"Phys. Einr.-Nr. der FDS",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x035c,
		"Datenverfaelschung beim Anlauf mit dem MSC. (T088) (Beim Absenden der DKV-internen Signalisierung XTAAI meldet  das OS-UP WTAKOM: Ident-Nummer nicht existent.)",
		1,
		{
			"Phys. Einr.-Nr. der FDS",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x035e,
		"Datenverfaelschung beim Anlauf mit dem MSC. (T088) (Prozessanstoss mit unzulaessigem Ereignis-Typ)",
		6,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Ereignis-Typ#",
			"Ident-Nr. des Absenders der Signalisierung",
			"#Opcode# aus der Signalisierung",
			"1.Byte der Signalisierung : #Entwickler-Info#",
			"2.Byte der Signalisierung : #Entwickler-Info#",
			"", "", "", "",
		},
	},

	{
		0x036b,
		"Datenverfaelschung in der DKV. (T001) (unzulaessiger Return-Code vom OS-UP WTAKOM bei Puffer-  anforderung fuer DKV-interne Signalisierung YOIAI an BT)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code von WTAKOM#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x036c,
		"Datenverfaelschung in der DKV. (T001) (Unzulaessiger Return-Code vom OS-UP WTAKOM bei Pufferanforderung fuer die DKV-interne Signalisierung XTBAI an die BT)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code von WTAKOM#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x036d,
		"Datenverfaelschung in der DKV. (T001) (Unzulaessiger Return-Code vom OS-UP WTAKOM bei Pufferanforderung fuer die DKV-interne Signalisierung YKKQI)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code von WTAKOM#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x036e,
		"Datenverfaelschung in der DKV. (T001) (Unzulaessiger Return-Code vom OS-UP WTAKOM bei Puffer-  anforderung fuer DKV-interne Signalisierung YVAAI)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code von WTAKOM#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0373,
		"Datenverfaelschung in der DKV. (T001) (Prozessanstoss mit unzulaessigem Ereignis-Typ)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"#Ereignis-Typ#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0374,
		"Datenverfaelschung. (T003) (Prozessanstoss mit unzulaessigem Ereignis)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Ereignis-Typ#",
			"#Opcode#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0376,
		"Datenverfaelschung in der DKV. (T001) (Unzulaessiger Return-Code vom MUK-UP YPNOK1)",
		2,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code MUK#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x037a,
		"Datenverfaelschung in der DKV. (T001) (Unzulaessiger Return-Code vom MUK-UP YPNZ03)",
		2,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code MUK#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x0384,
		"Datenverfaelschung. (T003) (Prozessanstoss mit unzulaessigem Ereignis-Typ)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Ereignis-Typ#",
			"#Opcode#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0385,
		"Ein SPK wird vom VT-Prozess nicht freigegeben, da die Identnummer in den lokalen Daten nicht der in der SPK-Liste entspricht",
		8,
		{
			"Physikalische Einrichtungs-Nummer der FDS",
			"Logische Einrichtungs-Nummer des SPK",
			"#Ident-Nummer#",
			"Nummer des vom Verbindungsabbruch betroffenen Teilnehmers .low  Byte",
			"Nummer des vom Verbindungsabbruch betroffenen Teilnehmers .high Byte",
			"Nationalitaet und MSC - Nummer des betroffenen Teilnehmers",
			"Prozess-Adresse      high Byte",
			"Prozess-Adresse      low  Byte",
			"", "",
		},
	},

	{
		0x0387,
		"Der Anlauf mit dem MSC konnte wegen ausbleibender Signalisierungen vom MSC nicht erfolgreich beendet werden. (T018) (Timeout fuer ACT-Auftrag fuer MSC)",
		6,
		{
			"Phys. Einr.-Nr. (LODEAD)",
			"#Globaler ST-Zustand# der BS",
			"#Globaler ST-Zustand# des MSC",
			"#Globaler ST-Zustand# des SAE 1",
			"#Globaler ST-Zustand# des SAE 2",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"", "", "", "",
		},
	},

	{
		0x038a,
		"Timeout: Ausbleiben einer Signalisierung vom SPK, aber in der SPK-Liste ist nicht mehr die Identnummer des Prozesses eingetragen, der SPK ist angelaufen oder gesperrt oder OSK(OGK)",
		8,
		{
			"Physikalische Einrichtungs-Nummer des SPK",
			"#Opcode# der ausgebliebenen Signalisierung",
			"Ident-Nummer",
			"Nummer des vom Verbindungsabbruch betroffenen Teilnehmers .low  Byte",
			"Nummer des vom Verbindungsabbruch betroffenen Teilnehmers .high Byte",
			"Nationalitaet und MSC - Nummer des betroffenen Teilnehmers",
			"Prozess-Adresse      high Byte",
			"Prozess-Adresse      low  Byte",
			"", "",
		},
	},

	{
		0x038b,
		"Datenverfaelschung im BS-Anlauf. (T055) (unzulaessiger Return-Code vom OS-UP WTAKOM bei Pufferanforderung fuer die DKV-interne Signalisierung YLANG)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code von WTAKOM#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x038c,
		"Datenverfaelschung im BS-Anlauf. (T055) (unzulaessiger Return-Code vom OS-UP WTAKOM bei Pufferanforderung fuer die DKV-interne Signalisierung YHAAI)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code von WTAKOM#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x038e,
		"Die Signalisierung an die STBFDS konnte nicht gesendet werden (Negativer Return-Code von WMESEP) DMA-Sperre gesetzt",
		2,
		{
			"FDS-Nr",
			"#Return-Code von WMESEP#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x038f,
		"Falscher Ereignis-Typ oder Opcode beim Singali- sierungsempfang vom PHE",
		5,
		{
			"FDS-Nr",
			"#Ereignis-Typ#",
			"FKS-Nummer",
			"#Ident-Nummer#",
			"#Opcode#",
			"", "", "", "", "",
		},
	},

	{
		0x0390,
		"Falscher Ereignis-Typ bei Signalisierung an ersten PHE",
		5,
		{
			"FDS-Nr",
			"#Ereignis-Typ#",
			"FKS-Nummer",
			"#Ident-Nummer#",
			"#Opcode#",
			"", "", "", "", "",
		},
	},

	{
		0x0391,
		"Falscher Ereignis-Typ bei Signalisierungsaussendung an zweiten PHE",
		5,
		{
			"FDS-Nr",
			"#Ereignis-Typ#",
			"FKS-Nr",
			"#Ident-Nummer#",
			"#Opcode#",
			"", "", "", "", "",
		},
	},

	{
		0x0392,
		"Ausbleiben der Quittung von der STBFDS",
		3,
		{
			"FDS-Nr",
			"#Ereignis-Typ#",
			"#Globaler ST-Zustand# der anderen FDS",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0393,
		"Falscher Aufruf oder Opcode beim Empfang der Quittung von der STBFDS",
		5,
		{
			"FDS-Nr",
			"#Ereignis-Typ#",
			"FKS-Nr",
			"#Ident-Nummer#",
			"#Opcode#",
			"", "", "", "", "",
		},
	},

	{
		0x0394,
		"Falscher Aufruf oder Opcode beim Signalisierungs- empfang in der STBFDS",
		5,
		{
			"FDS-Nr",
			"#Ereignis-Typ#",
			"FKS-Nr",
			"#Ident-Nummer#",
			"#Opcode#",
			"", "", "", "", "",
		},
	},

	{
		0x0395,
		"Komplementpruefung ergibt fehlerhafte Daten (FT-Zustaende)",
		9,
		{
			"FDS-Nr",
			"#FTZFQG#",
			"Komplement von #FTZFQG#",
			"#FTBFQG#",
			"Komplement von #FTBFQG#",
			"#FTPFUE# (LOW-Byte)",
			"#FTPFUE# (HIGH-Byte)",
			"Komplement von #FTPFUE# (LOW)",
			"Komplement von #FTPFUE# (HIGH)",
			"",
		},
	},

	{
		0x0396,
		"Interne Signalisierung (TFZAI) konnte nicht eingetragen werden",
		2,
		{
			"FDS-Nr",
			"#Return-Code von WTAKOM#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x0397,
		"Komplementpruefung im Anlauf ergibt fehlerhaftes Datum 'Bedingte Frequenzgenauigkeit' FTBFQG",
		3,
		{
			"FDS-Nr",
			"#FTBFQG#",
			"Komplement von #FTBFQG#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0398,
		"Datenverfaelschung in der DKV. (T001) ( Verfaelschter Return-Code aus dem Makro: YMZXVF)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"Prozedur-Identifikation des betreffenden Moduls : #Entwickler-Info#",
			"#Return-Code von YMZXVF#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x039a,
		"Datenverfaelschung bei der Ausgabe von Softwareobjektnamen. (T115) (In der Signalisierung XATAD (Softwareobjektnamenauftrag an die STBFDS) wurde von  der STBFDS ein falscher Einrichtungs-Typ festgestellt.)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"Prozedur-Identifikation des betreffenden Moduls : #Entwickler-Info#",
			"falscher #Einrichtungs-Typ# aus Signalisierung",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x039b,
		"Datenverfaelschung in der DKV. (T001) (Checksum-Fehler bei Ueberpruefung der Anlaufstufen  durch Audit)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"abgespeicherte Checksum (Istwert)",
			"ermittelte Checksum     (Sollwert)",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x039c,
		"Datenverfaelschung im BS-Anlauf. (T055) ( Aus Unterprogramm YPAKAE kommt unzulaessiger Return-Code zurueck)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code ANL#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x039f,
		"Datenverfaelschung im Anlauf der passiven FDS. (T056) (Die Uebertragung der Anlagenliste wurde nicht innerhalb der angegebenen Zeit von OS mit dem Ereignis-Typ MSDUE quittiert.)",
		4,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Ereignis-Typ#",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x03a0,
		"Datenverfaelschung im Anlauf der passiven FDS. (T056) (Prozesszustand verfaelscht)",
		3,
		{
			"Phys. Einr.-Nr.  der FDS",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x03a1,
		"Datenverfaelschung im Anlauf der passiven FDS. (T056) (Prozessanstoss mit nicht erwartetem Ereignis)",
		4,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Ereignis-Typ#",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x03a2,
		"Datenverfaelschung im Anlauf der passiven FDS. (T056) (Prozessanstoss mit nicht erwartetem Ereignis)",
		4,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Ereignis-Typ#",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x03a3,
		"SW-Fehler in der DKV. (T002) (Unzulaessiger Pruefroutinen-Aufruf.  Pruefroutinen-Tabellen-Index > KYHPRF.  <Theoretisch groesstmoeglicher Index-Wert ist 32 !>)",
		9,
		{
			"Phys. Einr.-Nr. der FDS",
			"Zyklus-Zaehler (LODPZZ) : #Entwickler-Info#",
			"Anzahl verbleib. Pruefrout. des Auftrags (LODAPR)",
			"Low-Byte des aktuellen Eintrags der Pruefroutinen- tabelle (LODAPL) : #Entwickler-Info#",
			"High-Byte des aktuellen Eintrags der Pruefroutinen- tabelle (LODAPH) : #Entwickler-Info#",
			"Pruefroutinen-Tabellen-Index (Istwert) #Entwickler-Info#",
			"#Globaler ST-Zustand# der eigenen FDS",
			"Low-Adr. der aktuellen Zyklus-Tabelle #Entwickler-Info#",
			"High-Adr. der aktuellen Zyklus-Tabelle #Entwickler-Info#",
			"",
		},
	},

	{
		0x03a4,
		"Datenverfaelschung in der DKV. (T001) (Unzulaessiger Anstoss des Audit-Prozesses (HDA).  <Falscher Ereignis-Typ, nur MTASKO erlaubt>)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Ereignis-Typ#",
			"#Opcode#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x03a5,
		"Datenverfaelschung in der DKV. (T001) (Unzulaessiger Anstoss des Audit-Prozesses (HDA). <Falscher Opcode, nur OYHAAI erlaubt>)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Ereignis-Typ#",
			"#Opcode#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x03a6,
		"Datenverfaelschung in der DKV. (T001) (Unzulaessiger Anstoss des Audit-Prozesses (HDA). <Falscher Ereignis-Typ, nur MZTO erlaubt>)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Ereignis-Typ#",
			"#Opcode#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x03a7,
		"Nicht behebbare Datenverfaelschung, durch Audits erkannt. (T054) (Erlaubnis-Tabelle defekt. Checksum-Pruefung fuer Erlaubnis-Tabelle war \"verboten\". <1. Byte der Erlaubnis-Tab., das sich auf die Pruefung der Erlaubnis-Tabelle selbst bezieht, war verfaelscht (/=KYHPST))",
		2,
		{
			"Phys. Einr.-Nr. der FDS",
			"1. Byte aus der Erlaubnis-Tab. (Soll-Wert KYHPST) #Entwickler-Info#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x03ad,
		"Datenverfaelschung in der DKV. (T001) (Unzulaessiger Return-Code von WTAKOM bei Puffer-Anforderung fuer OYKOAI)",
		3,
		{
			"#Phys. Einr.-Nr.# der FDS",
			"#Return-Code von WTAKOM#",
			"#Ident-Nummer# des eigenen Prozesses",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x03c9,
		"Datenverfaelschung beim Anstoss eines Konfigurations- auftrags. (T093) (falscher Einrichtungs-Typ im ACT-Auftrag mit  Anlaufart Erst-/Wiederanlauf)",
		5,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Einrichtungs-Typ# aus ACT-Auftrag",
			"#Log. Einr.-Nr.# aus ACT-Auftrag",
			"eigene Ident-Nr",
			"Ident-Nr. des Auftraggebers",
			"", "", "", "", "",
		},
	},

	{
		0x03ca,
		"Datenverfaelschung beim Anstoss eines Konfigurations- auftrags. (T093) (Parameterfehler: Einrichtungs-Typ fuer diesen partiellen  ACT-Auftrag nicht bekannt)",
		5,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Einrichtungs-Typ# aus ACT-Auftrag",
			"#Log. Einr.-Nr.#  aus ACT-Auftrag",
			"eigene Ident-Nr",
			"Ident-Nr. des Auftraggebers",
			"", "", "", "", "",
		},
	},

	{
		0x03cb,
		"Datenverfaelschung beim Anstoss eines Konfigurations- auftrags. (T093) (Prozessanstoss mit falschem Ereignis-Typ oder nicht erwarteter Signalisierung)",
		5,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Ereignis-Typ#",
			"Ident-Nr. des Auftraggebers",
			"#Opcode# aus der Signalisierung",
			"eigene Ident-Nr",
			"", "", "", "", "",
		},
	},

	{
		0x03cd,
		"Datenverfaelschung beim Konfigurieren einer Einrichtung nach ACT. (T095) (Beim Ueberpruefen der Anlaufart wird in der Tabelle  PYKxxx ein falsches Datum gelesen oder der Opcode des  ACT-Auftrages ist falsch)",
		7,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Globaler ST-Zustand# der zu konfigurierenden Einrichtung",
			"#Detaillierter ST-Zustand# der zu konfigurierenden Einrichtung",
			"#Opcode# des ACT-Auftrags",
			"#Einrichtungs-Typ# der zu konfigurierenden Einrichtung",
			"#Log. Einr.-Nr.# der zu konfigurierenden Einrichtung",
			"eigene Ident-Nr",
			"", "", "",
		},
	},

	{
		0x03ce,
		"Datenverfaelschung beim Konfigurieren einer Einrichtung nach ACT. (T095) (Beim Ueberpruefen der Anlaufart wird in den Tabellen  PYKGZx und PYKDZx der entsprechende ST-Zustand der  Einrichtung nicht gefunden)",
		7,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Globaler ST-Zustand# der zu konfigurierenden Einrichtung",
			"#Detaillierter ST-Zustand# der zu konfigurierenden Einrichtung",
			"#Opcode# des ACT-Auftrags",
			"#Einrichtungs-Typ# der zu konfigurierenden Einrichtung",
			"#Log. Einr.-Nr.# der zu konfigurierenden Einrichtung",
			"eigene Ident-Nr",
			"", "", "",
		},
	},

	{
		0x03cf,
		"Datenverfaelschung beim Anstoss eines Konfigurations- auftrags. (T093) (Unzulaessiger Return-Code vom OS-UP WTAKOM bei Pufferanforderung fuer die DKV-interne Signalisierung ZAAI)",
		8,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Globaler ST-Zustand# der zu konfigurierenden Einrichtung",
			"#Detaillierter ST-Zustand# der zu konfigurierenden Einrichtung",
			"#Opcode# des ACT-Auftrags",
			"#Einrichtungs-Typ# der zu konfigurierenden Einrichtung",
			"#Log. Einr.-Nr.# der zu konfigurierenden Einrichtung",
			"eigene Ident-Nr",
			"#Return-Code von WTAKOM#",
			"", "",
		},
	},

	{
		0x03d0,
		"SW-Fehler beim Konfigurieren einer Einrichtung nach ACT. (T098) (Prozess mit fester Ident-Nr. (MUK) ist  nicht vorhanden)",
		5,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Einrichtungs-Typ# der zu konfigurierenden Einrichtung",
			"#Log. Einr.-Nr.# der zu konfigurierenden Einrichtung",
			"eigene Ident-Nr",
			"Ident-Nr. des Auftraggebers",
			"", "", "", "", "",
		},
	},

	{
		0x03d1,
		"Datenverfaelschung beim Konfigurieren einer Einrichtung nach ACT. (T095) (Unzulaessiger Return-Code vom OS-UP WTAKOM bei Pufferanforderung fuer die DKV-interne Signalisierung YNSAI)",
		6,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Einrichtungs-Typ# der zu konfigurierenden Einrichtung",
			"#Log. Einr.-Nr.# der zu konfigurierenden Einrichtung",
			"eigene Ident-Nr",
			"Ident-Nr. des Auftraggebers des Konfigurations- auftrags",
			"#Return-Code von WTAKOM#",
			"", "", "", "",
		},
	},

	{
		0x03d2,
		"Test- und Traceinformation beim Konfigurieren einer Einrichtung. (T101) ( Ziel-Ident-Nummer nicht existent ,   bei Quittung an Auftraggeber )",
		5,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Einrichtungs-Typ#",
			"#Log. Einr.-Nr.#",
			"eigene Ident-Nr",
			"Ident-Nr. des Auftraggebers",
			"", "", "", "", "",
		},
	},

	{
		0x03d3,
		"Datenverfaelschung in der DKV. (T001) (Unzulaessiger Return-Code vom OS-UP WTAKOM bei Pufferanforderung fuer eine DKV-interne Signalisierung )",
		6,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Einrichtungs-Typ#",
			"#Log. Einr.-Nr.#",
			"eigene Ident-Nr",
			"Ident-Nr. des Absenders des Konfigurations- auftrags",
			"#Return-Code von WTAKOM#",
			"", "", "", "",
		},
	},

	{
		0x03d5,
		"Test- und Traceinformation beim Konfigurieren einer Einrichtung. (T101) (Alter ACT-Auftrag fuer PBR besteht-  neuer Auftrag wird beendet )",
		7,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Globaler ST-Zustand# des PBR",
			"#Detaillierter ST-Zustand# des PBR",
			"#Opcode# des aktuellen ACT-Auftrags",
			"#Einrichtungs-Typ# des PBR",
			"#Log. Einr.-Nr.# des PBR",
			"eigene Ident-Nr",
			"", "", "",
		},
	},

	{
		0x03d7,
		"Datenverfaelschung beim Konfigurieren des PBR nach ACT. (T085) (Unzulaessiger Zustandsuebergang)",
		7,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Globaler ST-Zustand# des PBR",
			"#Detaillierter ST-Zustand# des PBR",
			"#Opcode# des ACT-Auftrags",
			"#Einrichtungs-Typ# des PBR",
			"#Log. Einr.-Nr.# des PBR",
			"eigene Ident-Nr",
			"", "", "",
		},
	},

	{
		0x03d8,
		"Datenverfaelschung beim Anstoss eines Konfigurations- auftrags fuer die FDS. (T089) (Unzulaessiger Zustandsuebergang)",
		7,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Globaler ST-Zustand# der FDS",
			"#Detaillierter ST-Zustand# der FDS",
			"#Opcode# des ACT-Auftrags",
			"#Einrichtungs-Typ# der FDS",
			"#Log. Einr.-Nr.# der FDS",
			"eigene Ident-Nr",
			"", "", "",
		},
	},

	{
		0x03d9,
		"Test- und Traceinformation beim Konfigurieren einer Einrichtung. (T101) (ACT-Auftrag fuer PHE besteht bereits-  neuer ACT-Auftrag wird beendet )",
		7,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Globaler ST-Zustand# des zu konfigurierenden PHE",
			"#Detaillierter ST-Zustand# des zu konfigurierenden PHE",
			"#Opcode# des aktuellen ACT-Auftrags",
			"#Einrichtungs-Typ# des zu konfigurierenden PHE",
			"#Log. Einr.-Nr.# des zu konfigurierenden PHE",
			"eigene Ident-Nr",
			"", "", "",
		},
	},

	{
		0x03da,
		"Datenverfaelschung beim Anstoss eines Konfigurations- auftrags fuer einen PHE. (T092) (Unzulaessiger Zustandsuebergang)",
		7,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Globaler ST-Zustand# des zu konfigurierenden PHE",
			"#Detaillierter ST-Zustand# des zu konfigurierenden PHE",
			"#Opcode# des ACT-Auftrags",
			"#Einrichtungs-Typ# des zu konfigurierenden PHE",
			"#Log. Einr.-Nr.# des zu konfigurierenden PHE",
			"eigene Ident-Nr",
			"", "", "",
		},
	},

	{
		0x03db,
		"Datenverfaelschung beim Anstoss eines Konfigurations- auftrags fuer die BS. (T090) (Unzulaessiger Auftrag)",
		7,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Globaler ST-Zustand# der BS",
			"#Detaillierter ST-Zustand# der BS",
			"#Opcode# des ACT-Auftrags",
			"#Einrichtungs-Typ# aus KON-Auftrag",
			"#Log. Einr.-Nr.# aus KON-Auftrag",
			"eigene Ident-Nr",
			"", "", "",
		},
	},

	{
		0x03dc,
		"Test- und Traceinformation beim Konfigurieren einer Einrichtung. (T101) (Alter ACT-Auftrag fuer FME wird noch bearbeitet-  neuer Auftrag wird beendet)",
		7,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Globaler ST-Zustand# des FME",
			"#Detaillierter ST-Zustand# des FME",
			"#Opcode# des aktuellen ACT-Auftrags",
			"#Einrichtungs-Typ# aus ACT-Auftrag",
			"#Log. Einr.-Nr.# aus ACT-Auftrag",
			"eigene Ident-Nr",
			"", "", "",
		},
	},

	{
		0x03dd,
		"Datenverfaelschung beim Konfigurieren eines FME nach ACT. (T086) (Unzulaessiger Zustandsuebergang)",
		7,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Globaler ST-Zustand# des FME",
			"#Detaillierter ST-Zustand# des FME",
			"#Opcode# des ACT-Auftrags",
			"#Einrichtungs-Typ# aus ACT-Auftrag",
			"#Log. Einr.-Nr.# aus ACT-Auftrag",
			"eigene Ident-Nr",
			"", "", "",
		},
	},

	{
		0x03de,
		"Datenverfaelschung beim Anstoss eines Konfigurations- auftrags fuer einen als OGK arbeitenden OSK. (T097) (korrespondierender OSK(SPK) befindet sich im ST-Zustand PLA,  was nicht vorkommen darf)",
		7,
		{
			"Phys. Einr.-Nr. des OGK  (LODEAD)",
			"#Globaler ST-Zustand# des OGK",
			"#Detaillierter ST-Zustand# des OGK",
			"#Opcode# des ACT-Auftrags",
			"#Einrichtungs-Typ# aus ACT-Auftrag",
			"#Log. Einr.-Nr.# aus ACT-Auftrag",
			"eigene Ident-Nr",
			"", "", "",
		},
	},

	{
		0x03df,
		"Datenverfaelschung beim Anstoss eines Konfigurations- auftrags fuer einen als OGK arbeitenden OSK. (T097) (korrespondierender OSK(SPK) befindet sich in einem  undefinierten Zustand bei Erstanlauf OGK)",
		8,
		{
			"Phys. Einr.-Nr. (LODEAD)",
			"#Globaler ST-Zustand# des OGK",
			"#Detaillierter ST-Zustand# des OGK",
			"#Opcode# des ACT-Auftrags",
			"#Einrichtungs-Typ# aus ACT-Auftrag",
			"#Log. Einr.-Nr.# aus ACT-Auftrag",
			"eigene Ident-Nr",
			"#Globaler ST-Zustand# des SPK1",
			"", "",
		},
	},

	{
		0x03e0,
		"Test- und Traceinformation beim Konfigurieren einer Einrichtung. (T101) (Neuer ACT-Auftrag wird mit Negativ-Quittung (KYKARB)  quittiert, falls gefordert, da alter ACT-Auftrag im  Wiederanlauf bereits besteht)",
		7,
		{
			"Phys. Einr.-Nr. (LODEAD)",
			"#Globaler ST-Zustand# der zu konfigurierenden Einrichtung",
			"#Detaillierter ST-Zustand# der zu konfigurierenden Einrichtung",
			"#Opcode# des ACT-Auftrags",
			"#Einrichtungs-Typ# der zu konfigurierenden Einrichtung",
			"#Log. Einr.-Nr.# der zu konfigurierenden Einrichtung",
			"eigene Ident-Nr",
			"", "", "",
		},
	},

	{
		0x03e2,
		"Test- und Traceinformation beim Konfigurieren einer Einrichtung. (T101) (Alter ACT-Auftrag besteht fuer PFG-  neuer Auftrag wird abgelehnt)",
		7,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Globaler ST-Zustand# des zu konfigurierenden PFG",
			"#Detaillierter ST-Zustand# des zu konfigurierenden PFG",
			"#Opcode# aus aktuellem ACT-Auftrag",
			"#Einrichtungs-Typ# des zu konfigurierenden PFG",
			"#Log. Einr.-Nr.# des zu konfigurierenden PFG",
			"eigene Ident-Nr",
			"", "", "",
		},
	},

	{
		0x03e3,
		"Datenverfaelschung beim Anstoss eines Konfigurations- auftrags fuer das PFG. (T091) (Unzulaessiger Zustandsuebergang bei der Behandlung eines ACT-Auftrags)",
		7,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Globaler ST-Zustand# des zu konfigurierenden PFG",
			"#Detaillierter ST-Zustand# des zu konfigurierenden PFG",
			"#Opcode# des ACT-Auftrags",
			"#Einrichtungs-Typ# des zu konfigurierenden PFG",
			"#Log. Einr.-Nr.# des zu konfigurierenden PFG",
			"eigene Ident-Nr",
			"", "", "",
		},
	},

	{
		0x03e4,
		"Datenverfaelschung beim Anstoss eines Konfigurations- auftrags fuer einen SPK. (T096) (Der Zaehler fuer SPK's im ST-Zustand MBL/PLA befindet  sich in einem undefinierten Zustand, d. h. der Wert  ist groesser als die Maximalzahl SPK)",
		7,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Globaler ST-Zustand# des zu konfigurierenden SPK",
			"#Detaillierter ST-Zustand# des zu konfigurierenden SPK",
			"#Opcode# des ACT-Auftrags",
			"#Einrichtungs-Typ# des zu konfigurierenden SPK",
			"#Log. Einr.-Nr.# des zu konfigurierenden SPK",
			"eigene Ident-Nr",
			"", "", "",
		},
	},

	{
		0x03e5,
		"Test- und Traceinformation beim Konfigurieren einer Einrichtung. (T101) (Alter ACT-Auftrag wird noch bearbeitet -  neuer ACT-Auftrag wird abgelehnt (Wiederanlauffall))",
		5,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Einrichtungs-Typ# des neuen ACT-Auftrags",
			"#Log. Einr.-Nr.# des neuen ACT-Auftrags",
			"eigene Ident-Nr",
			"Ident-Nr. des Auftraggebers",
			"", "", "", "", "",
		},
	},

	{
		0x03e6,
		"Datenverfaelschung. (T003) ( Signalisierung YKSAI (Konfigurations-Auftrag MBL) von nicht  erwarteter Einrichtung)",
		4,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Einrichtungs-Typ#",
			"#Log. Einr.-Nr.#",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x03e9,
		"Datenverfaelschung in der DKV. (T001) (Prozessanstoss mit unzulaessigem Ereignis-Typ)",
		4,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Ereignis-Typ#",
			"eigene Ident-Nr",
			"#Opcode# aus der Signalisierung",
			"", "", "", "", "", "",
		},
	},

	{
		0x03ea,
		"Datenverfaelschung in der DKV. (T001) (Prozessanstoss mit unzulaessigem Opcode )",
		5,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Ereignis-Typ#",
			"Ident-Nr. des Absenders",
			"#Opcode#",
			"eigene Ident-Nr",
			"", "", "", "", "",
		},
	},

	{
		0x03ec,
		"Datenverfaelschung in der DKV. (T001) ( unzulaessiger alter KON-Auftrag )",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Opcode# (alter Auftrag, /= ACT )",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x03ed,
		"Durch Konfigurationsaenderung vom PBR/MSC wurde der letzte aktive SPK oder OSK in der Funktion des SPK abgeschaltet. (T037) ( MBL-Auftrag fuer letzten aktiven SPK   fuehrt zum VTB-Verlust )",
		2,
		{
			"Phys. Einr.-Nr. der FDS",
			"Phys. Einr.-Nr. des SPK",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x03ee,
		"Datenverfaelschung in der DKV. (T001) ( Zaehler FYKAPM verfaelscht, groesser max. Anzahl SPK )",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"Zaehler der SPK im PLA/MBL (FYKAPM) (Istwert)",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x03f1,
		"Nicht behebbare Datenverfaelschung, durch Audits erkannt. (T054) (Fehler in Checksum-Pruefung der ST-AL)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"Abgespeicherte Checksum (Istwert)",
			"Ermittelte Checksum  (Sollwert)",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x03f2,
		"Nicht behebbare Datenverfaelschung, durch Audits erkannt. (T054) (Fehler in Checksum-Pruefung der Tabelle der SK-Einrichtungs-  sperren)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"Abgespeicherte Checksum (Istwert)",
			"Ermittelte Checksum  (Sollwert)",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x03f3,
		"Nicht behebbare Datenverfaelschung, durch Audits erkannt. (T054) (Checksum-Fehler in der Anlaufbegrenzungs-WS)",
		4,
		{
			"Phys. Einr.-Nr. der FDS",
			"Ermittelte Checksum  (Sollwert)",
			"Abgespeicherte Checksum (Istwert)",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x03f4,
		"Datenverfaelschung in der DKV. (T001) (Unzulaessiger Return-Code vom OS-UP WTAKOM bei Puffer-  anforderung fuer DKV-interne Signalisierung YKOAI)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code von WTAKOM#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x03f5,
		"Behebbare bzw. ignorierbare Datenverfaelschung, durch Audits erkannt. (T053) (Dateninkonsistenz in Anlaufbegr.-WS: es laeuft kein KON-Prozess  (Ident-Nummer = 0),obwohl Einrichtung auf ihren Anlauf wartet  ACT-Auftrag muss gestellt werden.)",
		7,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Einrichtungs-Typ# der Einrichtung, fuer die der ACT-Auftrag gestellt wird",
			"Einrichtungs-Nr. der Einrichtung, fuer die der ACT-Auftrag gestellt wird",
			"Ident-Nr. aktueller Prozess aus Anlaufbegr.-WS",
			"Phys. Einr.-Nr. aktueller Prozess aus Anlaufbegr.-WS",
			"Ident-Nr. Vorgaengerprozess aus Anlaufbegr.-WS",
			"Phys. Einr.-Nr. Vorgaengerprozess aus Anlaufbegr.-WS",
			"", "", "",
		},
	},

	{
		0x03f6,
		"Behebbare bzw. ignorierbare Datenverfaelschung, durch Audits erkannt. (T053) (Dateninkonsistenz in Anlaufbegr.-WS:  Eine KON-Prozess-Ident-Nr. ist eingetragen, aber keine  zugehoerige Einrichtung als im Anlauf befindlich gekenn-  zeichnet; moeglicherweise Fehler in Prozedur YPKPPE)",
		5,
		{
			"Phys. Einr.-Nr. der FDS",
			"Ident-Nr. aktuelller Prozess (aus Anlaufbegr.-WS)",
			"Phys. Einr.-Nr. aktueller Prozess (aus Anlaufbegr.-WS)",
			"Ident-Nr. Vorgaengerprozess (aus Anlaufbegr.-WS)",
			"Phys. Einr.-Nr. Vorgaengerprozess (aus Anlaufbegr.-WS)",
			"", "", "", "", "",
		},
	},

	{
		0x03f7,
		"Datenverfaelschung in der DKV. (T001) (Unzulaessiger Return-Code vom OS-UP WTAKOM bei Puffer-  anforderung fuer DKV-interne Signalisierung YKOAF)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code von WTAKOM#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x03f8,
		"Behebbare bzw. ignorierbare Datenverfaelschung, durch Audits erkannt. (T053) (Dateninkonsistenz in Anlaufbegr.-WS: Der in der Anlaufbegr.-WS  definierte KON-Prozess existiert nicht; es wird ein neuer  ACT-Auftrag gestellt)",
		7,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Einrichtungs-Typ# der Einrichtung, fuer die der ACT-Auftrag gestellt wird",
			"Einrichtungs-Nr. der Einrichtung, fuer die der ACT-Auftrag gestellt wird",
			"Ident-Nr. aktueller Prozess aus Anlaufbegr.-WS",
			"Phys. Einr.-Nr. aktueller Prozess aus Anlaufbegr.-WS",
			"Ident-Nr. Vorgaengerprozess aus Anlaufbegr.-WS",
			"Phys. Einr.-Nr. Vorgaengerprozess aus Anlaufbegr.-WS",
			"", "", "",
		},
	},

	{
		0x03f9,
		"Behebbare Datenverfaelschung in der DKV. (T048) (Im Zustand UNA/PLA/MBL war der Port einer peripheren  Einrichtung offen; der Port wird geschlossen)",
		5,
		{
			"Phys. Einr.-Nr. der FDS",
			"Phys. Einr.-Nr. der peripheren Einrichtung",
			"#Einrichtungs-Typ#",
			"#Log. Einr.-Nr.#",
			"#Globaler ST-Zustand#",
			"", "", "", "", "",
		},
	},

	{
		0x03fa,
		"Behebbare Datenverfaelschung in der DKV. (T048) (Im Zustand ACT/STB ist der Port einer peripheren Einrichtung  geschlossen. Es wird ein ACT-Auftrag fuer die Einrichtung  gestellt)",
		5,
		{
			"Phys. Einr.-Nr. der FDS",
			"Phys. Einr.-Nr. der peripheren Einrichtung",
			"#Einrichtungs-Typ#",
			"#Log. Einr.-Nr.#",
			"#Globaler ST-Zustand#",
			"", "", "", "", "",
		},
	},

	{
		0x03fb,
		"Datenverfaelschung in der DKV. (T001) (Globaler ST-Zustand unplausibel)",
		5,
		{
			"Phys. Einr.-Nr. der FDS",
			"Phys. Einr.-Nr. der peripheren Einrichtung",
			"#Einrichtungs-Typ#",
			"#Log. Einr.-Nr.#",
			"#Globaler ST-Zustand#",
			"", "", "", "", "",
		},
	},

	{
		0x0402,
		"Datenverfaelschung in der DKV. (T001) (Prozessanstoss mit unzulaessigem Opcode )",
		5,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Ereignis-Typ#",
			"Ident-Nr. des Absenders",
			"#Opcode#",
			"eigene Ident-Nr",
			"", "", "", "", "",
		},
	},

	{
		0x0403,
		"Datenverfaelschung. (T003) ( Signalisierung von nicht erwarteter Einrichtung )",
		4,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Einrichtungs-Typ#",
			"#Log. Einr.-Nr.#",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x0404,
		"Datenverfaelschung in der DKV. (T001) ( nicht erwartete globale ST-Zustaende,   ST-Zustand (Einrichtung) /= MBL/UNA/PLA",
		6,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Einrichtungs-Typ#",
			"#Log. Einr.-Nr.#",
			"#Globaler ST-Zustand# der Einrichtung",
			"#Auftrags-Typ#",
			"eigene Ident-Nr",
			"", "", "", "",
		},
	},

	{
		0x040b,
		"Datenverfaelschung in der DKV. (T001) (Prozessanstoss mit unzulaessigem Opcode )",
		5,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Ereignis-Typ#",
			"Ident-Nr. des Absenders",
			"#Opcode#",
			"eigene Ident-Nr",
			"", "", "", "", "",
		},
	},

	{
		0x0422,
		"Datenverfaelschung. (T003) (Prozessanstoss mit unzulaessigem Ereignis)",
		4,
		{
			"Phys. Einr.-Nr. der FDS",
			"eigene Ident-Nr",
			"#Ereignis-Typ#",
			"#Opcode#",
			"", "", "", "", "", "",
		},
	},

	{
		0x0423,
		"Datenverfaelschung in der DKV. (T001) (Unzulaessiger Return-Code des MUK-UP YPNZ03)",
		2,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code MUK#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x0424,
		"Datenverfaelschung in der DKV. (T001) (unzulaessiger Return-Code des MUK-UP YPNAA4)",
		2,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code MUK#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x0426,
		"Datenverfaelschung in der DKV. (T001) (Unzulaessiger Return-Code vom MUK-UP YPNUA4)",
		2,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code MUK#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x0427,
		"Datenverfaelschung in der DKV. (T001) (Unzulaessiger Return-Code vom MUK-UP YPNUA3)",
		2,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code MUK#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x0428,
		"Datenverfaelschung in der DKV. (T001) (Unzulaessiger Return-Code vom MUK-UP YPNUZ1)",
		2,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code MUK#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x0429,
		"Datenverfaelschung in der DKV. (T001) (Unzulaessiger Return-Code vom MUK-UP YPNZ01)",
		2,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code MUK#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x0430,
		"Der angegebene SPK antwortet nicht beim Leitungsmessen bzw. beim Continuity Check. (T073) (Spiegel-Ausschalte-Auftrag wird vom SPK nicht mit der Signalisierung PVEQS quittiert. <Nach Ausloesen durch MSC mit der Signalisierung PVEAU. SPK-Zustand ist ACT>)",
		1,
		{
			"Phys. Einr.-Nr. des SPK",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x0431,
		"Der angegebene SPK antwortet nicht beim Leitungsmessen bzw. beim Continuity Check. (T073) (Spiegel-Ausschalte-Auftrag wird vom SPK nicht mit Signalisierung PVEQS quittiert. <Nach internem Ausloesen mit Signalisierung YVAAI. SPK-Zustand ist ACT>)",
		1,
		{
			"Phys. Einr.-Nr. des SPK",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x0432,
		"Datenverfaelschung beim Continuity Check bzw. beim Leitungsmessen. (T074) (Unzulaessiger Prozesszustand, Ereignis-Typ ist MREADY oder MZTO)",
		4,
		{
			"Phys. Einr.-Nr. der FDS",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"#Ereignis-Typ#",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x0433,
		"Datenverfaelschung beim Continuity Check bzw. beim Leitungsmessen. (T074) (Unzulaessiger Prozesszustand, Ereignis-Typ ist weder MREADY noch MZTO)",
		6,
		{
			"Phys. Einr.-Nr. der FDS",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"#Ereignis-Typ#",
			"#Opcode#",
			"Ident-Nr. des eintragenden Prozesses (Absender)",
			"eigene Ident-Nr",
			"", "", "", "",
		},
	},

	{
		0x0434,
		"Datenverfaelschung in der DKV. (T001) (Unzulaessiger Anstoss des SCC-Prozesses. <Falscher Ereignis-Typ, Ereignis-Typ ist weder MREADY noch MZTO>)",
		4,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Ereignis-Typ#",
			"#Opcode#",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x0435,
		"Datenverfaelschung in der DKV. (T001) (Unerwarteter Anstoss des SCC-Prozesses mit MZTO. <Ereignis-Typ MREADY von VT ging verloren>)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Ereignis-Typ#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0436,
		"Datenverfaelschung in der DKV. (T001) (Unzulaessiger Anstoss des SCC-Prozesses. <Signalisierung kam nicht vom MSC. Ereignis-Typ ist nicht MUELE>)",
		4,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Ereignis-Typ#",
			"#Opcode#",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x0437,
		"Datenverfaelschung in der DKV. (T001) (Unzulaessiger Anstoss des SCC-Prozesses. <Falscher Opcode, nur Opcode OPVEAU erlaubt. Ereignis-Typ ist MUELE>)",
		4,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Ereignis-Typ#",
			"#Opcode#",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x0438,
		"Datenverfaelschung in der DKV. (T001) (Unzulaessiger Anstoss des SCC-Prozesses. <Signalisierung kam nicht vom MSC. Ereignis-Typ ist nicht MUELE>)",
		4,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Ereignis-Typ#",
			"#Opcode#",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x0439,
		"Datenverfaelschung in der DKV. (T001) (Unzulaessiger Anstoss des SCC-Prozesses. <Falscher Opcode, nur Opcode OPVRAU erlaubt. Ereignis-Typ ist MUELE>)",
		4,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Ereignis-Typ#",
			"#Opcode#",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x043a,
		"Der Prozesstart erfolgte nicht ueber den erwarteten Anreiztyp <READY>, sondern durch einen anderen Anreiztyp <TIME-OUT>",
		8,
		{
			"Physikalische Einrichtungs-Nummer der FDS",
			"Frei",
			"#Ident-Nummer#",
			"Nummer des vom Verbindungsabbruch betroffenen Teilnehmers .low  Byte",
			"Nummer des vom Verbindungsabbruch betroffenen Teilnehmers .high Byte",
			"Nationalitaet und MSC - Nummer des betroffenen Teilnehmers",
			"Prozess-Adresse      high Byte",
			"Prozess-Adresse      low  Byte",
			"", "",
		},
	},

	{
		0x043b,
		"Endgueltige Einbuchung eines Teilnehmers wurde nicht durchgefuert, da die lokalen Daten des Prozesses nicht mit der Aktivdatei uebereinstimmen.",
		8,
		{
			"Physikalische Einrichtungs-Nummer der FDS",
			"Frei",
			"#Ident-Nummer#",
			"Nummer des vom Buchungsabbruch betroffenen Teilnehmers .low  Byte",
			"Nummer des vom Buchungsabbruch betroffenen Teilnehmers .high Byte",
			"Nationalitaet und MSC - Nummer des betroffenen Teilnehmers",
			"Prozess-Adresse      high Byte",
			"Prozess-Adresse      low  Byte",
			"", "",
		},
	},

	{
		0x043c,
		"Ein schreibender Zugriff auf die Aktivdatei wird nicht durchgefuehrt, da die lokalen Daten des Prozesses nicht mit der Aktivdatei uebereinstimmen",
		8,
		{
			"Physikalische Einrichtungs-Nummer der FDS",
			"Frei",
			"#Ident-Nummer#",
			"Nummer des vom Verbindungsabbruch betroffenen Teilnehmers .low  Byte",
			"Nummer des vom Verbindungsabbruch betroffenen Teilnehmers .high Byte",
			"Nationalitaet und MSC - Nummer des betroffenen Teilnehmers",
			"Prozess-Adresse      high Byte",
			"Prozess-Adresse      low  Byte",
			"", "",
		},
	},

	{
		0x043d,
		"Die Anzahl der Teilnehmer in der ACTFDS entspricht nicht der von der frueheren ACTFDS gemeldetet Anzahl eingebuchter Teilnehmer",
		3,
		{
			"Physikalische Einrichtungs-Nummer der FDS",
			"Teilnehmerstand     low  Byte",
			"Teilnehmerstand     high Byte",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x043e,
		"Das Meldeintervall einer OGK-Frequenz hat laenger als 6,5 Minuten gedauert",
		4,
		{
			"Physikalische Einrichtungs-Nummer der FDS",
			"Verkuerzte OGK-Frequenznummer",
			"Meldeintervalldauer (in 2,4 Sekunden-Einheiten) . low  Byte",
			"Meldeintervalldauer high Byte",
			"", "", "", "", "", "",
		},
	},

	{
		0x043f,
		"In der STBFDS musste vor der Aktualisierung der Aktiv- datei wegen Einbuchung zuerst ein Teilnehmer ausgebucht werden, der auf dem vorgesehenen Aktivdatei-Platz stand.",
		4,
		{
			"Physikalische Einrichtungs-Nummer der FDS",
			"Nummer des ausgebuchten Teilnehmers low  Byte",
			"Nummer des ausgebuchten Teilnehmers high Byte",
			"Nationalitaet und MSC-Nummer des ausgebuchten Teilnehmers",
			"", "", "", "", "", "",
		},
	},

	{
		0x0440,
		"Bei der Generierung eines Meldeaufrufs fuer einen Teilnehmer  wurde ein Fehler im #Zusatzdaten-Byte# der Aktivdatei festgestellt. Das Zusatzdaten-Byte wurde auf gespraechsfrei und endgueltig eingebucht korrigiert.",
		9,
		{
			"Physikalische Einrichtungs-Nummer der FDS",
			"Nummer des betroffenen Teilnehmers low  Byte",
			"Nummer des betroffenen Teilnehmers high Byte",
			"Nationalitaet und MSC-Nummer des betroffenen Teilnehmers",
			"#Zusatzdaten-Byte#",
			"#Verwaltungs-Byte#",
			"#Identnummer-Byte#",
			"AD-Index low Byte",
			"AD-Index high Byte",
			"",
		},
	},

	{
		0x0441,
		"In der STBFDS wurde bei der Aktualisierung der Aktivdatei in der STBFDS wegen Ausbuchung ein anderer Teilnehmer, als von der ACTFDS angegeben, ausgebucht.",
		4,
		{
			"Physikalische Einrichtungs-Nummer der FDS",
			"Nummer des ausgebuchten Teilnehmers low  Byte",
			"Nummer des ausgebuchten Teilnehmers high Byte",
			"Nationalitaet und MSC-Nummer das betroffene Teilnehmers",
			"", "", "", "", "", "",
		},
	},

	{
		0x0442,
		"Der Auftrag zur Aktualisierung der Aktivdatei in der STBFDS wegen Ausbuchung wurde nicht ausgefuehrt, da auf dem angegebenen Aktivdatei-Platz kein Teilnehmer eintragen war.",
		7,
		{
			"Physikalische Einrichtungs-Nummer der FDS",
			"Nummer des ausgebuchten Teilnehmers low Byte",
			"Nummer des ausgebuchten Teilnehmers high Byte",
			"Nationalitaet und MSC-Nummer des betrofffenen Teilnehmers",
			"AD-Index low Byte",
			"AD-Index high Byte",
			"#Zusatzdaten-Byte#",
			"", "", "",
		},
	},

	{
		0x0443,
		"Beziehungsausfall zum MSC; auf mehrfachen Versuch keine Antwort vom MSC mehr oder Ausfall des ZZK. (T035) ( BS-Anlauf wegen nicht zustande gekommener Verbindung zum MSC )",
		1,
		{
			"Phys. Einr.-Nr. der FDS",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x0445,
		"Die STB-FDS lief zu oft an und wurde deshalb nach UNA konfiguriert. (Die Anlaufstatistik fuer die STB-FDS hat ihren Schwellwert erreicht).",
		1,
		{
			"Physikalische Einrichtungs-Nummer der STB-FDS",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x044a,
		"Datenverfaelschung im Anlauf mit der im BYTE 1 angegebenen Einrichtung. (T041) (negativer Return-Code vom OS-UP WMESEP beim Absenden einer Signalisierung zwischen ACTFDS und STBFDS)",
		4,
		{
			"Phys. Einr.-Nr. der FDS",
			"Phys. Einr.-Nr. (LODEAD)",
			"unzulaessiger #Return-Code von WMESEP#",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x044b,
		"Datenverfaelschung im Anlauf mit der im BYTE 1 angegebenen Einrichtung. (T041) (Prozessanstoss mit einer nicht erwarteten Signalisierung von der STBFDS)",
		6,
		{
			"Phys. Einr.-Nr. der FDS",
			"Phys. Einr.-Nr. (LODEAD)",
			"#Ereignis-Typ#",
			"Ident-Nr. des Absenders aus Signalisierung",
			"#Opcode#",
			"eigene Ident-Nr",
			"", "", "", "",
		},
	},

	{
		0x044c,
		"Datenverfaelschung im Anlauf der passiven FDS. (T056) (unzulaessiger Return-Code vom OS-UP WTAKOM bei Pufferanforderung fuer die DKV-interne Signalisierung YLANG)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code von WTAKOM#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x044d,
		"Datenuebertragung von der passiven in die aktive FDS; im Anlauf der passiven FDS  nicht  moeglich. (T058) (Fehler beim Absetzen der DMA-Signalisierung YAAD  durch das OS-UP WMESEP von der STBFDS zur ACTFDS)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code von WMESEP#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x044e,
		"Datenverfaelschung im Anlauf der passiven FDS. (T056) (Prozessanstoss mit unzulaessigem Ereignis-Typ)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Ereignis-Typ#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x044f,
		"Datenverfaelschung im Anlauf der passiven FDS. (T056) (Prozessanstoss mit falschem Opcode)",
		4,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Ereignis-Typ#",
			"#Opcode#",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x0450,
		"Datenuebertragung von der aktiven in die passive FDS im Anlauf der passiven FDS nicht moeglich. (T057) (DMA-Uebertragung durch OS-UP WMSDUE nicht moeglich)",
		2,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code von WMSDUE#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x0451,
		"Datenverfaelschung im Anlauf der passiven FDS. (T056) (Prozessanstoss mit unzulaessigem Ereignis-Typ)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Ereignis-Typ#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0452,
		"Datenverfaelschung bei der Datenuebertragung von der aktiven in die passive FDS im Anlauf der passiven FDS. (T059) (Uebertragungs-Ende-Kennzeichen (IYAUEV,Low-Byte)  der Anlagenliste ist fehlerhaft )",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Uebertragungs-Ende-Kz.# (IYAUEV) (Low Byte)",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0453,
		"Datenverfaelschung bei der Datenuebertragung von der aktiven in die passive FDS im Anlauf der passiven FDS. (T059) (Uebertragungs-Ende-Kennzeichen (IYAUEV,High-Byte)  der Anlagenliste ist fehlerhaft )",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Uebertragungs-Ende-Kz.# (IYAUEV) (High-Byte)",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0454,
		"Datenverfaelschung im Anlauf der passiven FDS. (T056)  (Checksum-Pruefungen der Daten-AUD sind fehlerhaft oder  unzulaessiger Return-Code aus UP YPAKCS kommt zurueck)",
		4,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code ANL# : UP YPAKCS",
			"#Return-Code von CS-Pruefpr.#",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x0455,
		"Datenuebertragung von der passiven in die aktive FDS; im Anlauf der passiven FDS  nicht  moeglich. (T058)  (Fehler beim Absetzen der DMA-Signalisierung YADQD  durch das OS-UP WMESEP von der STBFDS zur ACTFDS)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code von WMESEP#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0457,
		"Datenverfaelschung im Anlauf der passiven FDS. (T056) (Unzulaessiger Return-Code aus UP YPAKOP wegen Fehler bei Pufferanforderung durch das OS-UP WTAKOM fuer DKV-interne Signalisierung YIAAI bzw. YHAAI oder Return-Code wurde verfaelscht)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code ANL#  (UP YPAKOP)",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0458,
		"Aktive FDS wurde  nicht  verfuegbar im Anlauf der passiven FDS. (T060) (Die ACTFDS wurde nicht innerhalb der angegebenen Zeit verfuegbar)",
		4,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Ereignis-Typ#",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x0459,
		"Datenuebertragung von der aktiven in die passive FDS im Anlauf der passiven FDS nicht moeglich. (T057) (Quittung YAQD von der ACTFDS kam nicht innerhalb der vorgegebenen Zeit)",
		4,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Ereignis-Typ#",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x0465,
		"Anlauf aufgrund einer Routinepruefung. (T112) (Anlauf-Auftrag fuer OGK oder SPK wegen OSK-Umschaltepruefung)",
		5,
		{
			"#Phys. Einr.-Nr.# der FDS",
			"#Einrichtungs-Typ#",
			"#Log. Einr.-Nr.# des OGK",
			"#Phys. Einr.-Nr.# des OGK",
			"#Ident-Nummer# des eigenen Prozesses",
			"", "", "", "", "",
		},
	},

	{
		0x0466,
		"Datenverfaelschung beim Anstoss eines Konfigurations- auftrags. (T093) (Negativer Return-Code vom OS-UP WTAKOM bei Pufferanforderung fuer die DKV-interne Signalisierung ZAAI)",
		5,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Einrichtungs-Typ# der zu konfigurierenden Einrichtung",
			"#Log. Einr.-Nr.# der zu konfigurierenden Einrichtung",
			"eigene Ident-Nr",
			"Ident-Nr. des Auftraggebers des Konfigurations- auftrags",
			"", "", "", "", "",
		},
	},

	{
		0x0467,
		"Datenverfaelschung beim Konfigurieren eines SAE nach ACT. (T087) (Unzulaessiger Zustandsuebergang)",
		7,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Globaler ST-Zustand# des SAE",
			"#Detaillierter ST-Zustand# des SAE",
			"#Opcode# des ACT-Auftrags",
			"#Einrichtungs-Typ# des SAE",
			"#Log. Einr.-Nr.# des SAE",
			"eigene Ident-Nr",
			"", "", "",
		},
	},

	{
		0x0468,
		"Datenverfaelschung beim Konfigurieren eines SAE nach ACT. (T087) (Unzulaessiger Return-Code aus Unterprogramm YPKCBZ)",
		8,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Globaler ST-Zustand# des SAE",
			"#Detaillierter ST-Zustand# des SAE",
			"#Opcode# des ACT-Auftrags",
			"#Einrichtungs-Typ# des SAE",
			"#Log. Einr.-Nr.# des SAE",
			"eigene Ident-Nr",
			"#Return-Code von YPKCBZ#",
			"", "",
		},
	},

	{
		0x0469,
		"Datenverfaelschung beim Konfigurieren des PBR nach ACT. (T085) (Prozessanstoss mit unzulaessigem Ereignis-Typ)",
		3,
		{
			"Phys. Einr.-Nr. (LODEAD)",
			"#Ereignis-Typ#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x046a,
		"Datenverfaelschung beim Anstoss eines Konfigurations- auftrags fuer die FDS. (T089) (Unzulaessiger Return-Code aus KON-UP YPKCBZ)",
		8,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Globaler ST-Zustand# der STBFDS",
			"#Detaillierter ST-Zustand# der STBFDS",
			"#Opcode# des ACT-Auftrags",
			"#Einrichtungs-Typ# der STBFDS",
			"#Log. Einr.-Nr.# der STBFDS",
			"eigene Ident-Nr",
			"#Return-Code von YPKCBZ#",
			"", "",
		},
	},

	{
		0x046b,
		"Datenverfaelschung beim Anstoss eines Konfigurations- auftrags fuer einen PHE. (T092) (Das KON-UP YPKCBZ liefert falschen Return-Code)",
		8,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Globaler ST-Zustand# des zu konfigurierenden PHE",
			"#Detaillierter ST-Zustand# des zu konfigurierenden PHE",
			"#Opcode# des ACT-Auftrags",
			"#Einrichtungs-Typ# des zu konfigurierenden PHE",
			"#Log. Einr.-Nr.# des zu konfigurierenden PHE",
			"eigene Ident-Nr",
			"#Return-Code von YPKCBZ#",
			"", "",
		},
	},

	{
		0x046c,
		"Datenverfaelschung beim Anstoss eines Konfigurations- auftrags fuer die BS. (T090) (Unzulaessiger Return-Code aus KON-UP YPKCBZ)",
		8,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Globaler ST-Zustand# der BS",
			"#Detaillierter ST-Zustand# der BS",
			"#Opcode# des ACT-Auftrags",
			"#Einrichtungs-Typ# der BS",
			"#Log. Einr.-Nr.# der BS",
			"eigene Ident-Nr",
			"#Return-Code von YPKCBZ#",
			"", "",
		},
	},

	{
		0x046e,
		"Datenverfaelschung beim Konfigurieren eines FME nach ACT. (T086) (Unzulaessiger Return-Code aus KON-UP YPKCBZ)",
		8,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Globaler ST-Zustand# des FME",
			"#Detaillierter ST-Zustand# des FME",
			"#Opcode# des ACT-Auftrags",
			"#Einrichtungs-Typ# des FME",
			"#Log. Einr.-Nr.# des FME",
			"eigene Ident-Nr",
			"#Return-Code von YPKCBZ#",
			"", "",
		},
	},

	{
		0x046f,
		"Datenverfaelschung beim Anstoss eines Konfigurations- auftrags fuer einen als OGK arbeitenden OSK. (T097) (Unzulaessiger Zustandsuebergang)",
		7,
		{
			"Phys. Einr.-Nr. (LODEAD)",
			"#Globaler ST-Zustand# des OGK",
			"#Detaillierter ST-Zustand# des OGK",
			"#Opcode# des Konfigurations-Auftrages",
			"#Einrichtungs-Typ# aus ACT-Auftrag",
			"#Log. Einr.-Nr.# aus ACT-Auftrag",
			"eigene Ident-Nr",
			"", "", "",
		},
	},

	{
		0x0473,
		"Datenverfaelschung beim Anstoss eines Konfigurations- auftrags fuer das PFG. (T091) (Unzulaessiger Return-Code aus Unterprogramm YPKCBZ bei der  Behandlung eines ACT-Auftrags.)",
		8,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Globaler ST-Zustand# des zu konfigurierenden PFG",
			"#Detaillierter ST-Zustand# des zu konfigurierenden PFG",
			"#Opcode# des ACT-Auftrags",
			"#Einrichtungs-Typ# des zu konfigurierenden PFG",
			"#Log. Einr.-Nr.# des zu konfigurierenden PFG",
			"eigene Ident-Nr",
			"#Return-Code von YPKCBZ#",
			"", "",
		},
	},

	{
		0x0474,
		"Datenverfaelschung beim Anstoss eines Konfigurations- auftrags fuer einen SPK. (T096) (Unzulaessiger Zustandsuebergang bei der Behandlung eines )  ACT-Auftrags)",
		5,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Einrichtungs-Typ# des zu konfigurierenden SPK",
			"#Log. Einr.-Nr.# des zu konfigurierenden SPK",
			"eigene Ident-Nr",
			"Ident-Nr. des Auftraggebers",
			"", "", "", "", "",
		},
	},

	{
		0x0475,
		"Datenverfaelschung beim Anstoss eines Konfigurations- auftrags fuer einen SPK. (T096) (Unzulaessiger Return-Code aus Unterprogramm YPKCBZ bei der  Behandlung eines ACT-Auftrags)",
		8,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Globaler ST-Zustand# des zu konfigurierenden SPK",
			"#Detaillierter ST-Zustand# des zu konfigurierenden SPK",
			"#Opcode# des ACT-Auftrags",
			"#Einrichtungs-Typ# des zu konfigurierenden SPK",
			"#Log. Einr.-Nr.# des zu konfigurierenden SPK",
			"eigene Ident-Nr",
			"#Return-Code von YPKCBZ#",
			"", "",
		},
	},

	{
		0x0476,
		"Datenverfaelschung beim Anlauf mit dem MSC. (T088) (Unzulaessiger Zustandsuebergang bei der Behandlung  eines ACT-Auftrags)",
		7,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Globaler ST-Zustand# des MSC",
			"#Detaillierter ST-Zustand# des MSC",
			"#Opcode# des ACT-Auftrags",
			"#Einrichtungs-Typ# aus ACT-Auftrag",
			"#Log. Einr.-Nr.# aus ACT-Auftrag",
			"eigene Ident-Nr",
			"", "", "",
		},
	},

	{
		0x0477,
		"Datenverfaelschung beim Anlauf mit dem MSC. (T088) (Unzulaessiger Return-Code aus KON-UP YPKCBZ bei der  Behandlung eines ACT-Auftrags)",
		8,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Globaler ST-Zustand# des MSC",
			"#Detaillierter ST-Zustand# des MSC",
			"#Opcode# des ACT-Auftrags",
			"#Einrichtungs-Typ# aus ACT-Auftrag",
			"#Log. Einr.-Nr.# aus ACT-Auftrag",
			"eigene Ident-Nr",
			"#Return-Code von YPKCBZ#",
			"", "",
		},
	},

	{
		0x0478,
		"Test- und Traceinformation beim Konfigurieren einer Einrichtung. (T101) (Alter ACT-Auftrag fuer MSC besteht-   alter ACT-Auftrag wird  beendet, neuer ACT-Auftrag wird weiterverarbeitet)",
		7,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Globaler ST-Zustand# des MSC",
			"#Detaillierter ST-Zustand# des MSC",
			"#Opcode# des neuen ACT-Auftrags",
			"#Einrichtungs-Typ# des MSC",
			"#Log. Einr.-Nr.# des MSC",
			"eigene Ident-Nr",
			"", "", "",
		},
	},

	{
		0x047e,
		"Der SAE hat einen Anlauf ausgefuehrt",
		3,
		{
			"Physikalische Einrichtungs-Nummer",
			"Neuer #Link-Zustand#",
			"Alter #Link-Zustand#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x047f,
		"Link-Zustands-Aenderung",
		3,
		{
			"Physikalische Einrichtungs-Nummer",
			"Neuer #Link-Zustand#",
			"Alter #Link-Zustand#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0480,
		"Link-Zustands-Aenderung",
		3,
		{
			"Physikalische Einrichtungs-Nummer",
			"Neuer #Link-Zustand#",
			"Alter #Link-Zustand#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0481,
		"Ein Meldezyklus-Ausgabepuffer fuer die Frequenz 4 wird beantragt, obwohl keiner mehr frei ist.",
		8,
		{
			"DKV-Nummer",
			"#Ident-Nummer#",
			"LOW-PC des letzten Prozesses",
			"HIGH-PC des letzten Prozesses",
			"Prozess-Status 1 des letzten Prozesses (LOW-TO)",
			"Prozess-Status 1 des letzten Prozesses (HIGH-TO)",
			"Prozess-Status 2 des letzten Prozesses",
			"Prozess-Status 3 des letzten Prozesses",
			"", "",
		},
	},

	{
		0x0482,
		"Ein Meldezyklus-Ausgabepuffer fuer die Frequenz 5 wird beantragt, obwohl keiner mehr frei ist.",
		8,
		{
			"DKV-Nummer",
			"#Ident-Nummer#",
			"LOW-PC des letzten Prozesses",
			"HIGH-PC des letzten Prozesses",
			"Prozess-Status 1 des letzten Prozesses (LOW-TO)",
			"Prozess-Status 1 des letzten Prozesses (HIGH-TO)",
			"Prozess-Status 2 des letzten Prozesses",
			"Prozess-Status 3 des letzten Prozesses",
			"", "",
		},
	},

	{
		0x0483,
		"Ein Meldezyklus-Ausgabepuffer fuer die Frequenz 6 wird beantragt, obwohl keiner mehr frei ist.",
		8,
		{
			"DKV-Nummer",
			"#Ident-Nummer#",
			"LOW-PC des letzten Prozesses",
			"HIGH-PC des letzten Prozesses",
			"Prozess-Status 1 des letzten Prozesses (LOW-TO)",
			"Prozess-Status 1 des letzten Prozesses (HIGH-TO)",
			"Prozess-Status 2 des letzten Prozesses",
			"Prozess-Status 3 des letzten Prozesses",
			"", "",
		},
	},

	{
		0x0484,
		"Ein Meldezyklus-Ausgabepuffer fuer die Frequenz 7 wird beantragt, obwohl keiner mehr frei ist.",
		8,
		{
			"DKV-Nummer",
			"#Ident-Nummer#",
			"LOW-PC des letzten Prozesses",
			"HIGH-PC des letzten Prozesses",
			"Prozess-Status 1 des letzten Prozesses (LOW-TO)",
			"Prozess-Status 1 des letzten Prozesses (HIGH-TO)",
			"Prozess-Status 2 des letzten Prozesses",
			"Prozess-Status 3 des letzten Prozesses",
			"", "",
		},
	},

	{
		0x0485,
		"Ein Meldezyklus-Ausgabepuffer fuer die Frequenz 8 wird beantragt, obwohl keiner mehr frei ist.",
		8,
		{
			"DKV-Nummer",
			"#Ident-Nummer#",
			"LOW-PC des letzten Prozesses",
			"HIGH-PC des letzten Prozesses",
			"Prozess-Status 1 des letzten Prozesses (LOW-TO)",
			"Prozess-Status 1 des letzten Prozesses (HIGH-TO)",
			"Prozess-Status 2 des letzten Prozesses",
			"Prozess-Status 3 des letzten Prozesses",
			"", "",
		},
	},

	{
		0x0486,
		"Ein Meldezyklus-Ausgabepuffer fuer die Frequenz 9 wird beantragt, obwohl keiner mehr frei ist.",
		8,
		{
			"DKV-Nummer",
			"#Ident-Nummer#",
			"LOW-PC des letzten Prozesses",
			"HIGH-PC des letzten Prozesses",
			"Prozess-Status 1 des letzten Prozesses (LOW-TO)",
			"Prozess-Status 1 des letzten Prozesses (HIGH-TO)",
			"Prozess-Status 2 des letzten Prozesses",
			"Prozess-Status 3 des letzten Prozesses",
			"", "",
		},
	},

	{
		0x0487,
		"Ein Meldezyklus-Ausgabepuffer fuer die Frequenz 10 wird beantragt, obwohl keiner mehr frei ist.",
		8,
		{
			"DKV-Nummer",
			"#Ident-Nummer#",
			"LOW-PC des letzten Prozesses",
			"HIGH-PC des letzten Prozesses",
			"Prozess-Status 1 des letzten Prozesses (LOW-TO)",
			"Prozess-Status 1 des letzten Prozesses (HIGH-TO)",
			"Prozess-Status 2 des letzten Prozesses",
			"Prozess-Status 3 des letzten Prozesses",
			"", "",
		},
	},

	{
		0x0488,
		"Ein Meldezyklus-Ausgabepuffer fuer die Frequenz 11 wird beantragt, obwohl keiner mehr frei ist.",
		8,
		{
			"DKV-Nummer",
			"#Ident-Nummer#",
			"LOW-PC des letzten Prozesses",
			"HIGH-PC des letzten Prozesses",
			"Prozess-Status 1 des letzten Prozesses (LOW-TO)",
			"Prozess-Status 1 des letzten Prozesses (HIGH-TO)",
			"Prozess-Status 2 des letzten Prozesses",
			"Prozess-Status 3 des letzten Prozesses",
			"", "",
		},
	},

	{
		0x0489,
		"Ein Meldezyklus-Ausgabepuffer fuer die Frequenz 12 wird beantragt, obwohl keiner mehr frei ist.",
		8,
		{
			"DKV-Nummer",
			"#Ident-Nummer#",
			"LOW-PC des letzten Prozesses",
			"HIGH-PC des letzten Prozesses",
			"Prozess-Status 1 des letzten Prozesses (LOW-TO)",
			"Prozess-Status 1 des letzten Prozesses (HIGH-TO)",
			"Prozess-Status 2 des letzten Prozesses",
			"Prozess-Status 3 des letzten Prozesses",
			"", "",
		},
	},

	{
		0x048a,
		"Ein Meldezyklus-Ausgabepuffer fuer die Frequenz 13 wird beantragt, obwohl keiner mehr frei ist.",
		8,
		{
			"DKV-Nummer",
			"#Ident-Nummer#",
			"LOW-PC des letzten Prozesses",
			"HIGH-PC des letzten Prozesses",
			"Prozess-Status 1 des letzten Prozesses (LOW-TO)",
			"Prozess-Status 1 des letzten Prozesses (HIGH-TO)",
			"Prozess-Status 2 des letzten Prozesses",
			"Prozess-Status 3 des letzten Prozesses",
			"", "",
		},
	},

	{
		0x048b,
		"Ein Meldezyklus-Ausgabepuffer fuer die Frequenz 14 wird beantragt, obwohl keiner mehr frei ist.",
		8,
		{
			"DKV-Nummer",
			"#Ident-Nummer#",
			"LOW-PC des letzten Prozesses",
			"HIGH-PC des letzten Prozesses",
			"Prozess-Status 1 des letzten Prozesses (LOW-TO)",
			"Prozess-Status 1 des letzten Prozesses (HIGH-TO)",
			"Prozess-Status 2 des letzten Prozesses",
			"Prozess-Status 3 des letzten Prozesses",
			"", "",
		},
	},

	{
		0x048c,
		"Ein Meldezyklus-Ausgabepuffer fuer die Frequenz 15 wird beantragt, obwohl keiner mehr frei ist.",
		8,
		{
			"DKV-Nummer",
			"#Ident-Nummer#",
			"LOW-PC des letzten Prozesses",
			"HIGH-PC des letzten Prozesses",
			"Prozess-Status 1 des letzten Prozesses (LOW-TO)",
			"Prozess-Status 1 des letzten Prozesses (HIGH-TO)",
			"Prozess-Status 2 des letzten Prozesses",
			"Prozess-Status 3 des letzten Prozesses",
			"", "",
		},
	},

	{
		0x048d,
		"In der Block-Bereitstellungs-Meldung stimmt die Phys. Einr.-Nr. des OgK nicht mit der Phys. Einr.-Nr. eines der drei aktiven OgK ueberein.",
		5,
		{
			"DKV-Nummer",
			"#Phys. Einr.-Nr.# in der Block-Bereitstellungs- Meldung",
			"#Phys. Einr.-Nr.# des OgK 1",
			"#Phys. Einr.-Nr.# des OgK 2",
			"#Phys. Einr.-Nr.# des OgK 3",
			"", "", "", "", "",
		},
	},

	{
		0x048e,
		"Phys. Einr.-Nr. der Einrichtung beim Freigeben der Einrichtung ausserhalb des Wertebereiches.",
		9,
		{
			"DKV-Nummer",
			"#Ident-Nummer#",
			"LOW-PC des letzten Prozesses",
			"HIGH-PC des letzten Prozesses",
			"Prozess-Status 1 des letzten Prozesses (LOW-TO)",
			"Prozess-Status 1 des letzten Prozesses (HIGH-TO)",
			"Prozess-Status 2 des letzten Prozesses",
			"Prozess-Status 3 des letzten Prozesses",
			"An das UP uebergebene #Phys. Einr.-Nr.#",
			"",
		},
	},

	{
		0x048f,
		"Phys. Einr.-Nr. der Einrichtung beim Sperren der Einrichtung ausserhalb des Wertebereiches.",
		9,
		{
			"DKV-Nummer",
			"#Ident-Nummer#",
			"LOW-PC des letzten Prozesses",
			"HIGH-PC des letzten Prozesses",
			"Prozess-Status 1 des letzten Prozesses (LOW-TO)",
			"Prozess-Status 1 des letzten Prozesses (HIGH-TO)",
			"Prozess-Status 2 des letzten Prozesses",
			"Prozess-Status 3 des letzten Prozesses",
			"An das UP uebergebene #Phys. Einr.-Nr.#",
			"",
		},
	},

	{
		0x0490,
		"Phys. Einr.-Nr. der Einrichtung beim Abfragen der Einrichtung (frei/gesperrt) ausserhalb des Wertebereiches.",
		9,
		{
			"DKV-Nummer",
			"#Ident-Nummer#",
			"LOW-PC des letzten Prozesses",
			"HIGH-PC des letzten Prozesses",
			"Prozess-Status 1 des letzten Prozesses (LOW-TO)",
			"Prozess-Status 1 des letzten Prozesses (HIGH-TO)",
			"Prozess-Status 2 des letzten Prozesses",
			"Prozess-Status 3 des letzten Prozesses",
			"An das UP uebergebene #Phys. Einr.-Nr.#",
			"",
		},
	},

	{
		0x0491,
		"Frequenz in mehr als einem OgK verwendet.",
		9,
		{
			"DKV-Nummer",
			"#Ident-Nummer#",
			"LOW-PC des letzten Prozesses",
			"HIGH-PC des letzten Prozesses",
			"Prozess-Status 1 des letzten Prozesses (LOW-TO)",
			"Prozess-Status 1 des letzten Prozesses (HIGH-TO)",
			"Prozess-Status 2 des letzten Prozesses",
			"Prozess-Status 3 des letzten Prozesses",
			"#Frequenz-Nummer#, die in mehreren OgKs verwendet wird",
			"",
		},
	},

	{
		0x0492,
		"Sprechkreis-Nummer des 2. OSK-Paares ausserhalb des Wertebereiches.",
		9,
		{
			"DKV-Nummer",
			"#Ident-Nummer#",
			"LOW-PC des letzten Prozesses",
			"HIGH-PC des letzten Prozesses",
			"Prozess-Status 1 des letzten Prozesses (LOW-TO)",
			"Prozess-Status 1 des letzten Prozesses (HIGH-TO)",
			"Prozess-Status 2 des letzten Prozesses",
			"Prozess-Status 3 des letzten Prozesses",
			"#Sprechkreis-Nummer# des 2. OSK-Paares",
			"",
		},
	},

	{
		0x0493,
		"Anzahl der OSK-Paerchen ausserhalb des Wertebereiches.",
		9,
		{
			"DKV-Nummer",
			"#Ident-Nummer#",
			"LOW-PC des letzten Prozesses",
			"HIGH-PC des letzten Prozesses",
			"Prozess-Status 1 des letzten Prozesses (LOW-TO)",
			"Prozess-Status 1 des letzten Prozesses (HIGH-TO)",
			"Prozess-Status 2 des letzten Prozesses",
			"Prozess-Status 3 des letzten Prozesses",
			"Anzahl der OSK-Paerchen",
			"",
		},
	},

	{
		0x0494,
		"FuPeF-Systemmeldung wurde empfangen. Die Bedeutung der Indizien muss unter der uebergebenen Systemmeldungs-Nummer in einer FuPeF-Beschreibung nachgelesen werden. Je nach Indizienlaenge sind nur die entsprechenden RP-Bytes gueltig.",
		7,
		{
			"Physikalische Einrichtungs-Nummer",
			"Systemmeldungs-Nummer (High-Byte)",
			"Systemmeldungs-Nummer (Low-Byte)",
			"Indizien-Byte 1 der FuPeF-Systemmeldung",
			"Indizien-Byte 2 der FuPeF-Systemmeldung",
			"Indizien-Byte 3 der FuPeF-Systemmeldung",
			"Indizien-Byte 4 der FuPeF-Systemmeldung",
			"", "", "",
		},
	},

	{
		0x0495,
		"DKO-Systemmeldung wurde empfangen. Die Bedeutung der Indizien muss unter der uebergebenen Systemmeldungs- Nummer in einer DKO-Beschreibung nachgelesen werden. Je nach Indizienlaenge sind nur die entsprechenden RP-Bytes gueltig.",
		8,
		{
			"Physikalische Einrichtungs-Nummer",
			"Systemmeldungs-Nummer (High-Byte)",
			"Systemmeldungs-Nummer (Low-Byte)",
			"Indizien-Byte 1 der DKO-Systemmeldung",
			"Indizien-Byte 2 der DKO-Systemmeldung",
			"Indizien-Byte 3 der DKO-Systemmeldung",
			"Indizien-Byte 4 der DKO-Systemmeldung",
			"Indizien-Byte 5 der DKO-Systemmeldung",
			"", "",
		},
	},

	{
		0x0496,
		"Systemmeldungs-Verlust. Da die MSC-Hif voruebergehend nicht verfuegbar war und der FBH-Ringpuffer seine Fuell- standsgrenze erreicht hat, wurden die RP-Eintraege ab diesem Zeitpunkt ueberschrieben.",
		8,
		{
			"Physikalische Einrichtungs-Nummer der DKV",
			"Systemmeldungs-Nummer (High-Byte) des ersten freigegebenen RP-Elementes",
			"Systemmeldungs-Nummer (Low-Byte) des ersten freigegebenen RP-Elementes",
			"Monat des ersten freigegebenen RP-Elementes",
			"Tag des ersten freigegebenen RP-Elementes",
			"Stunde des ersten freigegebenen RP-Elementes",
			"Minute des ersten freigegebenen RP-Elementes",
			"Ursprungs-FDS des ersten freigegebenen RP-Elementes",
			"", "",
		},
	},

	{
		0x0497,
		"Die FDS hat einen Anlauf durchgefuehrt",
		2,
		{
			"Physikalische Einrichtungs-Nummer der DKV",
			"#IYLKPR#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x0498,
		"Prozess wurde durch Timeout gestartet und hat Inkonsistenz im Ringpuffer festgestellt. Es gibt keine sinnvollen Indizien, trotzdem werden 10 Indizien ausgegeben !!!!!",
		0,
		{
			"", "", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x0499,
		"Prozess wurde durch einen unzulaessigen Ereignis-Typ aufgerufen.",
		3,
		{
			"Physikalische Einrichtungs-Nummer der DKV",
			"#Ereignis-Typ#",
			"#Opcode#, falls Ereignis-Typ = Signalisierung",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x049a,
		"Beim Empfang einer Signalisierung von der Parallel-FDS wurde festgestellt, dass der empfangene Opcode nicht der erwarteten Signalisierung entspricht.",
		2,
		{
			"Physikalische Einrichtungs-Nummer der DKV",
			"#Opcode#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x049b,
		"Prozess wurde durch einen unerwarteten Ereignis-Typ aufgerufen.",
		3,
		{
			"Physikalische Einrichtungs-Nummer der DKV",
			"#Ereignis-Typ#",
			"#Opcode#, falls Ereignis-Typ = Signalisierung",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x049c,
		"Prozess wurde durch einen unerwarteten Ereignis-Typ aufgerufen.",
		3,
		{
			"Physikalische Einrichtungs-Nummer der DKV",
			"#Ereignis-Typ#",
			"#Opcode#, falls Ereignis-Typ = Signalisierung",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x04a0,
		"Der Transfer der RP-Elemente von der STBFDS zur ACTFDS wird beendet, da die bisherige STBFDS ihren Betriebszustand in ACT geaendert hat.",
		2,
		{
			"Physikalische Einrichtungs-Nummer der DKV",
			"Port #DYIFDS#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x04a1,
		"Prozess wurde durch einen unerwarteten Ereignis-Typ aufgerufen.",
		3,
		{
			"Physikalische Einrichtungs-Nummer der DKV",
			"#Ereignis-Typ#",
			"#Opcode#, falls Ereignis-Typ = Signalisierung",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x04a2,
		"Prozess wurde durch einen unerwarteten Ereignis-Typ aufgerufen.",
		3,
		{
			"Physikalische Einrichtungs-Nummer der DKV",
			"#Ereignis-Typ#",
			"#Opcode#, falls Ereignis-Typ = Signalisierung",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x04a3,
		"RP-Element konnte nicht in die Parallel-FDS (ACT-FDS) uebertragen werden. (Parallel-FDS hat zweimal negativ quittiert und die eigene FDS befindet sich im Betriebszustand STB)",
		2,
		{
			"Physikalische Einrichtungs-Nummer der DKV",
			"Port #DYIFDS#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x04a4,
		"Die Parallel-FDS hat den RP-Transfer nicht quittiert und die eigene FDS hat ihren Betriebszustand von STB in ACT geaendert.",
		3,
		{
			"Physikalische Einrichtungs-Nummer der DKV",
			"Port #DYIFDS#",
			"#SYSDMP#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x04a5,
		"RP-Element konnte nicht in die Parallel-FDS uebertragen werden. (Die Parallel-FDS hat nicht quittiert und die eigene FDS befindet sich im Betriebszustand STB)",
		3,
		{
			"Physikalische Einrichtungs-Nummer der DKV",
			"Port #DYIFDS#",
			"#SYSDMP#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x04a7,
		"Die Parallel-FDS (ACT-FDS) \"nicht verfuegbar\" oder \"DMA-Sperre gesetzt\". Der Transfer der RP-Elemente von der STB-FDS zur ACT-FDS muss abgebrochen werden.",
		3,
		{
			"Physikalische Einrichtungs-Nummer der DKV",
			"Port #DYIFDS#",
			"#SYSDMP#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x04a8,
		"Prozess wurde mit nicht erwartetem Ereignis-Typ oder falschem Opcode aufgerufen.",
		3,
		{
			"Physikalische Einrichtungs-Nummer der DKV",
			"#Ereignis-Typ#",
			"#Opcode#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x04a9,
		"Vom PBR erwartete Signalisierung ist innerhalb der durch Timeout ueberwachten Zeit nicht gekommen.",
		1,
		{
			"Physikalische Einrichtungs-Nummer der DKV",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x04aa,
		"Prozess wurde mit nicht erwartetem Ereignis-Typ aufgerufen.",
		3,
		{
			"Physikalische Einrichtungs-Nummer der DKV",
			"#Ereignis-Typ#",
			"#Opcode#, falls Ereignis-Typ = Signalisierung",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x04ab,
		"Prozess wurde mit nicht erwartetem Ereignis-Typ aufgerufen",
		3,
		{
			"Physikalische Einrichtungs-Nummer der DKV",
			"#Ereignis-Typ#",
			"#Opcode#, falls Ereignis-Typ = Signalisierung",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x04ac,
		"Prozess wurde mit nicht erwartetem Ereignis-Typ aufgerufen",
		3,
		{
			"Physikalische Einrichtungs-Nummer der DKV",
			"#Ereignis-Typ#",
			"#Opcode#, falls Ereignis-Typ = Signalisierung",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x04ad,
		"Falscher Opcode beim Empfang der Quittung von einer peripheren Einrichtung",
		3,
		{
			"FDS-Nr",
			"FKS-Nummer",
			"#Opcode#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x04ae,
		"Falscher Ereignis-Typ beim Empfang der Quittung von einer peripheren Einrichtung",
		5,
		{
			"FDS-Nr",
			"#Ereignis-Typ#",
			"FKS-Nummer",
			"#Ident-Nummer#",
			"#Opcode#",
			"", "", "", "", "",
		},
	},

	{
		0x04af,
		"Falscher Opcode oder Ereignis-Typ beim Empfang einer DKV-internen Signalisierung",
		5,
		{
			"FDS-Nr",
			"#Ereignis-Typ#",
			"FKS-Nummer",
			"#Ident-Nummer#",
			"#Opcode#",
			"", "", "", "", "",
		},
	},

	{
		0x04b0,
		"Falscher Opcode beim Empfang der Quittung von einer peripheren Einrichtung",
		3,
		{
			"FDS-Nr",
			"FKS-Nummer",
			"#Opcode#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x04b1,
		"Falscher Ereignis-Typ beim Empfang der Quittung von einer peripheren Einrichtung",
		5,
		{
			"FDS-Nr",
			"#Ereignis-Typ#",
			"FKS-Nummer",
			"#Ident-Nummer#",
			"#Opcode#",
			"", "", "", "", "",
		},
	},

	{
		0x04b2,
		"Interne Signalisierung (TFGAI) konnte nicht eingetragen werden",
		1,
		{
			"FDS-Nr",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x04b3,
		"Interne Signalisierung (TVFAI) konnte nicht eingetragen werden",
		1,
		{
			"FDS-Nr",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x04b4,
		"Falscher Einrichtungs-Typ bei Auftrags-Aussendung an einzeln anlaufende Einrichtungen",
		3,
		{
			"FDS-Nr",
			"#Einrichtungs-Typ#",
			"Einrichtungs-Nummer",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x04b5,
		"Falscher Opcode oder Ereignis-Typ beim Empfang einer internen Signalisierung (Auftrag) vom PHE",
		5,
		{
			"FDS-Nr",
			"#Ereignis-Typ#",
			"FKS-Nummer",
			"#Ident-Nummer#",
			"#Opcode#",
			"", "", "", "", "",
		},
	},

	{
		0x04b6,
		"Falscher Einrichtungs-Typ bei Auftrags-Aussendung an eine periphere Einrichtung",
		2,
		{
			"FDS-Nr",
			"#Einrichtungs-Typ#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x04b7,
		"unzulaessiger Return-Code aus einem Unterprogramm (Systemmeldungs-Nummer wird in diesem Modul mehrfach benutzt ! )",
		4,
		{
			"FDS-Nr",
			"Prozedur-Identifikation",
			"Prozedur-Identifikation",
			"#Return-Code der FT#",
			"", "", "", "", "", "",
		},
	},

	{
		0x04b8,
		"Quittung von falscher Einrichtung",
		4,
		{
			"phys. Einrichtungs-Nummer aus Signalisierung",
			"erwartete phys. Einrichtungs-Nummer",
			"Prozess-Zustand (#LDTSTA#)",
			"#Ident-Nummer# aus Signalisierung",
			"", "", "", "", "", "",
		},
	},

	{
		0x04b9,
		"falscher Aufruf ohne Signalisierung: es wurde ein fuer den betreffenden Pozesszustand un- erlaubter Ereignis-Typ erkannt, zu dem es keine Signalisierung gibt (Timeout oder Ready)",
		4,
		{
			"FDS-Nr",
			"Prozess-Zustand (#LDTSTA#)",
			"#Ereignis-Typ#",
			"#Ident-Nummer# der Task",
			"", "", "", "", "", "",
		},
	},

	{
		0x04ba,
		"falscher Aufruf mit Signalisierung; es wurde ein fuer den betreffenden Prozesszustand unerlaubter Ereignis- Typ erkannt, zu dem es eine Signalisierung gibt",
		6,
		{
			"FDS-Nr",
			"Prozess-Zustand (#LDTSTA#)",
			"#Ereignis-Typ#",
			"#Ident-Nummer# der Task",
			"#Opcode#",
			"#Ident-Nummer# aus Signalisierung",
			"", "", "", "",
		},
	},

	{
		0x04bb,
		"Zustandsfehler ohne Signalisierung: es wurde ein unerlaubter Prozesszustand erkannt, bei dem keine Signalisierung existiert (Prozesszustand \"Signalisierung aussenden\")",
		4,
		{
			"FDS-Nr",
			"Prozesszustand (#LDTSTA#)",
			"#Ereignis-Typ#",
			"#Ident-Nummer# der Task",
			"", "", "", "", "", "",
		},
	},

	{
		0x04bc,
		"Zustandsfehler mit Signalisierung: es existiert eine Signalisierung mit unerlaubtem Prozesszustand (Prozesszustand \"Quittung bearbeiten\")",
		6,
		{
			"FDS-Nr",
			"Prozesszustand (#LDTSTA#)",
			"#Ereignis-Typ#",
			"#Ident-Nummer# der Task",
			"#Opcode#",
			"#Ident-Nummer# aus Signalisierung",
			"", "", "", "",
		},
	},

	{
		0x04bd,
		"Datenverfaelschung beim Laden der Datenbasis. (T143) (Die waehrend des Ladens ermittelte Checksum stimmt nicht mit der in der Datenbasis befindlichen Checksum ueberein.)",
		9,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Auftraggeberkennzeichen#",
			"Prozedur-Identifikation des betreffenden Moduls : #Entwickler-Info#",
			"LOW-Byte der errechneten Checksum",
			"MIDDLE-Byte der errechneten Checksum",
			"HIGH-Byte der errechneten Checksum",
			"LOW-Byte der Checksum in der Datenbasis",
			"MIDDLE-Byte der Checksum in der Datenbasis",
			"HIGH-Byte der Checksum in der Datenbasis",
			"",
		},
	},

	{
		0x04be,
		"Versionsunvertraeglichkeit zwischen DKV-Software und Datenbasis. (T144)",
		5,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Auftraggeberkennzeichen#",
			"1. Byte des fehlerhaften Versionskennzeichens",
			"2. Byte des fehlerhaften Versionskennzeichens",
			"Ort des fehlerhaften Versionskennzeichens (Urladedatei/BSSYF) #Entwickler-Info#",
			"", "", "", "", "",
		},
	},

	{
		0x04bf,
		"Datenverfaelschung in der DKV. (T001) (Unbekanntes Speicherbankkennzeichen in einer  AKZ-Tabelle.)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"Prozedur-Identifikation des betreffenden Moduls : #Entwickler-Info#",
			"Fehlerhaftes Speicherbankkennzeichen",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x04c0,
		"Unbekannter #Opcode# in Signalisierung von der MSC",
		3,
		{
			"Physikalische Einrichtungs-Nummer der DKV",
			"Empfangener #Opcode#",
			"#Zustand von DKV-BT-TDA#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x04c1,
		"Unbekannter #Opcode# bei interner Signalisierung",
		3,
		{
			"Physikalische Einrichtungs-Nummer der DKV",
			"Empfangener #Opcode#",
			"#Zustand von DKV-BT-TDA#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x04c2,
		"Time-Out wegen Ausbleiben der MSC-Signalisierungen (MZTO)",
		3,
		{
			"Physikalische Einrichtungs-Nummer der DKV",
			"Zonen-Nummer der letzten Signalisierung",
			"#Tabellenkennzeichen# (K) der letzten Signalisierung",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x04c3,
		"Unbekannter #Ereignis-Typ#",
		2,
		{
			"Physikalische Einrichtungs-Nummer der DKV",
			"Aktueller #Ereignis-Typ#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x04c4,
		"Unzulaessiges Kennzeichen in GTAU-Signalisierung, die nicht angefordert wurde.",
		3,
		{
			"Physikalische Einrichtungs-Nummer der DKV",
			"Kennzeichen in GTAU-Signalisierung",
			"#Zustand von DKV-BT-TDA#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x04c5,
		"Verlust der letzten Signalisierungen der aktuellen TD-Tabelle",
		3,
		{
			"Physikalische Einrichtungs-Nummer der DKV",
			"VXMELK: Kennzeichen aus GTAU-Signalisierung",
			"VXDBEA: Bearbeitungskennzeichen der DKV-BT-TDA",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x04c6,
		"Fehler bei CS-Pruefung erkannt",
		5,
		{
			"Physikalische Einrichtungs-Nummer der DKV",
			"CS aus Signalisierung (Lower Byte)",
			"CS aus Signalisierung (Higher Byte)",
			"Errechnete CS (Lower Byte)",
			"Errechnete CS (Higher Byte)",
			"", "", "", "", "",
		},
	},

	{
		0x04c7,
		"Fehler bei CS-Pruefung erkannt",
		5,
		{
			"Physikalische Einrichtungs-Nummer der DKV",
			"CS aus Signalisierung (Lower Byte)",
			"CS aus Signalisierung (Higher Byte)",
			"Errechnete CS (Lower Byte)",
			"Errechnete CS (Higher Byte)",
			"", "", "", "", "",
		},
	},

	{
		0x04c8,
		"(TUAU-)Signalisierung nicht zulaessig, da TD-Ueber- tragung von MSC",
		2,
		{
			"Physikalische Einrichtungs-Nummer der DKV",
			"#Tabellenkennzeichen#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x04c9,
		"(TUAU-)Signalisierung nicht zulaessig, da Uebertragung von angeforderten TD stattfindet.",
		3,
		{
			"Physikalische Einrichtungs-Nummer der DKV",
			"#Zustand von DKV-BT-TDA#",
			"#Tabellenkennzeichen#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x04ca,
		"Datenverfaelschung in der TD-Tabelle; TDA-Zustand war 'laufend'. TD-Anforderungsauftrag (STDAF) abgesetzt.",
		1,
		{
			"Physikalische Einrichtungs-Nummer der DKV",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x04cb,
		"Ueberwachungszeit fuer TD-Uebertragung abgelaufen (ueber DB-Parameter IXTZSK vorgegeben; z.ZT. 12 Min).",
		1,
		{
			"Physikalische Einrichtungs-Nummer der DKV",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x04cc,
		"Die BS hat keine gueltigen Tarifdaten empfangen. (Anzeige fuer Systemfehler gesetzt) (Enthaelt Byte 5 den Wert 0CH, so sind die Bytes 1 bis 3 nicht relevant)",
		6,
		{
			"Physikalische Einrichtungs-Nummer der DKV",
			"#Fehlerkennzeichen# des 1. Fehlers",
			"#Fehlerkennzeichen# des 2. Fehlers",
			"#Fehlerkennzeichen# des 3. Fehlers",
			"Anzahl neg. MSC-Uebertragungsversuche",
			"Anzahl ausbleibender MSC-Quittungen",
			"", "", "", "",
		},
	},

	{
		0x04cd,
		"Falscher #Ereignis-Typ#",
		2,
		{
			"Physikalische Einrichtungs-Nummer der DKV",
			"#Ereignis-Typ#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x04ce,
		"Pseudo-Fehler fuer die Uebergabe der gesammelten Daten vom DKV-Prozessverfolger an die DKV-FBH (fuer den von der MSC angestossenen DUMP-Auftrag).",
		9,
		{
			"Phys. Einrichtungsnummer",
			"RAM-Datum 1",
			"RAM-Datum 2",
			"RAM-Datum 3",
			"RAM-Datum 4",
			"RAM-Datum 5",
			"RAM-Datum 6",
			"RAM-Datum 7",
			"RAM-Datum 8",
			"",
		},
	},

	{
		0x04cf,
		"Datenverfaelschung  im  Anlauf  mit der  angegebenen Einrichtung. (T044) (Prozessanstoss durch unzulaessigen Opcode)",
		5,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Ereignis-Typ#",
			"Ident-Nr. des absendenden Prozesses",
			"#Opcode#",
			"Eigene Ident-Nr",
			"", "", "", "", "",
		},
	},

	{
		0x04d0,
		"Die angegebene Einrichtung wurde waehrend ihres Anlaufs nach UNA konfiguriert. (T045) (Parallel-FDS ist nicht verfuegbar)",
		4,
		{
			"Phys. Einr.-Nr",
			"Anzahl der Anlaufversuche",
			"#Return-Code von WMESEP#",
			"Eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x04d1,
		"Die angegebene Einrichtung wurde waehrend ihres Anlaufs nach UNA konfiguriert. (T045) (ST-Zustand \"DMA-Sperre\" wurde vom Macro-Aufruf WMESEP  als Return-Code zurueckgeliefert)",
		4,
		{
			"Phys. Einr.-Nr",
			"Anzahl der Anlaufversuche",
			"#Return-Code von WMESEP#",
			"Eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x04d2,
		"Die angegebene Einrichtung wurde waehrend ihres Anlaufs nach UNA konfiguriert. (T045) (SW-Versions-Nr. der ACTFDS und der STBFDS stimmen  nicht ueberein)",
		5,
		{
			"Phys. Einr.-Nr",
			"Anzahl der Anlaufversuche",
			"SW-Versions-Nr. der ACTFDS",
			"SW-Versions-Nr. der STBFDS",
			"Eigene Ident-Nr",
			"", "", "", "", "",
		},
	},

	{
		0x04d3,
		"Datenverfaelschung im BS-Anlauf. (T055) (unzulaessiger Return-Code vom OS-UP WTAKOM bei Pufferanforderung fuer die DKV-interne Signalisierung XTMII)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code von WTAKOM#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x04d4,
		"Datenverfaelschung im BS-Anlauf. (T055) (unzulaessiger Return-Code vom OS-UP WTAKOM bei Pufferanforderung fuer die DKV-interne Signalisierung XTAAI)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code von WTAKOM#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x04d5,
		"Datenverfaelschung im BS-Anlauf. (T055) (unzulaessiger Return-Code aus OS-UP WTAKOM bei Pufferanforderung fuer die DKV-interne Signalisierung YNLAI)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code von WTAKOM#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x04d6,
		"Datenverfaelschung im BS-Anlauf. (T055) (unzulaessiger Return-Code aus OS-UP WTAKOM bei Pufferanforderung fuer die DKV-interne Signalisierung YNLAI)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code von WTAKOM#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x04d7,
		"Datenverfaelschung im BS-Anlauf. (T055) (unzulaessiger Return-Code vom OS-UP WTAKOM bei Pufferanforderung fuer die DKV-interne Signalisierung YALQI)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code von WTAKOM#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x04d8,
		"Datenverfaelschung im BS-Anlauf. (T055) (unzulaessiger Return-Code vom OS-UP WTAKOM bei Pufferanforderung fuer die DKV-interne Signalisierung XLABM)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code von WTAKOM#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x04d9,
		"Datenverfaelschung im BS-Anlauf. (T055) (unzulaessiger Return-Code vom OS-UP WTAKOM bei Pufferanforderung fuer die DKV-interne Signalisierung XLABM)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code von WTAKOM#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x04da,
		"Datenverfaelschung im BS-Anlauf. (T055) (Prozessanstoss mit falschem Ereignis-Typ)",
		4,
		{
			"Phys. Einr.-Nr. der FDS",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"eigene Ident-Nr",
			"#Ereignis-Typ#",
			"", "", "", "", "", "",
		},
	},

	{
		0x04db,
		"Datenverfaelschung im BS-Anlauf. (T055) (Aus ANL-UP YPAKQU kommt unzulaessiger Return-Code zurueck)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code ANL#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x04dc,
		"Datenverfaelschung im BS-Anlauf. (T055) (unzulaessiger Return-Code aus DKV-interner Signalisierung XLQBM)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code von XLQBM#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x04dd,
		"Datenverfaelschung im BS-Anlauf. (T055) (Prozessanstoss mit falschem Ereignis-Typ bei Anstoss des ANL(ANK)-Steuermoduls YANKAS)",
		4,
		{
			"Phys. Einr.-Nr. der FDS",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"eigene Ident-Nr",
			"#Ereignis-Typ#",
			"", "", "", "", "", "",
		},
	},

	{
		0x04de,
		"Datenverfaelschung im BS-Anlauf. (T055) (Prozessanstoss mit falschem Ereignis-Typ oder Opcode beim Anstoss des ANL(ANK)-Steuermoduls YANKAS)",
		6,
		{
			"Phys. Einr.-Nr. der FDS",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"eigene Ident-Nr",
			"#Ereignis-Typ#",
			"#Opcode#",
			"Ident-Nr. des Absenders",
			"", "", "", "",
		},
	},

	{
		0x04df,
		"Datenverfaelschung im BS-Anlauf. (T055) (Unzulaessiger Prozesszustand)",
		6,
		{
			"Phys. Einr.-Nr. der FDS",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"eigene Ident-Nr",
			"#Ereignis-Typ#",
			"#Opcode#",
			"Ident-Nr. des Absenders",
			"", "", "", "",
		},
	},

	{
		0x04e0,
		"Datenverfaelschung im BS-Anlauf. (T055) (unzulaessiger Prozesszustand)",
		4,
		{
			"Phys. Einr.-Nr. der FDS",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"eigene Ident-Nr",
			"#Ereignis-Typ#",
			"", "", "", "", "", "",
		},
	},

	{
		0x04e1,
		"Datenverfaelschung im BS-Anlauf. (T055) (Prozessanstoss mit falschem Opcode)",
		5,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Ereignis-Typ#",
			"#Opcode#",
			"Ident-Nr. des Absenders",
			"eigene Ident-Nr",
			"", "", "", "", "",
		},
	},

	{
		0x04e2,
		"Datenverfaelschung im BS-Anlauf. (T055) (Prozessanstoss mit falschem Ereignis-Typ)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Ereignis-Typ#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x04e3,
		"Datenverfaelschung im BS-Anlauf. (T055) (Prozessanstoss mit falschem Opcode)",
		5,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Ereignis-Typ#",
			"#Opcode#",
			"Ident-Nr. des Absenders",
			"eigene Ident-Nr",
			"", "", "", "", "",
		},
	},

	{
		0x04e4,
		"Datenverfaelschung im BS-Anlauf. (T055) (Prozessanstoss mit falschem Ereignis-Typ)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Ereignis-Typ#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x04e5,
		"Datenverfaelschung im BS-Anlauf. (T055) (unzulaessiger Return-Code aus ANL-UP YPAKTQ)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code ANL#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x04e6,
		"Datenverfaelschung im BS-Anlauf. (T055) (Aus ANL-UP YPAKTQ kommt unzulaessiger Return-Code zurueck)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code ANL#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x04e7,
		"Datenverfaelschung im BS-Anlauf. (T055) (Aus ANL-UP YPAKTQ kommt unzulaessiger Return-Code zurueck)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code ANL#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x04e8,
		"Datenverfaelschung im BS-Anlauf. (T055) (Aus ANL-UP YPAKAE kommt unzulaessiger Return-Code zurueck)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code ANL#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x04e9,
		"Datenverfaelschung im BS-Anlauf. (T055) (Aus ANL-UP YPAKQU kommt unzulaessiger Return-Code zurueck)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code ANL#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x04ea,
		"Datenverfaelschung im BS-Anlauf. (T055) (Aus ANL-UP YPAKTQ kommt unzulaessiger Return-Code zurueck)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code ANL#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x04eb,
		"Datenverfaelschung im BS-Anlauf. (T055) (Aus ANL-UP YPAKQU kommt unzulaessiger Return-Code zurueck)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code ANL#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x04ec,
		"Datenverfaelschung im BS-Anlauf. (T055) (unzulaessiger Return-Code aus OS-UP WTAKOM bei Pufferanforderung fuer die DKV-interne Signalisierung YNLAI)",
		4,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code von WTAKOM#",
			"#ANK-Situationsanzeige#",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x04ed,
		"Datenverfaelschung im BS-Anlauf. (T055) (unzulaessiger Return-Code aus OS-UP WTAKOM bei Pufferanforderung fuer die DKV-interne Signalisierung YUAAI)",
		4,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code von WTAKOM#",
			"#ANK-Situationsanzeige#",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x04ee,
		"Datenverfaelschung im BS-Anlauf. (T055) (unzulaessiger Return-Code aus OS-UP WTAKOM bei Pufferanforderung fuer die DKV-interne Signalisierung YIAAI)",
		4,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code von WTAKOM#",
			"#ANK-Situationsanzeige#",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x04ef,
		"Datenverfaelschung im BS-Anlauf. (T055) (unzulaessiger Return-Code aus OS-UP WTAKOM bei Pufferanforderung fuer die DKV-interne Signalisierung YKAAI fuer PBR)",
		4,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code von WTAKOM#",
			"#ANK-Situationsanzeige#",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x04f0,
		"Datenverfaelschung im BS-Anlauf. (T055) (unzulaessiger Return-Code aus OS-UP WTAKOM bei Pufferanforderung fuer die DKV-interne Signalisierung YKAAI fuer PHE1)",
		4,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code von WTAKOM#",
			"#ANK-Situationsanzeige#",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x04f1,
		"Datenverfaelschung im BS-Anlauf. (T055) (unzulaessiger Return-Code aus OS-UP WTAKOM bei Pufferanforderung fuer die DKV-interne Signalisierung YKAAI fuer PHE2)",
		4,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code von WTAKOM#",
			"#ANK-Situationsanzeige#",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x04f2,
		"Datenverfaelschung im BS-Anlauf. (T055) (unzulaessiger Return-Code aus ANL-UP YPAKNQ)",
		4,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code ANL#",
			"#ANK-Situationsanzeige#",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x04f3,
		"Datenverfaelschung im BS-Anlauf. (T055) (unzulaessiger Return-Code aus ANL-UP YPAKNQ)",
		4,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code ANL#",
			"#ANK-Situationsanzeige#",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x04f4,
		"Die BS befindet sich im Zustand BUF (Betriebs- unfaehigkeit). Innerhalb der Ueberwachungszeit (z.Z. 1 Stunde) hat der Betreiber kein Kommando INIT-BS eingegeben; es erfolgt ein SW-Reset.(T125)",
		4,
		{
			"Phys. Einr.-Nr. der FDS",
			"Prozesszustand (LODSTA): #Entwickler-Info#",
			"#ANK-Situationsanzeige#",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x04f5,
		"Datenverfaelschung im BS-Anlauf. (T055) (Prozessanstoss mit unzulaessigem Ereignis-Typ)",
		5,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Ereignis-Typ#",
			"Prozesszustand (LODSTA): #Entwickler-Info#",
			"#ANK-Situationsanzeige#",
			"eigene Ident-Nr",
			"", "", "", "", "",
		},
	},

	{
		0x04f6,
		"Datenverfaelschung im BS-Anlauf. (T055) (Prozessanstoss mit unzulaessigem Ereignis-Typ bzw. Quittungen fuer Einrichtungsanlaeufe kamen nicht in der vorgegebenen Zeit)",
		6,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Ereignis-Typ#",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"Zaehler fehlender Quittungen (LODZFQ) : #Entwickler-Info#",
			"Zaehler gesendeter Konfigurationsauftraege (LODZKO) : #Entwickler-Info#",
			"#ANK-Situationsanzeige#",
			"", "", "", "",
		},
	},

	{
		0x04f7,
		"Datenverfaelschung im BS-Anlauf. (T055) (Eintreffen der nicht erwarteten Quittung YKAQI )",
		3,
		{
			"Phys. Einr.-Nr",
			"#Eintrag aus KOORDLISTE#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x04f8,
		"Im BS-Anlauf wird ein Konfigurationsauftrag nach ACT fuer eine Einrichtung negativ quittiert. (T072) (negative Quittung YKAQI fuer den ACT-Auftrag fuer PBR bei Betriebsunfaehigkeit (BUF) ==> Not-Betrieb ohne PBR hat keinen Sinn)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#ANK-Situationsanzeige#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x04f9,
		"Die angegebene Einrichtung wurde waehrend ihres Anlaufs nach UNA konfiguriert. (T045) (Anlaufversuchsstatistik der angegebenen peripheren  Einrichtung hat Schwellwert erreicht)",
		4,
		{
			"Phys. Einr.-Nr",
			"Anzahl der Anlaufversuche",
			"Prozesszustand (LODSTA): #Entwickler-Info#",
			"Eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x04fa,
		"Die angegebene Einrichtung wurde waehrend ihres Anlaufs nach UNA konfiguriert. (T045) (Anlaufversuchsstatistik der STBFDS hat Schwellwert  erreicht)",
		3,
		{
			"Phys. Einr.-Nr",
			"Anzahl der Anlaufversuche",
			"Eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x04fb,
		"Die in Byte 1 angegebene Einrichtung schickt Anlaufmeldung, obwohl Port bereits geschlossen ist. (T139) (Anlauf mit der in BYTE 1 angegebenen Einrichtung kann  nicht durchgefuehrt werden, da Port nicht geoeffnet;  die Anlaufmeldung wird ignoriert)",
		5,
		{
			"Phys. Einr.-Nr. der FDS",
			"Phys. Einr.-Nr",
			"#Einrichtungs-Typ#",
			"#Log. Einr.-Nr.#",
			"Eigene Ident-Nr",
			"", "", "", "", "",
		},
	},

	{
		0x04fc,
		"Datenverfaelschung im BS-Anlauf. (T055) (Prozessanstoss mit unzulaessigem Ereignistyp)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Ereignis-Typ#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x04fd,
		"Datenverfaelschung in der DKV. (T001) (Prozessanstoss bei unzulaessigem Prozesszustand)",
		4,
		{
			"Phys. Einr.-Nr. der FDS",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"#Ereignis-Typ#",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x04fe,
		"Datenverfaelschung in der DKV. (T001) (Prozessanstoss bei unzulaessigem Prozesszustand)",
		6,
		{
			"Phys. Einr.-Nr. der FDS",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"#Ereignis-Typ#",
			"eigene Ident-Nr",
			"#Opcode#",
			"Ident-Nr. aus Signalisierung",
			"", "", "", "",
		},
	},

	{
		0x04ff,
		"Datenverfaelschung in der DKV. (T001) (unzulaessiger Return-Code aus KON-UP YPKPVT)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code KON#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0500,
		"Am Ende des BS-Anlaufs sind nicht alle FUPEF-Einrichtungen verfuegbar, die fuer den Vermittlungsbetrieb bzw. zum Erfuellen der Bakenfunktion (je nach BS-Typ) benoetigt werden oder das MSC ist nicht verfuegbar. (T154) (VTB-Voraussetzungen bzw. Voraussetzungen zum Erfuellen  der Bakenfunktion bei BS-Anlauf-Ende nicht vorhanden,  ausser Frequenzgenauigkeit)",
		9,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Globaler ST-Zustand# OGK 1",
			"#Globaler ST-Zustand# OGK 2",
			"#Globaler ST-Zustand# OGK 3",
			"#Globaler ST-Zustand# PHE1",
			"#Globaler ST-Zustand# PHE2",
			"#Globaler ST-Zustand# MSC",
			"Zaehler FYKAUN : #Entwickler-Info#",
			"Zaehler FYKAPM : #Entwickler-Info#",
			"",
		},
	},

	{
		0x0501,
		"Datenverfaelschung in der DKV. (T001) (Zaehler fuer Anzahl SPKs im Zustand UNA (FYKAUN) ist  verfaelscht)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"verfaelschter Zaehler (FYKAUN)",
			"korrigierter Zaehler (FYKAUN)",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0502,
		"Datenverfaelschung in der DKV. (T001) (Zaehler fuer Anzahl SPKs im Zustand PLA/MBL (FYKAPM) ist  verfaelscht)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"verfaelschter Zaehler (FYKAPM)",
			"korrigierter Zaehler (FYKAPM)",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0503,
		"Behebbare Datenverfaelschung in der DKV. (T048) ( Returncode vom OS-UP WTAKOM bei Pufferanfoderung fuer die   DKV-interne Signalisierung YAAAI:   Ident-Nummer existiert nicht mehr )",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code von WTAKOM#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0504,
		"Datenverfaelschung in der DKV. (T001) ( Unzulaessiger Return-Code vom OS-UP WTAKOM bei   Pufferanforderung fuer die DKV-interne   Signalisierung YAAAI )",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code von WTAKOM#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0505,
		"Durch Konfigurationsauftrag wurde ein OGK ausser Betrieb genommen. Funktionstausch offenbar nicht moeglich. (T123) ( UNA bzw AMBL-Auftrag fur OSK(OGK) fuehrt zum Verlust   des OGK, bisheriger OSK(SPK) meldet sich nicht in der   Funktion OGK, sondern als SPK, .d.h. Verschlechterung   der VTB )",
		5,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Auftrags-Typ#",
			"Phys. Einr.-Nr",
			"#Globaler ST-Zustand# des korrespondierenden OSK",
			"eigene Ident-Nr",
			"", "", "", "", "",
		},
	},

	{
		0x0506,
		"Verlust der BS-VTB bzw. Verlust der Bakenfunktion (je nach BS-Typ) durch Konfiguration eines OSK nach UNA bzw. MBL. (T155) ( UNA/MBL-Auftrag fuer letzten aktiven OSK(OGK) fuehrt zum   VTB-Verlust bzw. Verlust der Bakenfunktion (je nach   BS-Typ), bisheriger OSK(SPK) meldet sich nicht als OGK )",
		5,
		{
			"Phys. Einr.-Nr. der FDS",
			"Phys. Einr.-Nr",
			"#Globaler ST-Zustand# des korrespondierenden OSK",
			"#Detaillierter ST-Zustand# des korrespondierenden OSK",
			"eigene Ident-Nr",
			"", "", "", "", "",
		},
	},

	{
		0x0507,
		"Datenverfaelschung in der DKV. (T001) ( Prozessanstoss mit unzulaessigem Ereignistyp )",
		4,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Ereignis-Typ#",
			"eigene Ident-Nr",
			"#Opcode# aus der Signalisierung",
			"", "", "", "", "", "",
		},
	},

	{
		0x0508,
		"MSC- oder ZZK-Ausfall im Zustand der Betriebs- unfaehigkeit. (T127) (ZZK-Ausfall bzw. MSC-Ausfall beim Konfigurieren  des MSC nach ACT. Betriebsunfaehigkeit liegt vor.)",
		2,
		{
			"Phys. Einr.-Nr. der FDS",
			"#ANK-Situationsanzeige#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x0509,
		"Beziehungswiederkehr zum MSC. (T129) (Anlauf mit dem MSC begonnen)",
		1,
		{
			"Phys. Einr.-Nr. der FDS",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x050a,
		"Datenverfaelschung in der DKV. (T001) ( Bei Konfiguration nach MBL des OSK(OGK),   unzulaessiger ST-Zustand des korrespondierenden   OSK(SPK) )",
		4,
		{
			"Phys. Einr.-Nr  des OSK(SPK)",
			"#Globaler ST-Zustand# des OSK(SPK)",
			"#Detaillierter ST-Zustand# des OSK(SPK)",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x050b,
		"Der OGK wurde vom Betreiber aus ACT nach MBL geschaltet. Keine OSK-Umschaltung mehr moeglich, da der korrespon- dierende OSK nicht verfuegbar ist. Aus dem BYTE 1 der Indizien ist ersichtlich, in welcher Funktion der OSK1 zum Fehlerzeitpunkt war; Wert 01: OGK, Wert 02: SPK. (T039) ( AMBL-Auftrag fuer OSK(OGK) fuehrt zum Verlust des OGK,   da korrespondierender OSK(SPK) nicht verfuegbar (UNA/MBL),   d.h Verschlechterung der VTB )",
		2,
		{
			"Phys. Einr.-Nr aus Signalisierung",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x050c,
		"Verlust der BS-VTB bzw. Verlust der Bakenfunktion (je nach BS-Typ), da kein OSK mehr verfuegbar; die angegebene Einrichtung wurde abgeschaltet. (T153) ( AMBL-Auftrag fuer letzten aktiven OSK(OGK) fuehrt zum   VTB-Verlust bzw. Verlust der Bakenfunktion (je nach   BS-Typ), da korrespondierender OSK(SPK) nicht   verfuegbar)",
		4,
		{
			"Phys. Einr.-Nr des OSK(SPK)",
			"#Globaler ST-Zustand# des OSK(SPK)",
			"#Detaillierter ST-Zustand# des OSK(SPK)",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x050d,
		"Datenverfaelschung beim Konfigurieren des PBR nach ACT. (T085) (Unzulaessiger Return-Code vom OS-UP WTAKOM bei  Pufferanforderung fuer DKV-interne Signalisierung  YLUTI zur Information ueber PBR-Anlauf)",
		3,
		{
			"Phys. Einr.-Nr. (LODEAD)",
			"#Return-Code von WTAKOM#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x050e,
		"Im Zustand BUF (Betriebsunfaehigkeit) wird der Anlauf mit dem MSC ordnungsgemaess abgeschlossen. Da aus dem Zustand BUF keine weitere Anlaufstufe mehr moeglich ist, wird SW-Reset angestossen. (T126) (Der ACT-Auftrag fuer das MSC wurde mit der  Signalisierung YKBAI positiv quittiert.  Es lag jedoch der Zustand Betriebsunfaehigkeit vor.)",
		1,
		{
			"Phys. Einr.-Nr. der FDS",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x050f,
		"Datenverfaelschung beim Anlauf mit der STBFDS. (T132) (Prozesszustand (LODSTA) unzulaessig)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"Phys. Einr.-Nr. (LODEAD)",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0510,
		"Datenverfaelschung beim Konfigurieren einer Einrichtung nach ACT. (T095) (Unzulaessiger Return-Code vom OS-UP WTAKOM bei  Pufferanforderung fuer DKV-interne Signalisierung  YLRTI zum Ringpuffertransfer)",
		3,
		{
			"Phys. Einr.-Nr. (LODEAD)",
			"#Return-Code von WTAKOM#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0511,
		"Ausbleiben der Statusmeldung vom angegebenen PHE. (T133) (Zeitueberwachung im Zustand YSZ1/WAST abgelaufen)",
		3,
		{
			"Phys. Einr.-Nr. des PHE",
			"#Detaillierter ST-Zustand# des PHE",
			"#Globaler ST-Zustand# des PHE",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0512,
		"Durch Konfigurieren eines OGK nach ACT wurde die BS-VTB erreicht. (T134) (Aenderung bzgl. der VTB in der FT (BS-VTB Erreicht)  beim Konfigurieren eines OGK nach ACT festgestellt.)",
		5,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Auftrags-Typ#",
			"#Einrichtungs-Typ# des OGK",
			"#Log. Einr.-Nr.# des OGK",
			"Phys. Einr.-Nr. des OGK",
			"", "", "", "", "",
		},
	},

	{
		0x0514,
		"Durch erfolgreichen Anlauf des MSC wurde die BS-VTB erreicht. (T135) (Aenderung bzgl. der VTB in der FT (BS-VTB Erreicht)  festgestellt)",
		2,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Auftrags-Typ#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x0515,
		"Nicht behebbare Datenverfaelschung, durch Audits erkannt. (T054) (Fehler in Checksum-Pruefung der Tabelle der ST-DB-  Herstellerparameter)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"Abgespeicherte Checksum (Sollwert)",
			"Ermittelte Checksum  (Istwert)",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0516,
		"Behebbare bzw. ignorierbare Datenverfaelschung, durch Audits erkannt. (T053) (Dateninkonsistenz in Anlaufbegr.-WS: Der in der Anlaufbegr.-WS  definierte KON-Prozess existiert nicht; es wird ein neuer  ACT-Auftrag gestellt)",
		7,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Einrichtungs-Typ# der Einrichtung, fuer die der ACT-Auftrag gestellt wird",
			"#Log. Einr.-Nr.# der Einrichtung, fuer die der ACT-Auftrag gestellt wird",
			"Ident-Nr. aktueller Prozess aus Anlaufbegr.-WS",
			"Phys. Einr.-Nr. aktueller Prozess aus Anlaufbegr.-WS",
			"Ident-Nr. Vorgaengerprozess aus Anlaufbegr.-WS",
			"Phys. Einr.-Nr. Vorgaengerprozess aus Anlaufbegr.-WS",
			"", "", "",
		},
	},

	{
		0x0517,
		"Behebbare bzw. ignorierbare Datenverfaelschung, durch Audits erkannt. (T053) (Kennzeichen KYKANL wiederholt in KOORDLISTE)",
		7,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Einrichtungs-Typ# der Einrichtung, fuer die KYKANL zusaetzlich in Anlaufbegr.-WS",
			"#Log. Einr.-Nr.# der Einrichtung, fuer die KYKANL zusaetzlich in Anlaufbegr.-WS",
			"Ident-Nr. aktueller Prozess aus Anlaufbegr.-WS",
			"Phys. Einr.-Nr. aktueller Prozess aus Anlaufbegr.-WS",
			"Ident-Nr. Vorgaengerprozess aus Anlaufbegr.-WS",
			"Phys. Einr.-Nr. Vorgaengerprozess aus Anlaufbegr.-WS",
			"", "", "",
		},
	},

	{
		0x0518,
		"Nicht behebbare Datenverfaelschung, durch Audits erkannt. (T054) (Unzulaessiger Return-Code vom OS-UP WTAKOM bei  Pufferanforderung fuer DKV-interne Signalisierung YKOAI)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code von WTAKOM#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0519,
		"Behebbare bzw. ignorierbare Datenverfaelschung, durch Audits erkannt. (T053) (Dateninkonsistenz in Anlaufbegr.-WS: Der in der Anlaufbegr.-WS  definierte KON-Prozess existiert nicht; es wird ein neuer  ACT-Auftrag gestellt)",
		7,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Einrichtungs-Typ# der Einrichtung, fuer die der ACT-Auftrag gestellt wird",
			"#Log. Einr.-Nr.# der Einrichtung, fuer die der ACT-Auftrag gestellt wird",
			"Ident-Nr. aktueller Prozess aus Anlaufbegr.-WS",
			"Phys. Einr.-Nr. aktueller Prozess aus Anlaufbegr.-WS",
			"Ident-Nr. Vorgaengerprozess aus Anlaufbegr.-WS",
			"Phys. Einr.-Nr. Vorgaengerprozess aus Anlaufbegr.-WS",
			"", "", "",
		},
	},

	{
		0x051a,
		"Nicht behebbare Datenverfaelschung, durch Audits erkannt. (T054) (Unzulaessiger Return-Code vom OS-UP WTAKOM bei  Pufferanforderung fuer DKV-interne Signalisierung YKOAI)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code von WTAKOM#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x051b,
		"Datenverfaelschung. (T003) (Prozessanstoss von unzulaessiger Einrichtung )",
		4,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Einrichtungs-Typ# aus der Signalisierung",
			"#Log. Einr.-Nr.# aus der Signalisierung",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x051c,
		"Datenverfaelschung in der DKV. (T001) (Prozessanstoss mit unzulaessigem Opcode )",
		5,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Ereignis-Typ#",
			"Ident-Nr. des Absenders",
			"#Opcode#",
			"eigene Ident-Nr",
			"", "", "", "", "",
		},
	},

	{
		0x051d,
		"Datenverfaelschung in der DKV. (T001) (Prozessanstoss mit unzulaessigem Ereignis-Typ)",
		4,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Ereignis-Typ#",
			"eigene Ident-Nr",
			"#Opcode#",
			"", "", "", "", "", "",
		},
	},

	{
		0x051e,
		"Datenverfaelschung in der DKV. (T001) ( nicht erwartete globale ST-Zustaende,   ST-Zustand (Einrichtung) /= MBL   bei PLA-Auftrag )",
		5,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Einrichtungs-Typ#",
			"#Log. Einr.-Nr.#",
			"#Globaler ST-Zustand# bei Auftragseingang",
			"eigene Ident-Nr",
			"", "", "", "", "",
		},
	},

	{
		0x051f,
		"Datenverfaelschung  im  Anlauf  mit der  angegebenen Einrichtung. (T044) ( Unzulaessiger Returncode VOM OS-UP WTAKOM bei   Pufferanforderung fuer die   DKV-interne Signalisierung YHOAI an die AUD )",
		4,
		{
			"Phys. Einr.-Nr der anlaufenden Einrichtung",
			"#Return-Code von WTAKOM#",
			"Ident-Nr. des Ziels",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x0520,
		"Datenverfaelschung  im  Anlauf  mit der  angegebenen Einrichtung. (T044) ( unzulaessiger Returncode vom OS-UP WTAKOM   bei Pufferanforderung fuer die DKV-interne   Signalisierung YKOAI )",
		5,
		{
			"Phys. Einr.-Nr. der anlaufenden Einrichtung",
			"#Return-Code von WTAKOM#",
			"#Einrichtungs-Typ#",
			"#Log. Einr.-Nr.#",
			"eigene Ident-Nr",
			"", "", "", "", "",
		},
	},

	{
		0x0521,
		"OSK-Umschaltung fuer das OSK-Paar, dessen eine phys. Einrichtungs-Nr. in Byte 0 angegeben ist, nicht moeglich. Vermutlich ist das Umschalterelais defekt oder stromlos. (T120) ( Fuer das OSK-Paar ist nur noch die SPK Funktion verfuegbar)",
		5,
		{
			"Phys. Einr.-Nr. der anlaufenden Einrichtung",
			"#Einrichtungs-Typ#",
			"#Log. Einr.-Nr.#",
			"#Log. Einr.-Nr.# des korrespondierenden OSK",
			"eigene Ident-Nr",
			"", "", "", "", "",
		},
	},

	{
		0x0522,
		"Der OSK in der Funktion des OGK meldet sich mit der Anlauf- aufforderung YAAS vor dem OSK in der Funktion des SPK. Beim letzten verfuegbaren OSK-Paar wird dem Umschaltewunsch nicht stattgegeben, sondern der OSK in der Funktion des SPK aufgefordert seinen Umschaltewunsch abzusenden. (T031) ( Anlauf des OSK's in der Funktion SPK wird nicht   unterstuetzt, da durch einen Funktionstausch kein   OSK(OGK) mehr verfuegbar waere ( letztes verfuegbares   OSK-Paar ) )",
		5,
		{
			"Phys. Einr.-Nr. der anlaufenden Einrichtung",
			"#Einrichtungs-Typ#",
			"#Log. Einr.-Nr.#",
			"#Log. Einr.-Nr.# des korrespondierenden OSK",
			"eigene Ident-Nr",
			"", "", "", "", "",
		},
	},

	{
		0x0523,
		"Der OSK in der Funktion des OGK meldet sich mit der Anlauf- aufforderung YAAS vor dem OSK in der Funktion des SPK. (T148) ( Funktionstausch veranlasst durch die Funktion SPK )",
		5,
		{
			"Phys. Einr.-Nr. der anlaufenden Einrichtung",
			"#Einrichtungs-Typ#",
			"#Log. Einr.-Nr.#",
			"#Log. Einr.-Nr.# des korrespondierenden OSK",
			"eigene Ident-Nr",
			"", "", "", "", "",
		},
	},

	{
		0x0524,
		"Datenverfaelschung  im  Anlauf  mit der  angegebenen Einrichtung. (T044) ( Unzulaessiger Returncode vom OS-UP WTAKOM bei   Pufferanforderung fuer die DKV-interne   Signalisierung  YKOAI )",
		5,
		{
			"Phys. Einr.-Nr. der anlaufenden Signalisierung",
			"#Return-Code von WTAKOM#",
			"#Einrichtungs-Typ#",
			"#Log. Einr.-Nr.#",
			"eigene Ident-Nr",
			"", "", "", "", "",
		},
	},

	{
		0x0525,
		"Datenverfaelschung  im  Anlauf  mit der  angegebenen Einrichtung. (T044) ( Unzulaessiger Returncode vom OS-UP WTAKOM bei   Pufferanforderung fuer DKV-interne   Signalisierung  YKWAI oder YKTAI  )",
		6,
		{
			"Phys. Einr.-Nr. der anlaufenden Einrichtung",
			"#Return-Code von WTAKOM#",
			"#Opcode#",
			"#Einrichtungs-Typ#",
			"#Log. Einr.-Nr.#",
			"eigene Ident-Nr",
			"", "", "", "",
		},
	},

	{
		0x0526,
		"Funktionstausch fuer OSK-Paar, dessen eine phys. Einrichtungs-Nr. in Byte 0 angegeben ist. (T121) ( Funktionstausch, verursacht durch Einrichtung im Zustand   MBL/UNA fuehrt zum AMBL/UNA-Auftrag fuer korrespondierenden   OSK, damit Zustand des Ports erhalten bleibt )",
		4,
		{
			"Phys. Einr.-Nr. der anlaufenden Einrichtung",
			"#Einrichtungs-Typ# gemeldete Funktion",
			"#Log. Einr.-Nr.# gemeldete Funktion",
			"eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x0527,
		"Datenverfaelschung  im  Anlauf  mit der  angegebenen Einrichtung. (T044) ( Unzulaessiger ST-Zustand ( Funktion die sich per YAAx   gemeldet hat ) )",
		6,
		{
			"Phys. Einr.-Nr. der anlaufenden Einrichtung",
			"#Einrichtungs-Typ#",
			"#Log. Einr.-Nr.#",
			"#Globaler ST-Zustand#",
			"#Detaillierter ST-Zustand#",
			"eigene Ident-Nr",
			"", "", "", "",
		},
	},

	{
		0x0528,
		"Datenverfaelschung  im  Anlauf  mit der  angegebenen Einrichtung. (T044) ( unzulaessiger ST-Zustand der Einrichtung, die sich nicht   per YAAx gemeldet hat beim Funktionstausch )",
		6,
		{
			"Phys. Einr.-Nr. der anlaufenden Einrichtung",
			"#Einrichtungs-Typ#",
			"#Log. Einr.-Nr.#",
			"#Globaler ST-Zustand#",
			"#Detaillierter ST-Zustand#",
			"eigene Ident-Nr",
			"", "", "", "",
		},
	},

	{
		0x0529,
		"Datenverfaelschung in der DKV. (T001) (falscher Einrichtungs-Typ bei Parameteruebergabe)",
		2,
		{
			"Phys. Einr.-Nr. der FDS",
			"empfangener #Einrichtungs-Typ#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x052a,
		"Datenverfaelschung in der DKV. (T001) (falsche FDS-Nummer bei Parameteruebergabe)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"eigene Ident-Nr",
			"fehlerhafte #Log. Einr.-Nr.#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x052b,
		"Datenverfaelschung in der DKV. (T001) (falsche FDS-Nummer bei Parameteruebergabe)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"eigene Ident-Nr",
			"fehlerhafte #Log. Einr.-Nr.#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x052c,
		"Datenverfaelschung in der DKV. (T001) (Unzulaessiger ST-Zustand des MSC  nach Eintreffen der MSC-Signalisierung STOK)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Globaler ST-Zustand# des MSC",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x052d,
		"Beziehungsverlust zum MSC. (T128) (ZZK-Ausfall-Beginn)",
		2,
		{
			"Phys. Einr.-Nr. der FDS",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x052e,
		"Verlust der BS-VTB. (T138) (ZZK-Ausfall fuehrte zum Verlust der BS-VTB)",
		2,
		{
			"Phys. Einr.-Nr. der FDS",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x052f,
		"Datenverfaelschung in der DKV. (T001) (unzulaessiger Return-Code vom OS-UP WTAKOM bei DKV-interner  Signalisierung YKCAI)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code von WTAKOM#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0530,
		"BS-VTB erreicht. (T136) (Das Erreichen der BS-VTB wurde erkannt, da ein SPK VT-benutzbar wurde)",
		2,
		{
			"Phys. Einr.-Nr. der FDS",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x0531,
		"Verlust der BS-VTB. (T138) (Der Verlust der VT-Benutzbarkeit eines SPK fuehrte zum Verlust der BS-VTB)",
		2,
		{
			"Phys. Einr.-Nr. der FDS",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x0532,
		"Datenverfaelschung in der DKV. (T001) (unzulaessiger Return-Code vom OS-UP WTAKOM bei Puffer-  anforderung fuer DKV-interne Signalisierung YKVAI )",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code von WTAKOM#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0533,
		"Behebbare Datenverfaelschung in der DKV. (T048) (Unplausibler ST-Zustand bei Initialisierung der KON)",
		4,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Globaler ST-Zustand#",
			"#Einrichtungs-Typ#",
			"#Log. Einr.-Nr.#",
			"", "", "", "", "", "",
		},
	},

	{
		0x0534,
		"Datenverfaelschung in der DKV. (T001) ( Unzulaessiger alter Konfigurations-Auftrag besteht )",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Opcode# (alter Auftrag /= ACT )",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0535,
		"Der OGK wurde nach UNA geschaltet; keine OSK-Umschaltung mehr moeglich, da der korrespondierende OSK nicht verfuegbar ist; aus dem BYTE 1 der Indizien ist ersichtlich, in welcher Funktion der OSK1 zum Fehlerzeitpunkt war; Wert 01: OGK, Wert 02: SPK. (T038) ( UNA-Auftrag fuer OGK fuehrt zum Verlust des OGK, da   korrespondierender OSK nicht verfuegbar (UNA/MBL),   d.h. Verschlechterung der VTB )",
		2,
		{
			"Phys. Einr.-Nr. aus der Signalisierung",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x0536,
		"Datenverfaelschung in der DKV. (T001) (Prozessanstoss mit unzulaessigem Ereignis)",
		6,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Ereignis-Typ#",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"eigene Ident-Nr",
			"#Opcode#",
			"Ident-Nr. aus Signalisierung",
			"", "", "", "",
		},
	},

	{
		0x0537,
		"Datenverfaelschung in der DKV. (T001) (Prozessanstoss mit unzulaessigem Ereignis)",
		5,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Ereignis-Typ#",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"eigene Ident-Nr",
			"#Opcode#",
			"", "", "", "", "",
		},
	},

	{
		0x0538,
		"Datenverfaelschung in der DKV. (T001) (unzulaessiger ST-Zustand des PHE in den Daten der DKV bei Eintreffen der Signalisierung YSTAE (PHE-Status-Meldung))",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Globaler ST-Zustand# des PHE",
			"#Detaillierter ST-Zustand# des PHE",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0539,
		"PHE-Zustaende unplausibel. (T137) (Nach einer Status-Meldung vom PHE sind die globalen ST-Zustaende der PHE laenger als 2 Min. unplausibel)",
		2,
		{
			"Phys. Einr.-Nr. (LODEAD)",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x053a,
		"MSC antwortet  nicht  bei Austausch der Sprechkreissperren. (T008)  (3. TO beim intern gestelltem YNZAI)",
		2,
		{
			"Phys. Einr.-Nr. der FDS",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x053b,
		"MSC antwortet  nicht  bei Austausch der Sprechkreissperren. (T008)  (3. TO beim Warten auf die 2. SSSAU/SNSAU)",
		2,
		{
			"Phys. Einr.-Nr. der FDS",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x053c,
		"Datenverfaelschung. (T003) (Prozessanstoss mit unzulaessigem Ereignis)",
		4,
		{
			"Phys. Einr.-Nr. der FDS",
			"eigene Ident-Nr",
			"#Ereignis-Typ#",
			"#Opcode#",
			"", "", "", "", "", "",
		},
	},

	{
		0x053d,
		"Datenverfaelschung in der DKV. (T001) (unzulaessiger Return-Code des MUK-UP YPNAA3)",
		2,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code MUK#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x053e,
		"Datenverfaelschung in der DKV. (T001)  (unzulaessiger Return-Code vom OS-UP WTAKOM bei  Pufferanforderung fuer DKV-interne Signalisierung YNLAI)",
		2,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code von WTAKOM#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x053f,
		"Datenverfaelschung in der DKV. (T001) (unzulaessiger Return-Code des MUK-UP YPNZ05)",
		2,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code MUK#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x0540,
		"Datenverfaelschung in der DKV. (T001) (unzulaessiger Return-Code des MUK-UP YPNZ05)",
		2,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code MUK#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x0541,
		"Datenverfaelschung in der DKV. (T001)  (unzulaessiger Return-Code vom OS-UP WTAKOM bei  Pufferanforderung fuer DKV-interne Signalisierung YKRAI)",
		2,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code von WTAKOM#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x0542,
		"Datenverfaelschung in der DKV. (T001)  (unzulaessiger Return-Code vom OS-UP WTAKOM bei  Pufferanforderung fuer DKV-interne Signalisierung YKRAI)",
		2,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code von WTAKOM#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x0543,
		"SW-Fehler in der DKV. (T002)  (Identnummer eines KON-Prozesses beim Absenden  der DKV-internen Signalisierung YKBAI nicht existent)",
		1,
		{
			"Phys. Einr.-Nr. der FDS",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x0544,
		"Datenverfaelschung bei der Datenuebertragung von der aktiven in die passive FDS im Anlauf der passiven FDS. (T059) (Wertebereich der Uhrdaten aus Signalisierung von ACTFDS falsch)",
		8,
		{
			"Phys. Einr.-Nr. der FDS",
			"Monatstag (BCD-Code)",
			"Wochentag (BCD-Code)",
			"Monat     (BCD-Code)",
			"Stunde    (BCD-Code)",
			"Minute    (BCD-Code)",
			"Sekunde   (BCD-Code)",
			"Jahreszahl (hexadezimal)",
			"", "",
		},
	},

	{
		0x0545,
		"Datenverfaelschung bei der Datenuebertragung von der aktiven in die passive FDS im Anlauf der passiven FDS. (T059) (Pruefsumme ueber Uhrdaten aus Signalisierung von ACTFDS falsch)",
		9,
		{
			"Phys. Einr.-Nr. der FDS",
			"Monatstag (BCD-Code)",
			"Wochentag (BCD-Code)",
			"Monat     (BCD-Code)",
			"Stunde    (BCD-Code)",
			"Minute    (BCD-Code)",
			"Sekunde   (BCD-Code)",
			"Jahreszahl (hexadezimal)",
			"Pruefsumme : #Entwickler-Info#",
			"",
		},
	},

	{
		0x0546,
		"Datenverfaelschung in der DKV. (T001) (Prozessanstoss mit unzulaessigem Ereignis-Typ)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Ereignis-Typ#",
			"#Opcode#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0547,
		"Die in Byte 1 angegebene Einrichtung quittiert Signalisierung der DKV nicht. (T141) (Wiederholter Time-out statt Uhrzeitquittung von der STBFDS)",
		2,
		{
			"Phys. Einr.-Nr. der FDS",
			"Phys. Einr.-Nr. der STBFDS",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x0548,
		"Datenverfaelschung in der DKV. (T001) (Checksum ueber den Stundenplan der Uhr ist verfaelscht)",
		1,
		{
			"Phys. Einr.-Nr. der FDS",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x0549,
		"Datenverfaelschung in der DKV. (T001) (Prozessanstoss mit unzulaessigem Ereignis-Typ)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Ereignis-Typ#",
			"#Opcode#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x054a,
		"Datenverfaelschung in der DKV. (T001) (Unzulaessiger Return-Code aus OS-UP WTAKOM bei Absenden einer DKV-internen Signalisierung an Audit)",
		2,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code von WTAKOM#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x054b,
		"Datenverfaelschung in der DKV. (T001) (Unzulaessiger Return-Code aus OS-UP WTAKOM bei Absenden einer DKV-internen Signalisierung an SPK-FEP)",
		2,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code von WTAKOM#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x054c,
		"Datenverfaelschung in der DKV. (T001) (Unzulaessiger Return-Code aus OS-UP WTAKOM bei Absenden einer DKV-internen Signalisierung an FME-FEP)",
		2,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code von WTAKOM#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x054d,
		"Datenverfaelschung in der DKV. (T001) (Unzulaessiger Return-Code aus OS-UP WTAKOM bei Absenden einer DKV-internen Signalisierung an OGK-FEP)",
		2,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code von WTAKOM#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x0551,
		"Fuer den angegebenen neuen OgK existiert kein Zwilling.",
		9,
		{
			"DKV-Nummer",
			"#Ident-Nummer#",
			"LOW-PC des letzten Prozesses",
			"HIGH-PC des letzten Prozesses",
			"Prozess-Status 1 des letzten Prozesses (LOW-TO)",
			"Prozess-Status 1 des letzten Prozesses (HIGH-TO)",
			"Prozess-Status 2 des letzten Prozesses",
			"Prozess-Status 3 des letzten Prozesses",
			"An das UP uebergebene #Phys. Einr.-Nr.# des neuen OgK",
			"",
		},
	},

	{
		0x0552,
		"Aus der Phys. Einr.-Nr. des neuen OgK ermittelte Phys. Einr.-Nr. des  alten OgK ist nicht als OgK eingetragen.",
		9,
		{
			"DKV-Nummer",
			"#Ident-Nummer#",
			"LOW-PC des letzten Prozesses",
			"HIGH-PC des letzten Prozesses",
			"Prozess-Status 1 des letzten Prozesses (LOW-TO)",
			"Prozess-Status 1 des letzten Prozesses (HIGH-TO)",
			"Prozess-Status 2 des letzten Prozesses",
			"Prozess-Status 3 des letzten Prozesses",
			"An das UP uebergebene #Phys. Einr.-Nr.# des neuen OgK",
			"",
		},
	},

	{
		0x0553,
		"Die angegebene Einrichtung wurde waehrend ihres Anlaufs nach UNA konfiguriert. (T045) (Der Anlaufkoordinatorprozess der STBFDS meldet kein  Anlaufende)",
		4,
		{
			"Phys. Einr.-Nr",
			"Anzahl der Anlaufversuche",
			"#Ereignis-Typ#",
			"Eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x0554,
		"Die angegebene Einrichtung wurde waehrend ihres Anlaufs nach UNA konfiguriert. (T045) (Die angegebene FUPEF-Einrichtung sendete 3 mal keine  Betriebsparameter-Quittung)",
		5,
		{
			"Phys. Einr.-Nr",
			"Anzahl der Anlaufversuche",
			"Prozesszustand (LODSTA): #Entwickler-Info#",
			"Anzahl der Anlaufwiederholungen wegen neg. BP-Quittung",
			"Eigene Ident-Nr",
			"", "", "", "", "",
		},
	},

	{
		0x0555,
		"Die angegebene Einrichtung wurde waehrend ihres Anlaufs nach UNA konfiguriert. (T045) (Angegebene FUPEF-Einrichtung sendete 3 mal negative  Betriebsparameter-Quittung oder Betriebsparameter-  Quittung kam zum falschen Zeitpunkt)",
		5,
		{
			"Phys. Einr.-Nr",
			"Anzahl der Anlaufversuche",
			"Prozesszustand (LODSTA): #Entwickler-Info#",
			"Anzahl der TO fuer BP-Quittung",
			"Eigene Ident-Nr",
			"", "", "", "", "",
		},
	},

	{
		0x0556,
		"Die angegebene Einrichtung wurde waehrend ihres Anlaufs nach UNA konfiguriert. (T045) (Angegebener SPK sendete 3 mal keine Tarifdaten-Quittung)",
		5,
		{
			"Phys. Einr.-Nr",
			"Anzahl der Anlaufversuche",
			"Prozesszustand (LODSTA): #Entwickler-Info#",
			"Anzahl der Anlaufwiederholungen wegen neg. TD-Quittung",
			"Eigene Ident-Nr",
			"", "", "", "", "",
		},
	},

	{
		0x0557,
		"Die angegebene Einrichtung wurde waehrend ihres Anlaufs nach UNA konfiguriert. (T045) (Angegebener SPK sendete 3 mal negative Tarifdaten-Quittung  oder Tarifdaten-Quittung kam zum falschen Zeitpunkt)",
		5,
		{
			"Phys. Einr.-Nr",
			"Anzahl der Anlaufversuche",
			"Prozesszustand (LODSTA): #Entwickler-Info#",
			"Anzahl der TO fuer TD-Quittung",
			"Eigene Ident-Nr",
			"", "", "", "", "",
		},
	},

	{
		0x0558,
		"Der angelaufene PBR quittiert, nach mehrmaliger Wiederholung, eine Signalisierung der DKV nicht. (T076) (PBR antwortet nicht auf Signalisierung YBFAV)",
		3,
		{
			"Phys. Einr.-Nr",
			"Timeout-Zaehler : #Entwickler-Info#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0559,
		"Der angegebene SAE konnte nicht nach ACT konfiguriert werden, da Zeitueberwachung abgelaufen. Die Einrichtung wurde nach UNA konfiguriert. (T131) (Timeout fuer ACT-Auftrag fuer SAE)",
		1,
		{
			"Phys. Einr.-Nr. (LODEAD)",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x055a,
		"Die BS-Anlaufzeit ist abgelaufen (17 Minuten),da die VTB-Voraussetzungen (bei Normal-BS) bzw. die Voraussetzungen zum Erfuellen der Bakenfunktion (Stand- alone-Bake) nicht erreicht wurden. (T156) (Timeout fuer ACT-Auftrag der BS. Eventuell auch Fehler in der Anlagenliste, z.B. OSK(OGK) wurde mit ACT und zugehoeriger OSK(SPK) mit PLA eingetragen)",
		2,
		{
			"Phys. Einr.-Nr. (LODEAD)",
			"#ANK-Situationsanzeige#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x055b,
		"Beziehungsausfall zum MSC; auf mehrfachen Versuch keine Antwort vom MSC mehr oder Ausfall des ZZK. (T035)  (UNA-Auftrag MSC bei TO nach YSTOK waehrend  des Wartens auf YSTAK / SWAU)",
		1,
		{
			"Phys. Einr.-Nr. des MSC",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x055c,
		"Beziehungsausfall zum MSC; auf mehrfachen Versuch keine Antwort vom MSC mehr oder Ausfall des ZZK. (T035)  (UNA-Auftrag MSC. Verschiedene Fehlerursachen in Byte 1   (Prozesszustand)  Byte 1 = 05 : 3.TO statt SSSQU waehrend eines Anlaufs  Byte 1 = 18 : TO statt SADAU )",
		2,
		{
			"Phys. Einr.-Nr. des MSC",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x055d,
		"Beziehungswiederkehr zum MSC. (T129)  (Eintreffen der MSC Anlaufmeldung SWAU. Der Prozess-  zustand zum Zeitpunkt des Eintreffens der Signalisierung  siehe Byte 1)",
		2,
		{
			"Phys. Einr.-Nr. der FDS",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x055e,
		"Verlust der BS-VTB. (T138) (Verlust der BS-VT-Bereitschaft nach Sperren der OGK-Sender)",
		2,
		{
			"Phys. Einr.-Nr. der FDS",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x055f,
		"BS-VTB erreicht. (T136) (BS-VT-Bereitschaft erreicht nach Freigabe der OGK-Sender)",
		2,
		{
			"Phys. Einr.-Nr. der FDS",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x0560,
		"HiF-Eintrag bei Beginn, Ende oder laenger andauernder Ueberlast",
		7,
		{
			"Physikalische Einrichtungsnummer der DKV",
			"Sperrgrad durch Rufblockunterbandlastmessung n/16 von 100% Sperre",
			"Sperrgrad durch zentrale Ueberlast n/16 von 100% Sperre",
			"Sperrgrad durch Ueberlast im zentralen Zeichenkanal n/16 von 100% Sperre",
			"Teilnehmerrestnummer (lowest 4 Bit) der ersten gesperrten Teilnehmergruppe",
			"Sperrgrad, der ueber die Funkschnittstelle verbreitet wird, n/16 von 100% Sperre",
			"Anteil neuer Teilnehmergeraete, die am Verkehrsangebot beteiligt sind, n/10 von 100% neuen Geraeten",
			"", "", "",
		},
	},

	{
		0x0561,
		"Datenverfaelschung in der DKV. (T001) ( Anzahl versorgter SPK-Parameter in der BS-DB groesser 95)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"Anzahl durch Parameter vorgeleistete SPK",
			"Anzahl OSK-Paare der BS",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0562,
		"Fehlerhafter Aufruf durch OS (mit falschem #Opcode#).",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Erwarteter #Opcode# (OTDRSU)",
			"Eingelangter #Opcode#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0563,
		"Fehlerhafter Aufruf durch OS (mit falschem #Ereignis-Typ#).",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Erwarteter #Ereignis-Typ# (MUELE)",
			"Eingelangter #Ereignis-Typ#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0564,
		"Fehlerhafter Aufruf durch OS (mit falschem #Opcode#).",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Erwarteter #Opcode# (OXTMRY)",
			"Eingelangter #Opcode#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0565,
		"Fehlerhafter Aufruf durch OS (mit falschem #Ereignis-Typ#).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"1.Moeglicher #Ereignis-Typ# (MZTO)",
			"2.Moeglicher #Ereignis-Typ# (MFKSO)",
			"3.Moeglicher #Ereignis-Typ# (MFKSS)",
			"tatsaechlich eingelangter #Ereignis-Typ#",
			"", "", "", "", "",
		},
	},

	{
		0x0566,
		"Fehlerhafter Aufruf durch OS (mit falschem #Ereignis-Typ#).",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Erwarteter #Ereignis-Typ# (MREADY)",
			"Eingelangter #Ereignis-Typ#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0567,
		"Fehlerhafter Aufruf durch OS (mit falschem #Ereignis-Typ#).",
		4,
		{
			"Phys. Einrichtungsnummer",
			"1. zulaessiger #Ereignis-Typ# (MZFTO)",
			"2. zulaessiger #Ereignis-Typ# (MZTO)",
			"tatsaechlich eingelangter #Ereignis-Typ#",
			"", "", "", "", "", "",
		},
	},

	{
		0x0568,
		"Fehlerhafter Aufruf durch OS (mit falschem #Opcode#).",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Erwarteter #Opcode# (OXTMII)",
			"Eingelangter #Opcode#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0569,
		"Fehlerhafter Aufruf durch OS (mit falschem #Ereignis-Typ#).",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Erwarteter #Ereignis-Typ# (MTASKO)",
			"Eingelangter #Ereignis-Typ#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x056a,
		"Fehlerhafter Aufruf durch OS (mit falschem #Ereignis-Typ#).",
		4,
		{
			"Phys. Einrichtungsnummer",
			"1.zulaessiger #Ereignis-Typ# (MREADY)",
			"2.zulaessiger #Ereignis-Typ# (MZTO)",
			"tatsaechlich eingelangter #Ereignis-Typ#",
			"", "", "", "", "", "",
		},
	},

	{
		0x056b,
		"Fehlerhafter Aufruf durch OS (mit falschem #Ereignis-Typ#).",
		4,
		{
			"Phys. Einrichtungsnummer",
			"1. moeglicher erwarteter #Ereignis-Typ# (MZTO)",
			"2. moeglicher erwarteter #Ereignis-Typ# (MZFTO)",
			"Tatsaechlich eingelangter #Ereignis-Typ#",
			"", "", "", "", "", "",
		},
	},

	{
		0x056c,
		"Fehlerhafter Aufruf durch OS (mit falschem #Ereignis-Typ#).",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Erwarteter #Ereignis-Typ# (MREADY)",
			"Eingelangter #Ereignis-Typ#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x056d,
		"Beginn einer lokalen Bedien-Session LOGIN",
		5,
		{
			"Phys. Einrichtungsnummer",
			"#Passwortstufe#",
			"Kommandoabsender = 0 (da nur PBR moeglich)",
			"Alarmanzeigen an PBR (0 = nein, 1 = ja)",
			"Systemmeldungen an MSC (0 = nein, 1 = ja)",
			"", "", "", "", "",
		},
	},

	{
		0x0570,
		"Ende einer Bedien-Session",
		2,
		{
			"Phys. Einrichtungsnummer",
			"#Logoff-Ursache# (KXIBED, KXITOP, KXILOK)",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x0571,
		"Datenfehler (bei Konsistenzpruefung der AVL)",
		3,
		{
			"Phys. Einrichtungsnummer",
			"#Entwickler-Info# (Kommandoidentifikation)",
			"#Ident-Nummer#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0572,
		"Datenfehler (bei Konsistenzpruefung der Kommando-Freigabe)",
		2,
		{
			"Phys. Einrichtungsnummer",
			"#Entwickler-Info# (falscher Wert) (zulaessig: KXIUNK, KXIBUF, KXIKBE, KXIKNO)",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x0573,
		"Datenfehler (bei Aufruf der Prozedur XPIPKG, falschen Wert der Kommando-Freigabe uebergeben)",
		2,
		{
			"Phys. Einrichtungsnummer",
			"#Entwickler-Info# (falscher Wert) (zulaessig: KXIUNK, KXIBUF, KXIKBE, KXIKNO)",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x0575,
		"Datenfehler (Anstoss mit unzulaessigem Ereignistyp oder Opcode)",
		3,
		{
			"Phys. Einrichtungsnummer",
			"#Ereignis-Typ#",
			"#Opcode#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0576,
		"Datenfehler (Anstoss mit unzulaessigem Ereignistyp oder Opcode)",
		3,
		{
			"Phys. Einrichtungsnummer",
			"#Ereignis-Typ#",
			"#Opcode#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0577,
		"Fehler beim Datenbasis-Transfer vom MSC zur DKV aufgetreten. (T145) ( Kein Zugriff auf BSSYF )",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Auftraggeberkennzeichen#",
			"Returncode vom MSC #Entwickler-Info#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0578,
		"Fehler beim Datenbasis-Transfer vom MSC zur DKV aufgetreten. (T145) (Datenverfaelschung oder unbekannter Returncode )",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"Prozedur-Identifikation des betreffenden Moduls : #Entwickler-Info#",
			"Returncode vom MSC",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0579,
		"Fehler beim Datenbasis-Transfer vom MSC zur DKV aufgetreten. (T145) (Die uebergebene Checksum stimmt nicht mit der errechneten ueberein. )",
		8,
		{
			"Phys. Einr.-Nr. der FDS",
			"Prozedur-Identifikation des betreffenden Moduls : #Entwickler-Info#",
			"LOW-Byte der errechneten Checksum",
			"MIDDLE-Byte der errechneten Checksum",
			"HIGH-Byte der errechneten Checksum",
			"LOW-Byte der uebergebenen Checksum",
			"MIDDLE-Byte der uebergebenen Checksum",
			"HIGH-Byte der uebergebenen Checksum",
			"", "",
		},
	},

	{
		0x057a,
		"Fehler beim Datenbasis-Transfer vom MSC zur DKV aufgetreten. (T145) (Der gespiegelte Auftrag stimmt nicht mit dem gestellten Auftrag ueberein. )",
		6,
		{
			"Phys. Einr.-Nr. der FDS",
			"Prozedur-Identifikation des betreffenden Moduls : #Entwickler-Info#",
			"LOW-Byte des gespiegelten DB-Blockdisplacements",
			"HIGH-Byte des gespiegelten DB-Blockdisplacements",
			"LOW-Byte der gespiegelten DB-Block-Laenge",
			"HIGH-Byte der gespiegelten DB-Block-Laenge",
			"", "", "", "",
		},
	},

	{
		0x057b,
		"Fehler beim Datenbasis-Transfer vom MSC zur DKV aufgetreten. (T145) (Die uebergebene Signalisierungsfolgenummer stimmt nicht mit der erwarteten ueberein. )",
		4,
		{
			"Phys. Einr.-Nr. der FDS",
			"Prozedur-Identifikation des betreffenden Moduls : #Entwickler-Info#",
			"erwartete Signalisierungsfolgenummer",
			"erhaltene Signalisierungsfolgenummer",
			"", "", "", "", "", "",
		},
	},

	{
		0x057c,
		"Die BS hat gueltige Tarifdaten erhalten. (Anzeige fuer Systemfehler rueckgesetzt)",
		1,
		{
			"Phys. Einrichtungsnummer",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x057d,
		"Prozess-Kommunikations-Quittung ausstaendig (keine Quittung der Pruefebene)",
		3,
		{
			"Phys. Einrichtungsnummer",
			"#Pruefmodus#",
			"Phys. Einrichtungsnummer der zu pruefenden Einrichtung",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x057e,
		"Datenfehler (Einrichtungstyp im Prozesspeicher verfaelscht)",
		2,
		{
			"Phys. Einrichtungsnummer",
			"Falscher #Einrichtungs-Typ#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x057f,
		"Datenfehler (Einrichtungsnummer in der Quittung der Pruefebene ungleich dem Prozesspeicher)",
		4,
		{
			"Phys. Einrichtungsnummer",
			"#Pruefmodus#",
			"Phys. Einrichtungsnummer der zu pruefenden Einrichtung",
			"Falsche Einrichtungsnummer",
			"", "", "", "", "", "",
		},
	},

	{
		0x0580,
		"Prozess-Kommunikations-Quittung ausstaendig (keine Quittung auf einen MBL-Auftrag an KON)",
		3,
		{
			"Phys. Einrichtungsnummer",
			"#Pruefmodus#",
			"Phys. Einrichtungsnummer der zu pruefenden Einrichtung",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0581,
		"Datenfehler (Einrichtungsnummer aus MBL-Quittung der KON ist ungleich dem Prozesspeicher)",
		4,
		{
			"Phys. Einrichtungsnummer",
			"#Pruefmodus#",
			"Phys. Einrichtungsnummer der zu pruefenden Einrichtung",
			"Falsche Einrichtungsnummer",
			"", "", "", "", "", "",
		},
	},

	{
		0x0582,
		"Datenfehler (#Return-Code KON# in MBL-Quittung ungleich KYKOK)",
		5,
		{
			"Phys. Einrichtungsnummer",
			"#Pruefmodus#",
			"Phys. Einrichtungsnummer der zu pruefenden Einrichtung",
			"#Return-Code KON#",
			"Betriebszustand der Einrichtung",
			"", "", "", "", "",
		},
	},

	{
		0x0583,
		"Datenfehler (Start des Prozesses mit falschem Ereignis-Typ oder falschem Opcode)",
		4,
		{
			"Phys. Einrichtungsnummer",
			"#Ereignis-Typ#",
			"#Ident-Nummer#",
			"#Opcode#",
			"", "", "", "", "", "",
		},
	},

	{
		0x0584,
		"Datenfehler (Start des Prozesses mit falschem Ereignis-Typ oder falschem Opcode)",
		4,
		{
			"Phys. Einrichtungsnummer",
			"#Ereignis-Typ#",
			"#Ident-Nummer#",
			"#Opcode#",
			"", "", "", "", "", "",
		},
	},

	{
		0x0585,
		"Prozess-Kommunikations-Quittung ausstaendig (Timeout der KON-Quittung eines ACT-Auftrags)",
		3,
		{
			"Phys. Einrichtungsnummer",
			"#Pruefmodus#",
			"Phys. Einrichtungsnummer der zu pruefenden Einrichtung",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0586,
		"Datenfehler (Einrichtungsnummer der KON-Quittung stimmt nicht mit dem Prozesspeicher ueberein)",
		4,
		{
			"Phys. Einrichtungsnummer",
			"#Pruefmodus#",
			"Phys. Einrichtungsnummer der zu pruefenden Einrichtung",
			"Falsche Einrichtungsnummer",
			"", "", "", "", "", "",
		},
	},

	{
		0x0587,
		"Konfiguration nicht erfolgreich durchgefuehrt (Returncode auf den ACT-Auftrag ungleich KYKOK)",
		5,
		{
			"Phys. Einrichtungsnummer",
			"#Pruefmodus#",
			"Phys. Einrichtungsnummer der zu pruefenden Einrichtung",
			"#Return-Code KON#",
			"#Globaler ST-Zustand# der Einrichtung",
			"", "", "", "", "",
		},
	},

	{
		0x0588,
		"Datenfehler (Start des Prozesses mit falschem Ereignis-Typ oder falschem Opcode)",
		4,
		{
			"Phys. Einrichtungsnummer",
			"#Ereignis-Typ#",
			"#Ident-Nummer#",
			"#Opcode#",
			"", "", "", "", "", "",
		},
	},

	{
		0x0589,
		"Prozess-Kommunikations-Quittung ausstaendig (keine Quittung der Pruefebene)",
		3,
		{
			"Phys. Einrichtungsnummer",
			"#Pruefmodus#",
			"Phys. Einrichtungsnummer der zu pruefenden Einrichtung",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x058a,
		"Datenfehler (#Einrichtungs-Typ# im Prozesspeicher verfaelscht)",
		2,
		{
			"Phys. Einrichtungsnummer",
			"Falscher #Einrichtungs-Typ#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x058b,
		"Datenfehler (Einrichtungsnummer in der Quittung der Pruefebene ungleich dem Prozesspeicher)",
		4,
		{
			"Phys. Einrichtungsnummer",
			"#Pruefmodus#",
			"Phys. Einrichtungsnummer der zu pruefenden Einrichtung",
			"Falsche Einrichtungsnummer",
			"", "", "", "", "", "",
		},
	},

	{
		0x058c,
		"Datenfehler (Start des Prozesses mit falschem Ereignis-Typ oder falschem Opcode)",
		4,
		{
			"Phys. Einrichtungsnummer",
			"#Ereignis-Typ#",
			"#Ident-Nummer#",
			"#Opcode#",
			"", "", "", "", "", "",
		},
	},

	{
		0x058d,
		"Datenfehler (Start des Prozesses mit falschem Ereignis-Typ oder falschem Opcode)",
		4,
		{
			"Phys. Einrichtungsnummer",
			"#Ereignis-Typ#",
			"#Ident-Nummer#",
			"#Opcode#",
			"", "", "", "", "", "",
		},
	},

	{
		0x058e,
		"Datenfehler (#Pruefmodus# im Prozesspeicher verfaelscht)",
		2,
		{
			"Phys. Einrichtungsnummer",
			"Falscher #Pruefmodus#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x0590,
		"Datenfehler (Start des Prozesses mit falschem Ereignis-Typ oder falschem Opcode)",
		4,
		{
			"Phys. Einrichtungsnummer",
			"#Ereignis-Typ#",
			"#Ident-Nummer#",
			"#Opcode#",
			"", "", "", "", "", "",
		},
	},

	{
		0x0591,
		"Datenfehler (#Pruefmodus# im Prozesspeicher verfaelscht)",
		2,
		{
			"Phys. Einrichtungsnummer",
			"Falscher #Pruefmodus#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x0592,
		"Unzulaessige Identnummer oder Task nicht mehr existent. (Ermittlung ueber NB-Tabelle)",
		3,
		{
			"Phys. Einrichtungsnummer",
			"#Ident-Nummer#",
			"#Log. Einr.-Nr.#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0593,
		"Unzulaessige Ident-.Nr. oder Task nicht mehr existent.",
		3,
		{
			"Phys. Einrichtungsnummer",
			"#Ident-Nummer#",
			"#FEP-Auftragsart#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0594,
		"Keine gueltige Ident-Nummer eingetragen.",
		2,
		{
			"Phys. Einrichtungsnummer",
			"#FEP-Auftragsart#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x0595,
		"Fuer die eingetragene Ident-Nummer ist kein Prozess aktiv.",
		2,
		{
			"Phys. Einrichtungsnummer",
			"#Ident-Nummer#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x0596,
		"Datenfehler. (Start des Prozesses mit falschem #Opcode# oder falschem #Ereignis-Typ#).",
		4,
		{
			"Phys. Einrichtungsnummer",
			"#Ereignis-Typ#",
			"#Ident-Nummer#",
			"#Opcode#",
			"", "", "", "", "", "",
		},
	},

	{
		0x0597,
		"Datenfehler. (Start des Prozesses mit falschem #Opcode#, Signalisierung vom PFG bzw. SPK haette eintreffen sollen).",
		4,
		{
			"Phys. Einrichtungsnummer",
			"#Ereignis-Typ#",
			"#Ident-Nummer#",
			"#Opcode#",
			"", "", "", "", "", "",
		},
	},

	{
		0x0598,
		"Datenfehler. (Start des Prozesses mit falschem #Opcode#, Signalisierung vom FME bzw. vom PFG haette eintreffen sollen).",
		4,
		{
			"Phys. Einrichtungsnummer",
			"#Ereignis-Typ#",
			"#Ident-Nummer#",
			"#Opcode#",
			"", "", "", "", "", "",
		},
	},

	{
		0x0599,
		"Datenfehler. (Start des Prozesses mit falschem #Opcode#, Signalisierung haette vom PFG eintreffen sollen).",
		4,
		{
			"Phys. Einrichtungsnummer",
			"#Ereignis-Typ#",
			"#Ident-Nummer#",
			"#Opcode#",
			"", "", "", "", "", "",
		},
	},

	{
		0x059b,
		"Datenfehler. (Aufruf mit falschem Opcode, Signalisierung vom PFG haette eintreffen sollen).",
		4,
		{
			"Phys. Einrichtungsnummer",
			"#Ereignis-Typ#",
			"#Ident-Nummer#",
			"#Opcode#",
			"", "", "", "", "", "",
		},
	},

	{
		0x059c,
		"Datenfehler. (Aufruf erfolgte mit falschem Opcode, Pruefquittungen vom PFG bzw. vom OGK haetten eintreffen sollen).",
		4,
		{
			"Phys. Einrichtungsnummer",
			"#Ereignis-Typ#",
			"#Ident-Nummer#",
			"#Opcode#",
			"", "", "", "", "", "",
		},
	},

	{
		0x059e,
		"Datenfehler. (Start des Prozesses mit falschem #Opcode#, Pruefergebnis von der SPK-Pruefung haette eintreffen sollen).",
		4,
		{
			"Phys. Einrichtungsnummer",
			"#Ereignis-Typ#",
			"#Ident-Nummer#",
			"#Opcode#",
			"", "", "", "", "", "",
		},
	},

	{
		0x059f,
		"Datenfehler. (Der Prozess zur Durchfuehrung der Pruefung quittiert den Pruefauftrag nicht.)",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Phys. Einrichtungsnummer der geprueften Einrichtung",
			"#Ident-Nummer#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x05a0,
		"Datenfehler. (Verfaelschung des Pruefbewertungs-Index in den Task-Lokalen Daten.)",
		6,
		{
			"Phys. Einrichtungsnummer",
			"#FEP-Auftragsart#",
			"#Ident-Nummer#",
			"#Einrichtungs-Typ#",
			"#Log. Einr.-Nr.#",
			"#Pruefbewertungs-Index#",
			"", "", "", "",
		},
	},

	{
		0x05a1,
		"Datenfehler. (Start des Prozesses mit falschem Ereignis-Typ oder falschem Opcode.)",
		4,
		{
			"Phys. Einrichtungsnummer",
			"#Ereignis-Typ#",
			"#Ident-Nummer#",
			"#Opcode#",
			"", "", "", "", "", "",
		},
	},

	{
		0x05a2,
		"Datenfehler. (Falscher #Pruefbewertungs-Index# fuer die Bewertung einer Funkeinrichtungspruefung.)",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Falscher #Pruefbewertungs-Index#",
			"#Ident-Nummer#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x05a3,
		"Datenfehler. (Byte 0 der Task-Lokalen Daten ist verfaelscht.)",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Byte 0 der Task-Lokalen Daten  #Entwickler-Info#",
			"#Ident-Nummer#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x05a4,
		"Bei der Funkeinrichtungspruefung wurde das PFG als defekt erkannt. (PFG-Eigenpruefungsfehler, Auftrags-Abweisung)",
		3,
		{
			"Phys. Einrichtungsnummer",
			"#Pruefergebnis-Nummer#",
			"#Pruefbewertungs-Index#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x05a5,
		"Bei der Funkeinrichtungspruefung wurde ein Fehler des PFG im Ablauf der Pruefung erkannt.",
		2,
		{
			"Phys. Einrichtungsnummer",
			"#Pruefergebnis-Nummer#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x05a6,
		"Bei der Funkeinrichtungspruefung wurde ein OSK als defekt erkannt. (Eine OSK-Umschaltung ist jedoch nicht moeglich, da der redundante OSK nicht verfuegbar ist.)",
		9,
		{
			"Phys. Einrichtungsnummer",
			"#Pruefergebnis-Nummer#",
			"Phys. Einrichtungsnummer des OSK in der SPK-Funktion",
			"Empfangsfeldstaerke des PFG oder Wert 0",
			"Jitterwert des PFG oder Wert 0",
			"Laufzeit des PFG oder Wert 0",
			"Empfangsfeldstaerke des Prueflings oder Wert 0",
			"Jitterwert des Prueflings oder Wert 0",
			"Laufzeit des Prueflings oder Wert 0",
			"",
		},
	},

	{
		0x05a7,
		"Bei der Funkeinrichtungspruefung wurde ein Fehler einer Einrichtung im Ablauf der Pruefung erkannt.",
		3,
		{
			"Phys. Einrichtungsnummer",
			"#Pruefergebnis-Nummer#",
			"#Pruefbewertungs-Index#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x05a8,
		"Datenfehler. (Ausbleiben der Quittung nach Veranlassung einer OSK-Umschaltung)",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Phys. Einrichtungsnummer des fehlerhaften OSK",
			"#Ident-Nummer#",
			"#Pruefergebnis-Nummer#",
			"Phys. Einrichtungsnummer  des OSK in der SPK-Funktion",
			"", "", "", "", "",
		},
	},

	{
		0x05a9,
		"Bei der Funkeinrichtungspruefung wurde ein OSK als defekt erkannt. Der OSK in der SPK-Funktion uebernimmt nach der OSK-Umschaltung nicht die OGK-Funktion.",
		4,
		{
			"Phys. Einrichtungsnummer",
			"#Globaler ST-Zustand# des OSK in der SPK-Funktion",
			"Phys. Einrichtungsnummer des fehlerhaft geprueften OSK",
			"#Pruefergebnis-Nummer#",
			"", "", "", "", "", "",
		},
	},

	{
		0x05aa,
		"Die Funkeinrichtungspruefung kann nicht gestartet werden, da eine OSK-Umschaltepruefung noch immer andauert.",
		3,
		{
			"Phys. Einrichtungsnummer",
			"#FEP-Auftragsart#",
			"#Einrichtungs-Typ#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x05ab,
		"Datenfehler. (Start des Prozesses mit falschem Ereignis-Typ oder falschem Opcode.)",
		4,
		{
			"Phys. Einrichtungsnummer",
			"#Ereignis-Typ#",
			"#Ident-Nummer#",
			"#Opcode#",
			"", "", "", "", "", "",
		},
	},

	{
		0x05ac,
		"Datenfehler. (Start des Prozesses mit falschem Ereignis-Typ oder falschem Opcode.)",
		4,
		{
			"Phys. Einrichtungsnummer",
			"#Ereignis-Typ#",
			"#Ident-Nummer#",
			"#Opcode#",
			"", "", "", "", "", "",
		},
	},

	{
		0x05ad,
		"Datenfehler. (Byte 0 der Task-Lokalen Daten ist verfaelscht.)",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Byte 0 der Task-Lokalen Daten  #Entwickler-Info#",
			"#Ident-Nummer#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x05ae,
		"Datenfehler. (Start des Prozesses mit falschem Ereignis-Typ oder falschem Opcode.)",
		4,
		{
			"Phys. Einrichtungsnummer",
			"#Ereignis-Typ#",
			"#Ident-Nummer#",
			"#Opcode#",
			"", "", "", "", "", "",
		},
	},

	{
		0x05af,
		"Es wurde kein aktiver SPK mehr fuer die Funkeinrichtungspruefung eines FME gefunden.",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Phys. Einrichtungsnummer des FME",
			"#Ident-Nummer#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x05b0,
		"Datenfehler. (Start des Prozesses mit falschem Ereignis-Typ oder falschem Opcode.)",
		4,
		{
			"Phys. Einrichtungsnummer",
			"#Ereignis-Typ#",
			"#Ident-Nummer#",
			"#Opcode#",
			"", "", "", "", "", "",
		},
	},

	{
		0x05b1,
		"Datenfehler. (Start des Prozesses mit falschem Ereignis-Typ oder falschem Opcode.)",
		4,
		{
			"Phys. Einrichtungsnummer",
			"#Ereignis-Typ#",
			"#Ident-Nummer#",
			"#Opcode#",
			"", "", "", "", "", "",
		},
	},

	{
		0x05b3,
		"Datenfehler. (Start des Prozesses mit falschem Ereignis-Typ oder falschem Opcode.)",
		4,
		{
			"Phys. Einrichtungsnummer",
			"#Ereignis-Typ#",
			"#Ident-Nummer#",
			"#Opcode#",
			"", "", "", "", "", "",
		},
	},

	{
		0x05b4,
		"Datenfehler. (Start des Prozesses mit falschem Ereignis-Typ.)",
		3,
		{
			"Phys. Einrichtungsnummer",
			"#Ereignis-Typ#",
			"#Ident-Nummer#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x05b5,
		"Datenfehler. (Start des Prozesses mit falschem Ereignis-Typ oder falschem Opcode.)",
		4,
		{
			"Phys. Einrichtungsnummer",
			"#Ereignis-Typ#",
			"#Ident-Nummer#",
			"#Opcode#",
			"", "", "", "", "", "",
		},
	},

	{
		0x05b7,
		"Datenfehler. (Start des Prozesses mit falschem Ereignis-Typ oder falschem Opcode.)",
		4,
		{
			"Phys. Einrichtungsnummer",
			"#Ereignis-Typ#",
			"#Ident-Nummer#",
			"#Opcode#",
			"", "", "", "", "", "",
		},
	},

	{
		0x05b8,
		"Datenfehler. (Start des Prozesses mit falschem Ereignis-Typ oder falschem Opcode.)",
		4,
		{
			"Phys. Einrichtungsnummer",
			"#Ereignis-Typ#",
			"#Ident-Nummer#",
			"#Opcode#",
			"", "", "", "", "", "",
		},
	},

	{
		0x05b9,
		"Datenfehler. (Start des Prozesses mit falschem Ereignis-Typ oder falschem Opcode.)",
		4,
		{
			"Phys. Einrichtungsnummer",
			"#Ereignis-Typ#",
			"#Ident-Nummer#",
			"#Opcode#",
			"", "", "", "", "", "",
		},
	},

	{
		0x05bb,
		"Datenfehler. (Start des Prozesses mit falschem Opcode.)",
		4,
		{
			"Phys. Einrichtungsnummer",
			"#Ereignis-Typ#",
			"#Ident-Nummer#",
			"#Opcode#",
			"", "", "", "", "", "",
		},
	},

	{
		0x05bc,
		"Datenfehler. (Falsche Parameter in der Auftragssignalisierung bzw. Pruefquittung.)",
		8,
		{
			"Phys. Einrichtungsnummer",
			"Phys. Einrichtungsnummer der zu pruefenden Einrichtung",
			"#Ident-Nummer#",
			"#Log. Einr.-Nr.#",
			"",
			"#Einrichtungs-Typ# in der Signalisierung",
			"#Log. Einr.-Nr.# in der Signalisierung",
			"Phys. Einrichtungsnummer in der Signalisierung",
			"", "",
		},
	},

	{
		0x05bd,
		"Nicht behebbare Datenverfaelschung, durch Audits erkannt. (T054) (Erlaubnis-Tabelle defekt. <Ermittelte Checksum stimmt nicht mit der abgespeicherten Checksum ueberein>)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"In der Erlaubnis-Tab. gespeicherte Checksum  (Istwert)",
			"Ermittelte Checksum   (Sollwert)",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x05be,
		"Datenverfaelschung in der DKV. (T001) (Unzulaessiger Returncode aus WTAKOM nach  Prozess-Start YHVAI)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"Eigene Ident-Nr",
			"#Return-Code von WTAKOM#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x05bf,
		"Datenfehler (Aufruf durch falschen #Opcode# oder falschen #Ereignis-Typ#)",
		4,
		{
			"Phys. Einrichtungsnummer",
			"#Ereignis-Typ#",
			"#Ident-Nummer#",
			"#Opcode#",
			"", "", "", "", "", "",
		},
	},

	{
		0x05c0,
		"Datenfehler (Aufruf durch falschen #Opcode# oder falschen #Ereignis-Typ#)",
		4,
		{
			"Phys. Einrichtungsnummer",
			"#Ereignis-Typ#",
			"#Ident-Nummer#",
			"#Opcode#",
			"", "", "", "", "", "",
		},
	},

	{
		0x05c1,
		"Datenfehler (Aufruf mit falschem #Ereignis-Typ# oder falschem #Opcode#)",
		4,
		{
			"Phys. Einrichtungsnummer",
			"#Ereignis-Typ#",
			"#Ident-Nummer#",
			"#Opcode#",
			"", "", "", "", "", "",
		},
	},

	{
		0x05c2,
		"Aktivieren PHE aufgrund O&M-Kommando",
		3,
		{
			"Phys. Einrichtungsnummer",
			"#Zeitbezug#",
			"#Return-Code an Betreiber#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x05c3,
		"Datenfehler. (Anstoss des Prozesses mit falschem #Ereignis-Typ#/#Opcode#).",
		4,
		{
			"Phys. Einrichtungsnummer",
			"#Ereignis-Typ#",
			"#Ident-Nummer#",
			"#Opcode#",
			"", "", "", "", "", "",
		},
	},

	{
		0x05c4,
		"Datenfehler. (Start des Prozesses mit falschem Ereignis-Typ oder falschem Opcode.)",
		4,
		{
			"Phys. Einrichtungsnummer",
			"#Ereignis-Typ#",
			"#Ident-Nummer#",
			"#Opcode#",
			"", "", "", "", "", "",
		},
	},

	{
		0x05c5,
		"Datenfehler. (Start des Prozesses mit ungueltigem Ereignis-Typ oder ungueltigem Opcode.)",
		4,
		{
			"Phys. Einrichtungsnummer",
			"#Ereignis-Typ#",
			"#Ident-Nummer#",
			"#Opcode#",
			"", "", "", "", "", "",
		},
	},

	{
		0x05c7,
		"Datenfehler (Aufruf durch falschen #Ereignis-Typ# oder falschen #Opcode#)",
		4,
		{
			"Phys. Einrichtungsnummer",
			"#Ereignis-Typ#",
			"#Ident-Nummer#",
			"#Opcode#",
			"", "", "", "", "", "",
		},
	},

	{
		0x05c8,
		"Datenfehler. (Aufruf durch falschen #Ereignis-Typ# oder falschen #Opcode#)",
		4,
		{
			"Phys. Einrichtungsnummer",
			"#Ereignis-Typ#",
			"#Ident-Nummer#",
			"#Opcode#",
			"", "", "", "", "", "",
		},
	},

	{
		0x05c9,
		"BS-Anlauf aufgrund Kommando \"Anlauf BS\" hat nicht statt- gefunden.",
		3,
		{
			"Phys. Einrichtungsnummer",
			"#Ereignis-Typ#",
			"#Ident-Nummer#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x05ca,
		"Datenfehler (Aufruf mit falschem #Ereignis-Typ# oder falschem #Opcode#)",
		4,
		{
			"Phys. Einrichtungsnummer",
			"#Ereignis-Typ#",
			"#Ident-Nummer#",
			"#Opcode#",
			"", "", "", "", "", "",
		},
	},

	{
		0x05cb,
		"Datenfehler. (Aufruf durch falschen #Ereignis-Typ# oder falschen #Opcode#)",
		4,
		{
			"Phys. Einrichtungsnummer",
			"#Ereignis-Typ#",
			"#Ident-Nummer#",
			"#Opcode#",
			"", "", "", "", "", "",
		},
	},

	{
		0x05cc,
		"Anlauf BS aufgrund O&M-Kommando durchfuehren",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Phys. Einrichtungsnummer PBR oder MSC",
			"#Return-Code an Betreiber#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x05ce,
		"Analuf BS aufgrund O&M-Kommando",
		2,
		{
			"Phys. Einrichtungsnummer",
			"#Return-Code an Betreiber#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x05d0,
		"Datenfehler. (Start des Prozesses mit falschem Ereignis-Typ oder falschem Opcode.)",
		4,
		{
			"Phys. Einrichtungsnummer",
			"#Ereignis-Typ#",
			"#Ident-Nummer#",
			"#Opcode#",
			"", "", "", "", "", "",
		},
	},

	{
		0x05d1,
		"Datenfehler. (Start des Prozesses mit falschem Ereignis-Typ oder falschem Opcode.)",
		4,
		{
			"Phys. Einrichtungsnummer",
			"#Ereignis-Typ#",
			"#Ident-Nummer#",
			"#Opcode#",
			"", "", "", "", "", "",
		},
	},

	{
		0x05d2,
		"Datenfehler. (Start des Prozesses mit falschem Ereignis-Typ oder falschem Opcode.)",
		4,
		{
			"Phys. Einrichtungsnummer",
			"#Ereignis-Typ#",
			"#Ident-Nummer#",
			"#Opcode#",
			"", "", "", "", "", "",
		},
	},

	{
		0x05d3,
		"Datenfehler. (Start des Prozesses mit falschem Ereignis-Typ oder falschem Opcode.)",
		4,
		{
			"Phys. Einrichtungsnummer",
			"#Ereignis-Typ#",
			"#Ident-Nummer#",
			"#Opcode#",
			"", "", "", "", "", "",
		},
	},

	{
		0x05d4,
		"Datenfehler. (Start des Prozesses mit falschem Ereignis-Typ oder falschem Opcode.)",
		4,
		{
			"Phys. Einrichtungsnummer",
			"#Ereignis-Typ#",
			"#Ident-Nummer#",
			"#Opcode#",
			"", "", "", "", "", "",
		},
	},

	{
		0x05d5,
		"Prozess-Kommunikations-Quittung ausstaendig. (Quittung der KON nicht eingetroffen.)",
		4,
		{
			"Phys. Einrichtungsnummer",
			"#Einrichtungs-Typ#",
			"#Log. Einr.-Nr.#",
			"#Zielzustand#",
			"", "", "", "", "", "",
		},
	},

	{
		0x05d6,
		"Datenfehler. (Einrichtungsnummer in der KON-Quittung stimmt nicht mit dem Prozesspeicher ueberein.)",
		5,
		{
			"Phys. Einrichtungsnummer",
			"#Einrichtungs-Typ# (aus Signalisierung)",
			"#Log. Einr.-Nr.# (aus Signalisierung)",
			"Phys. Einrichtungsnummer (aus Signalisierung)",
			"Phys. Einrichtungsnummer (aus Prozesspeicher)",
			"", "", "", "", "",
		},
	},

	{
		0x05d7,
		"Konfiguration ist nicht positiv durchgefuehrt. (Returncode in der Quittung ungleich KYKOK.)",
		5,
		{
			"Phys. Einrichtungsnummer",
			"#Einrichtungs-Typ# (aus Signalisierung)",
			"#Log. Einr.-Nr.# (aus Signalisierung)",
			"Phys. Einrichtungsnummer (aus Signalisierung)",
			"Kennzeichen aus der Quittung (#Return-Code KON#)",
			"", "", "", "", "",
		},
	},

	{
		0x05d8,
		"Datenfehler. (Byte 7 der Task-Lokalen Daten verfaelscht.)",
		2,
		{
			"Phys. Einrichtungsnummer",
			"#Entwickler-Info# (zulaessig: KYTKPA, KYTKTA, KYTKZA)",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x05d9,
		"Datenfehler. (Zustandsuebergang im Prozesspeicher ungueltig.)",
		2,
		{
			"Phys. Einrichtungsnummer",
			"Falscher Zustandsuebergang",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x05da,
		"Datenfehler. (Start des Prozesses mit falschem Ereignis-Typ oder falschem Opcode.)",
		4,
		{
			"Phys. Einrichtungsnummer",
			"#Ereignis-Typ#",
			"#Ident-Nummer#",
			"#Opcode#",
			"", "", "", "", "", "",
		},
	},

	{
		0x05db,
		"Datenfehler. (Checksum der Signalisierung ist falsch.)",
		9,
		{
			"Phys. Einrichtungsnummer",
			"Byte 0 der Signalisierung #Entwickler-Info#",
			"Byte 1 der Signalisierung #Entwickler-Info#",
			"Byte 2 der Signalisierung #Entwickler-Info#",
			"Byte 3 der Signalisierung #Entwickler-Info#",
			"Byte 4 der Signalisierung #Entwickler-Info#",
			"Byte 5 der Signalisierung #Entwickler-Info#",
			"Byte 6 der Signalisierung #Entwickler-Info#",
			"Byte 7 der Signalisierung #Entwickler-Info#",
			"",
		},
	},

	{
		0x05dc,
		"Datenfehler. (Byte 0 der Task-Lokalen Daten verfaelscht.)",
		5,
		{
			"Phys. Einrichtungsnummer",
			"#Ereignis-Typ#",
			"#Ident-Nummer#",
			"#Opcode#",
			"Byte 0 der Task-Lokalen Daten #Entwickler-Info#",
			"", "", "", "", "",
		},
	},

	{
		0x05dd,
		"Die Anwenderidentnummer aus \"Freigabemeldung-SPK\" ist nicht die Selbe wie in der SPK-Liste und fuer die Identnummer der SPK-Liste ist kein Prozess aktiv.",
		4,
		{
			"Physikalische Einrichtungs-Nummer der FDS",
			"Logische Einrichtungs-Nummer des SPK",
			"#Ident-Nummer# aus \"Freigabemeldung-SPK\"",
			"#Ident-Nummer# aus der SPK-Liste",
			"", "", "", "", "", "",
		},
	},

	{
		0x05de,
		"Bei der Plausibilitaetspruefung eines SPK wurde ein interner VT-Fehler festgestellt und korrigiert.",
		3,
		{
			"Physikalishe Einrichtungs-Nummer der FDS",
			"Logische Einrichtungs-Nummer des SPK",
			"Zustand der SPK-Liste (#BITSPK#)",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x05df,
		"Bei der Plausibilitaetspruefung der Aktivdatei-Daten eines eingebuchten Teilnehmers wurde ein Fehler erkannt und korrigiert.",
		8,
		{
			"Physikalische Einrichtungs-Nummer der FDS",
			"Nummer des eingebuchten Teilnehmers low  Byte",
			"Nummer des eingebuchten Teilnehmers high Byte",
			"Nationalitaet und MSC - Nummer des eingebuchten Teilnehmers",
			"#Zusatzdaten-Byte#",
			"#Verwaltungs-Byte#",
			"#Identnummer-Byte#",
			"#Prozessaktivitaet#",
			"", "",
		},
	},

	{
		0x05e0,
		"FEP-Einzelergebnis einer BS-Pruefung",
		9,
		{
			"Phys. Einrichtungsnummer",
			"#Pruefergebnis-Nummer#",
			"Empfangsfeldstaerke des PFG oder Wert 00",
			"Jitterwert des PFG oder Wert 00",
			"Laufzeit des PFG oder Wert 00",
			"Empfangsfeldstaerke des Prueflings oder Wert 00",
			"Jitterwert des Prueflings oder Wert 00",
			"Laufzeit des Prueflings oder Wert 00",
			"#Frequenz-Nummer#",
			"",
		},
	},

	{
		0x05e2,
		"FEP-Ergebnis einer Dauerpruefung, negatives Einzelergebnis",
		9,
		{
			"Phys. Einrichtungsnummer",
			"#Pruefergebnis-Nummer#",
			"Empfangsfeldstaerke des PFG oder Wert 00",
			"Jitterwert des PFG oder Wert 00",
			"Laufzeit des PFG oder Wert 00",
			"Empfangsfeldstaerke des Prueflings oder Wert 00",
			"Jitterwert des Prueflings oder Wert 00",
			"Laufzeit des Prueflings oder Wert 00",
			"#Frequenz-Nummer#",
			"",
		},
	},

	{
		0x05e3,
		"FEP-Ergebnis der Einzelpruefung",
		9,
		{
			"Phys. Einrichtungsnummer",
			"#Pruefergebnis-Nummer#",
			"Empfangsfeldstaerke des PFG oder Wert 00",
			"Jitterwert des PFG oder Wert 00",
			"Laufzeitmesswert des PFG oder Wert 00",
			"Empfangsfeldstaerke des Prueflings oder Wert 00",
			"Jitterwert des Prueflings oder Wert 00",
			"Laufzeitmesswert des Prueflings oder Wert 00",
			"#Frequenz-Nummer#",
			"",
		},
	},

	{
		0x05e4,
		"Bei der Funkeinrichtungspruefung wurde das PFG als defekt erkannt.",
		9,
		{
			"Phys. Einrichtungsnummer",
			"#Pruefbewertungs-Index#",
			"#Pruefergebnis-Nummer# (gilt nur fuer Byte 5,6,7,8)",
			"Phys. Einrichtungsnummer der 1. negativ geprueften Einrichtung",
			"#Pruefergebnis-Nummer# der 1. negativ geprueften Einrichtung",
			"Phys. Einrichtungsnummer einer negativ geprueften Einrichtung (oder 0FCH)",
			"Phys. Einrichtungsnummer einer negativ geprueften Einrichtung (oder 0FCH)",
			"Phys. Einrichtungsnummer einer negativ geprueften Einrichtung (oder 0FCH)",
			"Phys. Einrichtungsnummer einer negativ geprueften Einrichtung (oder 0FCH)",
			"",
		},
	},

	{
		0x05e5,
		"Bei der Funkeinrichtungspruefung wurde eine Einrichtung als defekt erkannt.",
		9,
		{
			"Phys. Einrichtungsnummer",
			"#Pruefergebnis-Nummer#",
			"Feldstaerke (Messwert des PFG)",
			"Jitter      (Messwert des PFG)",
			"Laufzeit    (Messwert des PFG)",
			"Feldstaerke (Messwert der Einrichtung)",
			"Jitter      (Messwert der Einrichtung)",
			"Laufzeit    (Messwert der Einrichtung)",
			"#Frequenz-Nummer# (nur bei OGK)",
			"",
		},
	},

	{
		0x05e6,
		"Die Einrichtung OGK, FME oder PFG antwortet nicht (Zaehler fuer Anzahl der Einzelsignalisierungen ist ueberschritten)",
		1,
		{
			"#Phys. Einr.-Nr.#",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x05e7,
		"Die Konsistenzpruefung einer Matrix in der FT ergibt fehlerhafte Daten (Matrix fuer die Zustaende der Funkschnittstellen-Sperren)",
		3,
		{
			"FDS-Nr",
			"ermittelte Checksum",
			"abgespeicherte Checksum",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x05e8,
		"Die Konsistenzpruefung ergibt ein fehlerhaftes Datum ('Warteschlangenzustand' FTFSWZ)",
		3,
		{
			"FDS-Nr",
			"#FTFSWZ#",
			"Komplement von #FTFSWZ#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x05e9,
		"Der PHE antwortet nicht (Zaehler fuer Anzahl der Einzelsignalisierungen ist abgelaufen)",
		1,
		{
			"#Phys. Einr.-Nr.#",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x05ea,
		"Interne Signalisierung (TVSAI) konnte nicht eingetragen werden",
		2,
		{
			"FDS-Nr",
			"#Return-Code von WTAKOM#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x05eb,
		"Datenverfaelschung in der DKV. (T001) ( Teile der BT-DB durch Checksum-Pruefung als verfaelscht   erkannt )",
		8,
		{
			"Phys. Einr.-Nr. der FDS",
			"Nummer der ueberprueften internen BT-Checksum #Entwickler-Info#",
			"Errechnete Checksum LOW-Byte",
			"Errechnete Checksum MIDDLE-Byte",
			"Errechnete Checksum HIGH-Byte",
			"Gespeicherte Checksum LOW-Byte",
			"Gespeicherte Checksum MIDDLE-Byte",
			"Gespeicherte Checksum HIGH-Byte",
			"", "",
		},
	},

	{
		0x05ec,
		"Datenfehler (Ruecksetzen des ICC-Kennzeichens nicht moeglich, da Auftragsrichtung falsch)",
		2,
		{
			"Phys. Einrichtungsnummer",
			"#Entwickler-Info# (falscher Wert, zulaessig: KXIPBR oder KXIMSC)",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x05ed,
		"Datenfehler (Kommando-Kennzeichen kann nicht rueckgesetzt werden, da Auftragsart nicht gueltig)",
		2,
		{
			"Phys. Einrichtungsnummer",
			"#Entwickler-Info#  (falscher Wert)",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x05ee,
		"Datenfehler (ein anderer Prozess als der, der das Kommando-Kennzeichen gesetzt hat, versucht das Kennzeichen rueckzusetzen, wird jedoch nicht ausgefuehrt)",
		4,
		{
			"Phys. Einrichtungsnummer",
			"#Entwickler-Info# (Kommandoidentifikation)",
			"#Ident-Nummer# (eingetragen in AVL)",
			"falscher Wert (#Ident-Nummer# des Prozeses)",
			"", "", "", "", "", "",
		},
	},

	{
		0x05ef,
		"falscher #Opcode#",
		2,
		{
			"Phys. Einrichtungsnummer",
			"#Opcode#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x05f0,
		"Falscher #Ereignis-Typ#",
		2,
		{
			"Phys. Einrichtungsnummer",
			"Falscher #Ereignis-Typ#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x05f1,
		"Datenfehler (Start des Prozesses mit falschem Ereignis-Typ oder falschem Opcode)",
		4,
		{
			"Phys. Einrichtungsnummer",
			"#Ereignis-Typ#",
			"#Ident-Nummer#",
			"#Opcode#",
			"", "", "", "", "", "",
		},
	},

	{
		0x05f2,
		"Datenfehler. (PFG quittiert fruehzeitig Pruefauftrag).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"#Ereignis-Typ#",
			"#Ident-Nummer#",
			"#Opcode#",
			"#Pruefergebnis-Nummer#",
			"", "", "", "", "",
		},
	},

	{
		0x05f3,
		"PHE-Suchlauf wurde nicht durchgefuehrt",
		2,
		{
			"Phys. Einrichtungsnummer",
			"#Zeitbezug#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x05f4,
		"Datenfehler (PHE quittiert den Suchlauf-Auftrag n-mal nicht)",
		3,
		{
			"Phys. Einrichtungsnummer",
			"#Zeitbezug#",
			"#Return-Code an Betreiber#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x05f5,
		"Datenfehler. (Anstoss des Prozesses mit falschem #Ereignis-Typ#/#Opcode#)",
		4,
		{
			"Phys. Einrichtungsnummer",
			"#Ereignis-Typ#",
			"#Ident-Nummer#",
			"#Opcode#",
			"", "", "", "", "", "",
		},
	},

	{
		0x05f6,
		"Datenfehler. (Anstoss des Prozesses mit falschem Ereignistyp).",
		3,
		{
			"Phys. Einrichtungsnummer",
			"#Ereignis-Typ#",
			"#Ident-Nummer#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x05f7,
		"Datenfehler. (Anstoss des Prozesses mit falschem Ereignistyp).",
		3,
		{
			"Phys. Einrichtungsnummer",
			"#Ereignis-Typ#",
			"#Ident-Nummer#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x05f9,
		"Datenfehler (Ausbleiben der Quittung ueber die Verfuegbarkeit der BS-DB)",
		4,
		{
			"Phys. Einrichtungsnummer",
			"#Ereignis-Typ#",
			"#Ident-Nummer#",
			"#Opcode#",
			"", "", "", "", "", "",
		},
	},

	{
		0x05fa,
		"Datenfehler (Checksumfehler bei Konsistenzpruefung der Einrichtungs= tabelle)",
		1,
		{
			"Phys. Einrichtungsnummer",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x05fb,
		"Falscher Opcode oder Ereignistyp beim Empfang einer Signalisierung vom PHE",
		5,
		{
			"FDS-Nr",
			"#Ereignis-Typ#",
			"FKS-Nummer",
			"#Ident-Nummer#",
			"#Opcode#",
			"", "", "", "", "",
		},
	},

	{
		0x05fc,
		"Datenverfaelschung in der DKV. (T001) (Unbekanntes Auftraggeberkennzeichen im Sicherungsfeld)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"Prozedur-Identifikation des betreffenden Moduls : #Entwickler-Info#",
			"Fehlerhaftes Auftraggeberkennzeichen #Entwickler-Info#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x05fd,
		"Datenverfaelschung in der DKV. (T001) (Prozesskommunikation zum Ladeprozess der BT-ALV  gestoert)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"Prozedur-Identifikation des betreffenden Moduls : #Entwickler-Info#",
			"Return-Code des OS: #Entwickler-Info#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x05ff,
		"Behebbare Datenverfaelschung in der DKV. (T048) (Keine Sperre in KON fuer SPK eingetragen, in der VT  dagegen ST-gesperrt; Einrichtung wird nach ACT  konfiguriert)",
		3,
		{
			"Phys. Einr.-Nr. des SPK",
			"#Log. Einr.-Nr.# des SPK",
			"#Return-Code FUPPAR+1 von ZMASKP#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0600,
		"Datenverfaelschung in der DKV. (T001) (SPK ist in der SPK-Liste nicht eingerichtet (keine  Kanalnummern vorhanden); Einrichtung wird nach UNA  konfiguriert)",
		1,
		{
			"Phys. Einr.-Nr. des SPK",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x0601,
		"Datenverfaelschung in der DKV. (T001) (Beim Absenden eines ACT-Auftrages fuer einen SPK  unzulaessiger Returncode aus WTAKOM nach  Ueberpruefen der Konsistenz zwischen ST- und VT-Daten)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code von WTAKOM#",
			"Eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0602,
		"Behebbare Datenverfaelschung in der DKV. (T048) (Anzahl SPK im Prozesspeicher groesser als 95)",
		2,
		{
			"Phys. Einr.-Nr. der FDS",
			"Anzahl SPK aus Prozesspeicher",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x0603,
		"Datenverfaelschung in der DKV. (T001) (Globaler ST-Zustand unzulaessig)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Globaler ST-Zustand# des SPK",
			"#Log. Einr.-Nr.# des SPK",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0604,
		"Behebbare Datenverfaelschung in der DKV. (T048) (SPK ist in der KON nicht ACT, jedoch in der VT nicht  gesperrt; Abhilfe durch Setzen der ST-Sperre in der VT)",
		4,
		{
			"Phys. Einr.-Nr. des SPK",
			"#Log. Einr.-Nr.# des SPK",
			"#Return-Code FUPPAR+1 von ZMASKP#",
			"#Globaler ST-Zustand# des SPK",
			"", "", "", "", "", "",
		},
	},

	{
		0x0605,
		"Datenverfaelschung in der DKV. (T001) (Unzulaessiger Returncode in FUPPAR+0 aus ZMASKP)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code FUPPAR+0 von ZMASKP#",
			"Eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0606,
		"Behebbare Datenverfaelschung in der DKV. (T048) (SPK ist in der KON mit einer VT-relevanten Sperre  gesperrt, in der VT dagegen nicht ST-gesperrt;  Einrichtung wird nach ACT konfiguriert)",
		4,
		{
			"Phys. Einr.-Nr. des SPK",
			"#Log. Einr.-Nr.# des SPK",
			"#Return-Code FUPPAR+1 von ZMASKP#",
			"Gesetzte Sperre in #BYKASK#",
			"", "", "", "", "", "",
		},
	},

	{
		0x0609,
		"Protokollierung des O&M-Kommandos KONFIGURIEREN BS-EINRICHTUNG",
		9,
		{
			"Phys. Einrichtungsnummer",
			"#Einrichtungs-Typ#",
			"#Jobcode#",
			"Einrichtungsnummer",
			"#Zielzustand#",
			"#Konfigurations-Bedingung#",
			"DB-Generation",
			"Anzahl permanenter Aenderungen",
			"#Return-Code an Betreiber#",
			"",
		},
	},

	{
		0x060a,
		"Datenverfaelschung bzw. SW-Fehler in der DKV. (T004) (Die parameterbeschreibenden Informationen aus der Datenbasis konnten in einem Aenderungs- auftrag nicht nachgeladen werden)",
		4,
		{
			"Phys. Einr.-Nr. der FDS",
			"Prozedur-Identifikation #Entwickler-Info#",
			"#Return-Code von XLQBM#",
			"",
			"", "", "", "", "", "",
		},
	},

	{
		0x060b,
		"Bearbeitung des O&M-Kommandos \"Aendern Parameter\". (T147) (Es traf kein neuer Teilauftrag beim Aendern von Parametern innerhalb der Zeitueberwachung in der BS ein)",
		4,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Jobcode#",
			"Kennzeichen fuer interne Auftragsbeendigung #Entwickler-Info#",
			"Abbruchkennzeichen Timeout #Entwickler-Info#",
			"", "", "", "", "", "",
		},
	},

	{
		0x060c,
		"SW-Fehler in der DKV. (T002) (Die Quittung vom Ladeprozess beim Nach- laden der parameterbeschreibenden Informationen traf nicht ein)",
		5,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Jobcode#",
			"Kennzeichen fuer interne Auftragsbeendigung #Entwickler-Info#",
			"Abbruchkennzeichen fehlende Quittung des Ladeprozesses #Entwickler-Info#",
			"",
			"", "", "", "", "",
		},
	},

	{
		0x060d,
		"Bearbeitung des O&M-Kommandos \"Aendern Parameter\". (T147) (Der BEGINN-Teilauftrag vom MSC mit Signalisierungs- inhalt wird protokolliert)",
		7,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Einrichtungs-Typ#",
			"#Jobcode#",
			"Einrichtungsnummer",
			"#IDN#",
			"BS-DB-Generation",
			"Anzahl permanenter Aenderungen der BS-DB",
			"", "", "",
		},
	},

	{
		0x060e,
		"Bearbeitung des O&M-Kommandos \"Aendern Parameter\". (T147) (Der BEGINN-Teilauftrag vom PBR mit Signalisierungs- inhalt wird protokolliert)",
		7,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Einrichtungs-Typ#",
			"#Jobcode#",
			"Einrichtungsnummer",
			"#IDN#",
			"BS-DB-Generation",
			"Anzahl permanenter Aenderungen der BS-DB",
			"", "", "",
		},
	},

	{
		0x060f,
		"Bearbeitung des O&M-Kommandos \"Aendern Parameter\". (T147) (Der Aenderungsteilauftrag mit Signalisierungsinhalt wird protokolliert)",
		8,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Einrichtungs-Typ#",
			"#Jobcode#",
			"Einrichtungsnummer",
			"#IDN#",
			"Parameterwert (LOW-Byte)",
			"Parameterwert (HIGH-Byte)",
			"Zusatzangabe #Entwickler-Info#",
			"", "",
		},
	},

	{
		0x0610,
		"Bearbeitung des O&M-Kommandos \"Aendern Parameter\". (T147) (Der Abbruch-Teilauftrag mit Signalisierungsinhalt wird protokolliert)",
		6,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Einrichtungs-Typ#",
			"#Jobcode#",
			"Einrichtungsnummer",
			"#IDN#",
			"#Return-Code an Betreiber#",
			"", "", "", "",
		},
	},

	{
		0x0611,
		"Bearbeitung des O&M-Kommandos \"Aendern Parameter\". (T147) (Der Inhalt des Ende-Teilauftrags vom MSC und der Returncode der BS wird protokolliert)",
		9,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Einrichtungs-Typ#",
			"#Jobcode#",
			"Einrichtungsnummer",
			"#IDN#",
			"Checksum des MSC (LOW - Byte) #Entwickler-Info#",
			"Checksum des MSC (MIDDLE - Byte) #Entwickler-Info#",
			"Checksum des MSC (HIGH - Byte) #Entwickler-Info#",
			"#Return-Code an Betreiber#",
			"",
		},
	},

	{
		0x0612,
		"Bearbeitung des O&M-Kommandos \"Aendern Parameter\". (T147) (Der Inhalt des ENDE-Teilauftrag vom PBR und der Return- code der BS wird protokolliert)",
		6,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Einrichtungs-Typ#",
			"#Jobcode#",
			"Einrichtungsnummer",
			"#IDN#",
			"#Return-Code an Betreiber#",
			"", "", "", "",
		},
	},

	{
		0x0613,
		"Datenverfaelschung bzw. SW-Fehler in der DKV. (T004) (Der Aenderungsstand der BSSYF stimmt nicht mit dem in der BS-DB ueberein)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"Prozedur-Identifikation #Entwickler-Info#",
			"Return-Code an Auftraggeber #Entwickler-Info#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0614,
		"Datenverfaelschung bzw. SW-Fehler in der DKV. (T004) (Die physik. Einr.-Adresse des SPK der die Quittung sendet, stimmt nicht mit der erwarteten ueberein: Der falsche SPK antwortet)",
		5,
		{
			"Phys. Einr.-Nr. der FDS",
			"Prozedur-Identifikation #Entwickler-Info#",
			"erhaltene Phys. Einr.-Nr",
			"erwartete Phys. Einr.-Nr",
			"",
			"", "", "", "", "",
		},
	},

	{
		0x0615,
		"Datenverfaelschung bei der Datenuebertragung von der aktiven in die passive FDS beim Aendern der BT-DB (T064) (Die beim Transfer der DB-Aenderungen an die STBFDS erhaltene Checksum ueber alle Signalisierungen stimmt nicht mit der errechneten ueberein)",
		8,
		{
			"Phys. Einr.-Nr. der FDS",
			"Prozedur-Identifikation #Entwickler-Info#",
			"erhaltene Checksum (LOW - Byte)",
			"erhaltene Checksum (MIDDLE - Byte)",
			"erhaltene Checksum (HIGH - Byte)",
			"errechnete Checksum (LOW - Byte)",
			"errechnete Checksum (MIDDLE - Byte)",
			"errechnete Checksum (HIGH - Byte)",
			"", "",
		},
	},

	{
		0x0616,
		"Bearbeitung des O&M-Kommandos \"Aendern Parameter\". (T147) (Die Beendigung eines Aenderungsauftrags durch den BT-ICC wird protokolliert)",
		4,
		{
			"Phys. Einr.-Nr. der FDS",
			"Auftraggeberkennzeichen (BT-ICC) #Entwickler-Info#",
			"Intern Datennummer (Pseudo-IDN) #Entwickler-Info#",
			"Kennzeichen fuer Abbruch durch BT-ICC #Entwickler-Info#",
			"", "", "", "", "", "",
		},
	},

	{
		0x0617,
		"Datenverfaelschung bzw. SW-Fehler in der DKV. (T004) (Die logische Einrichtungsnummer in der Eingangs-  schnittstelle ist unzulaessig)",
		2,
		{
			"Phys. Einr.-Nr. der FDS",
			"falsche #Log. Einr.-Nr.# des OGK",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x0618,
		"Datenverfaelschung in der DKV. (T001) (Die Parameterwertlaenge im Aenderungspuffer ist verfaelscht worden)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"Prozedur-Identifikation #Entwickler-Info#",
			"falsche Parameterwertlaenge #Entwickler-Info#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0619,
		"Datenverfaelschung in der DKV. (T001) (Das Checksumkennzeichen im Aenderungspuffer ist verfaelscht worden)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"Prozedur-Identifikation #Entwickler-Info#",
			"falsches Checksumkennzeichen #Entwickler-Info#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x061a,
		"Bearbeitung des O&M-Kommandos \"Aendern Parameter\". (T147) (Der Aenderungsauftrag wird wegen fehlender MSC-Ver- bindung abgebrochen)",
		4,
		{
			"Phys. Einr.-Nr. der FDS",
			"Einrichtungs-Typ #Entwickler-Info#",
			"Absenderkennzeichen (MSC/PBR) #Entwickler-Info#",
			"Kennzeichen Abbruch wegen fehlender MSC-Verbindung #Entwickler-Info#",
			"", "", "", "", "", "",
		},
	},

	{
		0x061b,
		"Datenverfaelschung im BS-Anlauf. (T055) (unzulaessiger Return-Code vom OS-UP WTAKOM bei Pufferanforderung fuer die DKV-interne Signalisierung ZADCI)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code von WTAKOM#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x061c,
		"Datenverfaelschung im BS-Anlauf. (T055) (unzulaessiger Return-Code vom OS-UP WTAKOM bei Pufferanforderung fuer die DKV-interne Signalisierung ZADCI)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code von WTAKOM#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x061d,
		"Datenverfaelschung im BS-Anlauf. (T055) (unzulaessiger Return-Code vom OS-UP WTAKOM bei Pufferanforderung fuer die DKV-interne Signalisierung ZADCI)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code von WTAKOM#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x061e,
		"Datenfehler. (Konsistenzpruefung der OGK-Zeitschlitztabelle)",
		2,
		{
			"Phys. Einrichtungsnummer",
			"#Entwickler-Info#  (falsche Checksum)",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x061f,
		"Datenverfaelschung in der DKV. (T001) (Prozessanstoss mit unzulaessigem Ereignistyp)",
		4,
		{
			"#Phys. Einr.-Nr.# der FDS",
			"#Ereignis-Typ#",
			"#Opcode#",
			"#Ident-Nummer# des eigenen Prozesses",
			"", "", "", "", "", "",
		},
	},

	{
		0x0620,
		"Datenverfaelschung in der DKV. (T001) (Fuer einen zu pruefenden OSK(OGK) existiert laut OS kein Zwilling)",
		2,
		{
			"#Phys. Einr.-Nr.# der FDS",
			"#Log. Einr.-Nr.# des OGK",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x0621,
		"Datenverfaelschung bzw. SW-Fehler in der DKV. (T004) (Time-out bei Warten auf Anstoss durch READY)",
		3,
		{
			"#Phys. Einr.-Nr.# der FDS",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"#Ereignis-Typ#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0622,
		"Behebbare Datenverfaelschung in der DKV. (T048) (Unerwarteter Ereignis-Typ oder Opcode bei Warten auf Ready- Anstoss)",
		3,
		{
			"#Phys. Einr.-Nr.# der FDS",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"#Ereignis-Typ#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0623,
		"Datenverfaelschung oder Quittungsverlust bei den Audits fuer periphere Einrichtungen. (T069) (Meldung der KON ueber OGK Anlauf nicht erhalten)",
		3,
		{
			"#Phys. Einr.-Nr.# der FDS",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"#Ereignis-Typ#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0624,
		"Der OSK-Funktionstausch bei der Umschaltepruefung wurde nicht durchgefuehrt oder der SPK war nach der Pruefung nicht aktiv. (T040) (Umschaltung wurde nicht durchgefuehrt und Time-out bei Warten auf Meldung der KON ueber OGK Anlauf)",
		2,
		{
			"#Phys. Einr.-Nr.# der FDS",
			"#Log. Einr.-Nr.# des OSK(OGK)",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x0625,
		"OSK-Umschaltung nicht moeglich; vermutlich ist das Umschalterelais defekt oder stromlos. Der OSK(SPK) wurde nach UNA konfiguriert. (T146) (Der OSK(SPK) wurde nach UNA konfiguriert)",
		2,
		{
			"#Phys. Einr.-Nr.# des SPK",
			"#Phys. Einr.-Nr.# des OGK",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x0626,
		"Der OSK-Funktionstausch bei der Umschaltepruefung wurde nicht durchgefuehrt oder der SPK war nach der Pruefung nicht aktiv. (T040) (Umschaltung hat nicht funktioniert, da der SPK inzwischen nicht mehr verfuegbar war)",
		3,
		{
			"#Phys. Einr.-Nr.# der FDS",
			"#Phys. Einr.-Nr.# des OGK",
			"#Phys. Einr.-Nr.# des SPK",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0627,
		"Datenverfaelschung in der DKV. (T001) (Undefinierter Prozess-Zustand bei Anstoss mit Time-out oder Ready)",
		3,
		{
			"#Phys. Einr.-Nr.# der FDS",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"#Ereignis-Typ#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0628,
		"Datenverfaelschung in der DKV. (T001) (Undefinierter Prozess-Zustand bei Anstoss mit Signalisierung)",
		5,
		{
			"#Phys. Einr.-Nr.# der FDS",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"#Ereignis-Typ#",
			"#Opcode#",
			"#Ident-Nummer# des Absenders",
			"", "", "", "", "",
		},
	},

	{
		0x0629,
		"Datenfehler (Herkunft des Auftrags beim Ruecksetzen des ICC-Kennzeichens ist mit der in der AVL eingetragenen nicht ident)",
		3,
		{
			"Phys. Einrichtungsnummer",
			"#Entwickler-Info# (Eingabe-Parameter)",
			"#Entwickler-Info# (abgelegtes Kennzeichen aus AVL)",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x062a,
		"Datenverfaelschung beim Konfigurieren des angegebenen PHE. (T103) (Prozessanstoss mit unzulaessigem Ereignis-Typ)",
		3,
		{
			"Phys. Einr.-Nr. (LODEAD)",
			"#Ereignis-Typ#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x062b,
		"Datenverfaelschung in der DKV. (T001)  (unzulaessiger Return-Code vom OS-UP WTAKOM bei  Pufferanforderung fuer DKV-interne Signalisierung YOIAI)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code von WTAKOM#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x062c,
		"Ein Teilnehmer mit der Nummer 0 versucht Einbuchung",
		8,
		{
			"Physikalische Einrichtungs-Nummer der FDS",
			"Frei",
			"#Ident-Nummer#",
			"Nummer des vom Verbindungsabbruch betroffenen Teilnehmers .low  Byte",
			"Nummer des vom Verbindungsabbruch betroffenen Teilnehmers .high Byte",
			"Nationalitaet und MSC - Nummer des betroffenen Teilnehmers",
			"Prozess-Adresse      high Byte",
			"Prozess-Adresse      low  Byte",
			"", "",
		},
	},

	{
		0x062d,
		"Bei der Plausibilitaetspruefung der Aktivdatei-Daten eines eingebuchten Teilnehmers als Folge einer Meldenegativ-Quittung wurde ein Fehler erkannt und korrigiert.",
		7,
		{
			"Physikalische Einrichtungs-Numer der FDS",
			"Nummer des eingebuchten Teilnehmers low  Byte",
			"Nummer des eingebuchten Teilnehmers high Byte",
			"Nationalitaet und MSC - Nummer des eingebuchten Teilnehmers",
			"#Zusatzdaten-Byte#",
			"#Verwaltungs-Byte#",
			"#Identnummer-Byte#",
			"", "", "",
		},
	},

	{
		0x062e,
		"Summe der Zaehlerinhalte von den Zaehlern fuer die Anzahl eingebuchter Teilnehmer je OGK-Frequenz ist groesser als 4096",
		2,
		{
			"Physikalische Einrichtungs-Nummer des FDS",
			"Frei",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x062f,
		"Summe der Zaehlerinhalte von den Zaehlern fuer die Anzahl eingebuchter Teilnehmer je OGK-Frequenz ist ungleich dem Zaehler fuer die Gesamtanzahl eingebuchter Teilnehmer",
		2,
		{
			"Physikalische Einrichtungs-Nummer der FDS",
			"Frei",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x0630,
		"Beim Ausbuchen eines Teilnehmers aus der Aktivdatei war der Zaehlerinhalt des Zaehlers fuer die Anzahl eingebuchter Teilnehmer der zugehoerigen OGK-Frequenz bereits 0.",
		2,
		{
			"Physikalische Einrichtungs-Nummer der FDS",
			"Verkuerzte OGK-Frequenznummer",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x0631,
		"Beim Eintrag eines Teilnehmers in die Aktivdatei war der Zaehlerinhalt des Zaehlers fuer die Gesamtanzahl eingebuchter Teilnehmer bereits 4096.",
		2,
		{
			"Physikalische Einrichtungs-Nummer der FDS",
			"Frei",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x0632,
		"Beim Ausbuchen eines Teilnehmers aus der Aktivdatei war der Zaehlerinhalt des Zaehler fuer die Gesamtanzahl eingebuchter Teilnehmer bereits 0.",
		2,
		{
			"Physikalische Einrichtungs-Nummer der FDS",
			"Frei",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x0633,
		"Der Inhalt eines Zaehlers fuer die Anzahl eingebuchter Teilnehmer je OGK-Frequenz stimmt nicht mit dem zugehoerigen Komplement ueberein.",
		7,
		{
			"Physikalische Einrichtungs-Nummer der FDS",
			"Anzahl eingebuchter Teilnehmer der OGK-Frequenz low  Byte",
			"Anzahl eingebuchter Teilnehmer der OGK-Frequenz high Byte",
			"Komplement low  Byte",
			"Komplement high Byte",
			"Adresse des OGK-Datensatzes low  Byte",
			"Adresse des OGK-Datensatzes high Byte",
			"", "", "",
		},
	},

	{
		0x0634,
		"Der Inhalt des Zaehlers fuer die Gesamtanzahl eingebuchter Teilnehmer stimmt nicht mit dem Komplement ueberein.",
		5,
		{
			"Physikalische Einrichtungs-Nummer der FDS",
			"Gesamtanzahl eingebuchter Teilnehmer low  Byte",
			"Gesamtanzahl eingebuchter Teilnehmer high Byte",
			"Komplement low  Byte",
			"Komplement high Byte",
			"", "", "", "", "",
		},
	},

	{
		0x0635,
		"Negatives FEP-Ergebnis einer SPK-Pruefung, die im Zuge einer FME-Pruefung durchgefuehrt wurde. Alle Indizien beziehen sich auf das SPK-Pruefergebnis.",
		8,
		{
			"Phys. Einrichtungsnummer",
			"#Pruefergebnis-Nummer#",
			"Empfangsfeldstaerke des PFG",
			"Jitterwert des PFG",
			"Laufzeit des PFG",
			"Empfangsfeldstaerke des Prueflings",
			"Jitterwert des Prueflings",
			"Laufzeit des Prueflings",
			"", "",
		},
	},

	{
		0x0636,
		"BS-VTB erreicht. (T136) (BS-VT-Bereitschaft erreicht nach Freigabe der OGK-Sender)",
		2,
		{
			"Phys. Einr.-Nr. der FDS",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x0637,
		"Zustandsaenderung bei den Funkschnittstellen-Sperren",
		5,
		{
			"FDS-Nr",
			"Anwender 0= VT, 1= FT, 2= KON1, 3=KON2, 4= KON3, 5= KON4, 6= KON5 7= MUK",
			"Sperrenzustaende aus Sicht des Anwenders, alter Wert bitcodiert, Auswertung siehe Indizienbyte 4",
			"Sperrenzustaende aus Sicht des Anwenders, neuer Wert bitcodiert, Auswertung siehe Indizienbyte 4",
			"Sperrenzustaende nach Bilanzierung ueber alle Anwender bitcodiert, Bit 0= Senden OGK3, Bit 1= Senden OGK2, Bit 2= Senden OGK1, Bit 3= Gehende Verbindung Bit 4= Einbuchen Bit x= 0: gesperrt = 1: erlaubt",
			"", "", "", "", "",
		},
	},

	{
		0x0638,
		"Datenverfaelschung beim Anstoss eines Konfigurations- auftrags fuer einen SPK. (T096) (Der Zaehler fuer SPK's im ST-Zustand UNA befindet  sich in einem undefinierten Zustand, d. h. der Wert  ist groesser als die Maximalzahl SPK)",
		7,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Globaler ST-Zustand# des zu konfigurierenden SPK",
			"#Detaillierter ST-Zustand# des zu konfigurierenden SPK",
			"#Opcode# des ACT-Auftrags",
			"#Einrichtungs-Typ# des zu konfigurierenden SPK",
			"#Log. Einr.-Nr.# des zu konfigurierenden SPK",
			"eigene Ident-Nr",
			"", "", "",
		},
	},

	{
		0x0639,
		"Datenverfaelschung beim Konfigurieren des PBR nach ACT. (T085) (Unzulaessiger Return-Code vom OS-UP WTAKOM bei  Pufferanforderung fuer DKV-interne Signalsierung  YUBAI zur Information ueber PBR-Ausfall)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code von WTAKOM#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x063a,
		"Der Zaehler fuer die Anzahl der fuer die VT insgesamt verfuegbaren SPK (Normalkanaele und Erweiterungskanaele) wurde bei der Routinepruefung korrigiert.",
		8,
		{
			"Physikalische Einrichtungs-Nummer der FDS",
			"Anzahl der verfuegbaren SPK (NK und EK)",
			"ungueltig",
			"ungueltig",
			"ungueltig",
			"ungueltig",
			"ungueltig",
			"ungueltig",
			"", "",
		},
	},

	{
		0x063b,
		"Selbstaendiger Abbruch der Dauerpruefung",
		2,
		{
			"Phys. Einrichtungsnummer",
			"#Pruefergebnis-Nummer#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x063c,
		"Im BS-Anlauf mit Laden der Datenbasis vom MSC erhaelt die BS keine Quittung auf die Anforderung des ersten Datenblockes. (T149)",
		1,
		{
			"Phys. Einr.-Nr. der FDS",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x063d,
		"Das Laden der gesamten oder das partielle Nachladen der Datenbasis vom MSC, das durch den durch das 2. Indizienbyte gekennzeichneten Auftraggeber initiiert wurde, konnte wegen fehlender Verbindung zum MSC nicht durchgefuehrt werden. (T150)",
		2,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Auftraggeberkennzeichen#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x063e,
		"Das Laden der gesamten oder das partielle Nachladen der Datenbasis vom MSC, das durch den durch das Byte 1 gekennzeichneten Auftraggeber initiiert wurde, konnte wegen mehrmaligem fehlerhaften Datenblocktransfer oder fehlender Auftragsquittung nicht durchgefuehrt werden. (T151)",
		2,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Auftraggeberkennzeichen#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x063f,
		"Das Laden der gesamten oder das partielle Nachladen der Datenbasis vom MSC, das durch den durch das 2. Indizienbyte gekennzeichneten Auftraggeber initiiert wurde, konnte wegen Unstimmigkeiten zwischen uebergebener und intern errechneter Pruefsumme nicht durchgefuehrt werden. (T152)",
		8,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Auftraggeberkennzeichen#",
			"LOW-Byte der intern errechneten Pruefsumme",
			"MIDDLE-Byte der intern errechneten Pruefsumme",
			"HIGH-Byte der intern errechneten Pruefsumme",
			"LOW-Byte der uebergebenen Pruefsumme",
			"MIDDLE-Byte der uebergebenen Pruefsumme",
			"HIGH-Byte der uebergebenen Pruefsumme",
			"", "",
		},
	},

	{
		0x0640,
		"Das Laden der gesamten oder das partielle Nachladen der Datenbasis vom MSC, das durch den durch das Byte 1 gekennzeichneten Auftraggeber initiiert wurde, konnte wegen fehlender Verbindung zum MSC nicht durchgefuehrt werden. (T150)",
		2,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Auftraggeberkennzeichen#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x0641,
		"Das Laden der gesamten oder das partielle Nachladen der Datenbasis vom MSC, das durch den durch das Byte 1 gekennzeichneten Auftraggeber initiiert wurde, konnte wegen mehrmaligem fehlerhaften Datenblocktransfer oder fehlender Auftragsquittung nicht durchgefuehrt werden. (T151)",
		2,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Auftraggeberkennzeichen#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x0642,
		"Versionsunvertraeglichkeit zwischen DKV-Software und Datenbasis. (T144)",
		5,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Auftraggeberkennzeichen#",
			"1. Byte des fehlerhaften Versionskennzeichens",
			"2. Byte des fehlerhaften Versionskennzeichens",
			"Ort des fehlerhaften Versionskennzeichens",
			"", "", "", "", "",
		},
	},

	{
		0x0643,
		"Das Laden der gesamten oder das partielle Nachladen der Datenbasis vom MSC, das durch den durch das Byte 1 gekennzeichneten Auftraggeber initiiert wurde, konnte wegen Unstimmigkeiten zwischen uebergebener und intern errechneter Pruefsumme nicht durchgefuehrt werden. (T152)",
		8,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Auftraggeberkennzeichen#",
			"LOW-Byte der intern errechneten Pruefsumme",
			"MIDDLE-Byte der intern errechneten Pruefsumme",
			"HIGH-Byte der intern errechneten Pruefsumme",
			"LOW-Byte der uebergebenen Pruefsumme",
			"MIDDLE-Byte der uebergebenen Pruefsumme",
			"HIGH-Byte der uebergebenen Pruefsumme",
			"", "",
		},
	},

	{
		0x0644,
		"Datenverfaelschung beim Laden der Datenbasis. (T143) (Die waehrend des Ladens ermittelte Checksum stimmt nicht mit der in der Datenbasis befindlichen Checksum ueberein.)",
		9,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Auftraggeberkennzeichen#",
			"Prozedur-Identifikation des betreffenden Moduls : #Entwickler-Info#",
			"LOW-Byte der errechneten Checksum",
			"MIDDLE-Byte der errechneten Checksum",
			"HIGH-Byte der errechneten Checksum",
			"LOW-Byte der Checksum in der Datenbasis",
			"MIDDLE-Byte der Checksum in der Datenbasis",
			"HIGH-Byte der Checksum in der Datenbasis",
			"",
		},
	},

	{
		0x0645,
		"Fehler beim Datenbasis-Transfer vom MSC zur DKV aufgetreten. (T145) ( Kein Zugriff auf BSSYF )",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Auftraggeberkennzeichen#",
			"Returncode vom MSC #Entwickler-Info#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0646,
		"Datenverfaelschung in der DKV. (T001) (Prozessanstoss mit unzulaessigem Ereignis-Typ)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"Prozesszustand (LODSTA) #Entwickler-Info#",
			"#Ereignis-Typ#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0647,
		"Datenverfaelschung in der DKV. (T001) (Prozessanstoss mit unzulaessigem Ereignis-Typ)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"Prozesszustand (LODSTA) #Entwickler-Info#",
			"#Ereignis-Typ#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0648,
		"Datenverfaelschung in der DKV. (T001) (Unerwartete Signalisierung)",
		5,
		{
			"Phys. Einr.-Nr. der FDS",
			"Prozesszustand (LODSTA) #Entwickler-Info#",
			"#Ereignis-Typ#",
			"#Opcode#",
			"Ident-Nr. des Absenders",
			"", "", "", "", "",
		},
	},

	{
		0x0649,
		"Datenverfaelschung in der DKV. (T001) (Prozesszustand undefiniert, unzulaessiger Prozessanstoss durch Timeout oder Ready)",
		4,
		{
			"Phys. Einr.-Nr. der FDS",
			"Prozesszustand (LODSTA) #Entwickler-Info#",
			"eigene Ident-Nr",
			"#Ereignis-Typ#",
			"", "", "", "", "", "",
		},
	},

	{
		0x064a,
		"Datenverfaelschung in der DKV. (T001) (Prozesszustand undefiniert, unzulaessiger Prozessanstoss)",
		6,
		{
			"Phys. Einr.-Nr. der FDS",
			"Prozesszustand (LODSTA) #Entwickler-Info#",
			"eigene Ident-Nr",
			"#Ereignis-Typ#",
			"#Opcode#",
			"Ident-Nr. des Absenders",
			"", "", "", "",
		},
	},

	{
		0x064b,
		"Versionsunvertraeglichkeit zwischen DKV-Software und Datenbasis. (T144)",
		5,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Auftraggeberkennzeichen#",
			"1. Byte des fehlerhaften Versionskennzeichens",
			"2. Byte des fehlerhaften Versionskennzeichens",
			"Ort des fehlerhaften Versionskennzeichens #Entwickler-Info#",
			"", "", "", "", "",
		},
	},

	{
		0x064c,
		"Versionsunvertraeglichkeit zwischen DKV-Software und Datenbasis. (T144)",
		5,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Auftraggeberkennzeichen#",
			"1. Byte des fehlerhaften Versionskennzeichens",
			"2. Byte des fehlerhaften Versionskennzeichens",
			"Ort des fehlerhaften Versionskennzeichens #Entwickler-Info#",
			"", "", "", "", "",
		},
	},

	{
		0x064d,
		"OSK-Umschaltung aufgrund einer zyklischen Funkeinrichtungspruefung",
		6,
		{
			"Phys. Einrichtungsnummer",
			"#Ereignis-Typ#",
			"#Ident-Nummer#",
			"Phys. Einrichtungsnummer des OSK in der OGK-Funktion",
			"Phys. Einrichtungsnummer des OSK in der SPK-Funktion",
			"#Pruefergebnis-Nummer#",
			"", "", "", "",
		},
	},

	{
		0x064e,
		"Behebbare Datenverfaelschung in der DKV. (T048) (Dateninkonsistenz in der SPK-Liste der VT;  Einrichtung wird nach ACT konfiguriert)",
		4,
		{
			"#Phys. Einr.-Nr.# des SPK",
			"#Log. Einr.-Nr.# des SPK",
			"#Return-Code FUPPAR+0 von ZMASKP#",
			"#Return-Code FUPPAR+1 von ZMASKP#",
			"", "", "", "", "", "",
		},
	},

	{
		0x064f,
		"Datenverfaelschung in der DKV. (T001) (Fuer einen zu pruefenden OSK(OGK) existiert laut OS kein Zwilling)",
		2,
		{
			"#Phys. Einr.-Nr.# der FDS",
			"#Log. Einr.-Nr.# des OGK",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x0650,
		"Datenverfaelschung bei der internen Prozesskommunikation (T066) (Unzulaessiger Return-Code vom OS-UP WTAKOM bei  Pufferanforderung YUBAI )",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code von WTAKOM#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0651,
		"Datenverfaelschung in der DKV. (T001) (Prozessanstoss mit unzulaessigem Ereignis-Typ)",
		3,
		{
			"#Phys. Einr.-Nr.# der FDS",
			"#Ereignis-Typ#",
			"#Opcode#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0652,
		"Datenfehler. (Anstoss mit unzulaessigem Ereignis-Typ oder Opcode)",
		3,
		{
			"Phys. Einrichtungsnummer",
			"#Ereignis-Typ#",
			"#Opcode#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0653,
		"Ende der Bedien-Session nach LOGOFF-Anforderung an PBR ( Keine Quittung vom PBR )",
		1,
		{
			"Phys. Einrichtungsnummer",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x0654,
		"Datenverfaelschung in der DKV. (T001) (Unzulaessiger Anstoss des Audit-Prozesses HDV im Prozesszustand KYHZPR)",
		4,
		{
			"Phys. Einr.-Nr. der FDS",
			"Eigene Ident-Nr",
			"#Ereignis-Typ#",
			"#Opcode#",
			"", "", "", "", "", "",
		},
	},

	{
		0x0655,
		"Datenverfaelschung in der DKV. (T001) (Unzulaessiger Anstoss des Audit-Prozesses HDV im Prozesszustand KYHZYA)",
		4,
		{
			"Phys. Einr.-Nr. der FDS",
			"Eigene Ident-Nr",
			"#Ereignis-Typ#",
			"#Opcode#",
			"", "", "", "", "", "",
		},
	},

	{
		0x0656,
		"Datenverfaelschung in der DKV. (T001) (Unzulaessiger Anstoss des Audit-Prozesses HDV im Prozesszustand KYHZYP)",
		4,
		{
			"Phys. Einr.-Nr. der FDS",
			"Eigene Ident-Nr",
			"#Ereignis-Typ#",
			"#Opcode#",
			"", "", "", "", "", "",
		},
	},

	{
		0x0657,
		"Datenverfaelschung in der DKV. (T001) (Undefinierter Prozesszustand)",
		5,
		{
			"#Phys. Einr.-Nr.# der FDS",
			"Eigene Ident-Nr",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"#Ereignis-Typ#",
			"#Opcode#",
			"", "", "", "", "",
		},
	},

	{
		0x0658,
		"Datenverfaelschung in der DKV. (T001) (Unzulaessiger Return-Code von WTAKOM bei Puffer-Anforderung fuer OYKKQI)",
		3,
		{
			"#Phys. Einr.-Nr.# der FDS",
			"#Return-Code von WTAKOM#",
			"#Ident-Nummer# des eigenen Prozesses",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0659,
		"WSV-Datenfehler: Beim Eintrag einer gehenden Verbindung in das Vorhof-Wartefeld war die Anzahl der Vorhof - Eintraege bereits auf Maximalwert.",
		1,
		{
			"Physikalische Einrichtungsnummer der FDS",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x065a,
		"WSV-Datenfehler: Im Vorhof fuer gehende Verbindungen sind alle Plaetze belegt, die Anzahl der Vorhofeintraege ist jedoch ungleich der Anzahl eingerichteter Vorhof- plaetze.",
		2,
		{
			"Physikalische Einrichtungsnummer der FDS",
			"Anzahl der Eintraege im Vorhof fuer gehende Verbindungen",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x065b,
		"FEP-Ergebnis einer Dauerpruefung Weitere Indizien sind in den Systemmeldungen HYFBNG bzw. HYFBPO enthalten.",
		6,
		{
			"Phys. Einrichtungsnummer",
			"#Pruefergebnis-Nummer#",
			"Anzahl Pruefungen  unteres Byte",
			"oberes Byte",
			"Anzahl negative Pruefungen unteres Byte",
			"oberes Byte",
			"", "", "", "",
		},
	},

	{
		0x065c,
		"Letztes negatives Pruefergebnis einer Dauerpruefung Weitere Indizien sind in den Systemmeldungen HYFBER bzw. HYFBPO enthalten.",
		9,
		{
			"Phys. Einrichtungsnummer",
			"#Pruefergebnis-Nummer#",
			"Empfangsfeldstaerke des PFG oder Wert 0",
			"Jitterwert des PFG oder Wert 0",
			"Laufzeit des PFG oder Wert 0",
			"Empfangsfeldstaerke des Prueflings oder Wert 0",
			"Jitterwert des Prueflings oder Wert 0",
			"Laufzeit des Prueflings oder Wert 0",
			"#Frequenz-Nummer#",
			"",
		},
	},

	{
		0x065d,
		"Letztes positives Pruefergebnis einer Dauerpruefung Weitere Indizien sind in den Systemmeldungen HYFBNG bzw. HYFBER enthalten.",
		7,
		{
			"Phys. Einrichtungsnummer",
			"Empfangsfeldstaerke des PFG oder Wert 0",
			"Jitterwert des PFG oder Wert 0",
			"Laufzeit des PFG oder Wert 0",
			"Empfangsfeldstaerke des Prueflings oder Wert 0",
			"Jitterwert des Prueflings oder Wert 0",
			"Laufzeit des Prueflings oder Wert 0",
			"", "", "",
		},
	},

	{
		0x065e,
		"Task beendet sich, obwohl sie lt. OS-Tabelle gar nicht mehr aktiv ist.",
		8,
		{
			"DKV-Nummer",
			"LOW-PC des letzten Prozesses",
			"HIGH-PC des letzten Prozesses",
			"Prozess-Status 1 des letzten Prozesses (LOW-TO)",
			"Prozess-Status 1 des letzten Prozesses (HIGH-TO)",
			"Prozess-Status 2 des letzten Prozesses",
			"Prozess-Status 3 des letzten Prozesses",
			"#Ident-Nummer# des letzten Prozesses",
			"", "",
		},
	},

	{
		0x065f,
		"Fehlerhafte Eingabe fuer Frequenzen oder Zeitschlitze wird in BSSYF nicht wirksam. Massnahme: Anlauf mit Laden der Datenbasis, um verfaelschte Daten wiederrueckzusetzen. (T146) (Aenderungsauftrag hatte mehr als 16 gueltige Frequenzen mit zugewiesenem Zeitschlitz zur Folge.)",
		1,
		{
			"Phys. Einr.-Nr. der FDS",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x0660,
		"FuPeF-Systemmeldung wurde empfangen. Die Bedeutung der Indizien muss unter der uebergebenen Systemmeldungs-Nummer in einer FuPeF-Beschreibung nachgelesen werden. Je nach Indizienlaenge sind nur die entsprechenden RP-Bytes gueltig.",
		7,
		{
			"Physikalische Einrichtungs-Nummer",
			"Systemmeldungs-Nummer (High-Byte)",
			"Systemmeldungs-Nummer (Low-Byte)",
			"Indizien-Byte 1 der FuPeF-Systemmeldung",
			"Indizien-Byte 2 der FuPeF-Systemmeldung",
			"Indizien-Byte 3 der FuPeF-Systemmeldung",
			"Indizien-Byte 4 der FuPeF-Systemmeldung",
			"", "", "",
		},
	},

	{
		0x0661,
		"Die empfangene Systemmeldung kam von der FuPef, aber die Systemmeldungs-Nummer war unterhalb des Wertebereiches.r",
		8,
		{
			"Physikalische Einrichtungs-Nummer",
			"Systemmeldungs-Nummer (High-Byte)",
			"Systemmeldungs-Nummer (Low-Byte)",
			"Indizien-Byte 1 der FuPeF-Systemmeldung",
			"Indizien-Byte 2 der FuPeF-Systemmeldung",
			"Indizien-Byte 3 der FuPeF-Systemmeldung",
			"Indizien-Byte 4 der FuPeF-Systemmeldung",
			"#Alarm-Gewicht#/Indizienlaenge der FuPeF- Systemmeldunmg",
			"", "",
		},
	},

	{
		0x0662,
		"FuPeF-Systemmeldung wurde empfangen. Die Bedeutung der Indizien muss unter der uebergebenen Systemmeldungs-Nummer in einer FuPeF-Beschreibung nachgelesen werden. Je nach Indizienlaenge sind nur die entsprechenden RP-Bytes gueltig.",
		7,
		{
			"Physikalische Einrichtungs-Nummer",
			"Systemmeldungs-Nummer (High-Byte)",
			"Systemmeldungs-Nummer (Low-Byte)",
			"Indizien-Byte 1 der FuPeF-Systemmeldung",
			"Indizien-Byte 2 der FuPeF-Systemmeldung",
			"Indizien-Byte 3 der FuPeF-Systemmeldung",
			"Indizien-Byte 4 der FuPeF-Systemmeldung",
			"", "", "",
		},
	},

	{
		0x0663,
		"Prozess wurde durch einen unerwarteten Ereignis-Typ aufgerufen.",
		3,
		{
			"Physikalische Einrichtungs-Nummer der DKV",
			"#Ereignis-Typ#",
			"#Opcode#, falls Ereignis-Typ = Signalisierung",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0664,
		"Prozess wurde durch einen unerwarteten Ereignis-Typ aufgerufen.",
		3,
		{
			"Physikalische Einrichtungs-Nummer der DKV",
			"#Ereignis-Typ#",
			"#Opcode#, falls Ereignis-Typ = Signalisierung",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0665,
		"Prozess wurde durch einen unerwarteten Ereignis-Typ aufgerufen.",
		3,
		{
			"Physikalische Einrichtungs-Nummer der DKV",
			"#Ereignis-Typ#",
			"#Opcode#, falls Ereignis-Typ = Signalisierung",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0666,
		"Der Transfer der RP-Elemente von der STBFDS zur ACTFDS wird beendet, da die bisherige STBFDS ihren Betriebszustand geaendert hat.",
		2,
		{
			"Physikalische Einrichtungs-Nummer der DKV",
			"#Globaler ST-Zustand# der Parallel-FDS",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x0667,
		"Bei der Funkeinrichtungspruefung wurde das PFG als defekt erkannt. (Anzahl der negativ geprueften Einrichtungen uebersteigt  den Schwellwert)",
		9,
		{
			"Phys. Einrichtungsnummer des PFG",
			"Phys. Einrichtungsnummer einer defekt erkannten Einrichtung",
			"#Pruefergebnis-Nummer# einer defekt erkannten Einrichtung",
			"Phys. Einrichtungsnummer einer defekt erkannten Einrichtung",
			"#Pruefergebnis-Nummer# einer defekt erkannten Einrichtung",
			"Phys. Einrichtungsnummer einer defekt erkannten Einrichtung",
			"#Pruefergebnis-Nummer# einer defekt erkannten Einrichtung",
			"Phys. Einrichtungsnummer einer defekt erkannten Einrichtung - oder Wert 0FFH",
			"#Pruefergebnis-Nummer# einer defekt erkannten Einrichtung - oder Wert 0FFH",
			"",
		},
	},

	{
		0x0668,
		"Eintrag der Auftragsnummer des abgeschickten PV-Kommandos",
		2,
		{
			"Phys. Einrichtungsnummer",
			"Auftragsnummer des Kommandos #Entwickler-Info#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x0669,
		"Bei der Suche nach einen freien SPK in einem bestimmten SPK-Buendel wurde ein Fehler bei der Bitleistenzuordnung festgestellt.",
		2,
		{
			"Phys. Einrichtungsnummer",
			"SPK-Nummer",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x066a,
		"Der Zaehler fuer die Anzahl der fuer die VT verfuegbaren Normalkanaele wurde bei der Routinepruefung korrigiert.",
		2,
		{
			"Phys. Einrichtungsnummer",
			"Anzahl der verfuegbaren Normalkanaele",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x066b,
		"ZZK-Ueberlast im Anlauf",
		2,
		{
			"Phys. Einrichtungsnummer",
			"#Opcode# der ZZK-Ueberlastmeldung",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x066c,
		"MSC-Ueberlast im BS-Anlauf",
		2,
		{
			"Phys. Einrichtungsnummer",
			"#Opcode# der MSC-Ueberlastmeldung",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x066d,
		"ENGPASS BEI LEEREN ZEITSCHLITZEN (ANFANG/ENDE)",
		7,
		{
			"Phys. Einrichtungsnummer",
			"Anfang/Ende des Engpasses (Anfang=01H, Ende=00H)",
			"LOG. OGK-NR",
			"VERKUERZTE FREQU.NR",
			"ANZAHL LEERE ZS",
			"ANZAHL KOLLISIONEN",
			"ANZAHL DER BETRACHTETEN ZEITSCHLITZE",
			"", "", "",
		},
	},

	{
		0x066e,
		"SCHWELLWERT-VERLETZUNG BEI VERKEHRSUEBERWACHUNG",
		9,
		{
			"Phys. Einrichtungsnummer",
			"berechneter Indikatorwert (LOW-Byte)",
			"berechneter Indikatorwert (HIGH-Byte)",
			"Schwellwert (LOW-Byte)",
			"Schwellwert (HIGH-Byte)",
			"Aktivdatei-Stand (LOW-Byte)",
			"Aktivdatei-Stand (HIGH-Byte)",
			"Higher Nibble: Indikator-Nummer (1-10) Lower Nibble:  BS-Status Bit 0 :  Ueberlast ja(1)/ nein(0) Bit 1 :  Warteschlangenbetrieb ja(1)/ nein(0) Bit 2 :  Daten unsicher (1)/ sicher (0) Bit 3 :  don't care",
			"Prozentsatz der VT-verfuegbaren SPK",
			"",
		},
	},

	{
		0x066f,
		"Fehler beim Datenbasis-Transfer vom MSC zur DKV aufgetreten. (Die in der BSSYF hinterlegte Datenbasis passt nicht zur BS)",
		10,
		{
			"Phys. Einrichtungsnummer der FDS",
			"#Auftraggeberkennzeichen#",
			"DPC-Nr. BS (Low Byte)",
			"DPC-Nr. BS (High Byte)",
			"DPC-Nr. MSC (Low Byte)",
			"DPC-Nr. MSC (High Byte)",
			"DPC-Nr. MSC aus dem PROM (Low Byte)",
			"DPC-Nr. MSC aus dem PROM (High Byte)",
			"DPC-Nr. BS aus dem PROM (Low Byte)",
			"DPC-Nr. BS aus dem PROM (High Byte)",
		},
	},

	{
		0x0670,
		"Fehler beim Datenbasis-Transfer vom MSC zur DKV aufgetreten. (Die in der BSSYF hinterlegte Datenbasis passt nicht zur BS)",
		10,
		{
			"Phys. Einrichtungsnummer der FDS",
			"#Auftraggeberkennzeichen#",
			"DPC-Nr. BS (Low-Byte)",
			"DPC-Nr. BS (High-Byte)",
			"DPC-Nr. MSC (Low-Byte)",
			"DPC-Nr. MSC (High-Byte)",
			"DPC-Nr. MSC aus dem PROM (Low-Byte)",
			"DPC-Nr. MSC aus dem PROM (High-Byte)",
			"DPC-Nr. BS aus dem PROM (Low-Byte)",
			"DPC-Nr. BS aus dem PROM (High-Byte)",
		},
	},

	{
		0x0671,
		"Datenverfaelschung im BS-Anlauf. (T055) (unzulaessiger Return-Code vom OS-UP WTAKOM bei Pufferanforderung fuer die DKV-interne Signalisierung XSOAI)",
		3,
		{
			"Phys. Einr.-Nr. der FDS",
			"#Return-Code von WTAKOM#",
			"eigene Ident-Nr",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x0672,
		"Datenverfaelschung im Anlauf mit der im BYTE 1 angegebenen Einrichtung. (T041) (Anlauf mit dem PFG: Unzulaessiger Prozesszustand bei Prozessanstoss mit beliebigem Ereignis)",
		5,
		{
			"Phys. Einr.-Nr. der FDS",
			"Phys. Einr.-Nr. (LODEAD)",
			"#Ereignis-Typ#",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"eigene Ident-Nr",
			"", "", "", "", "",
		},
	},

	{
		0x0673,
		"Datenverfaelschung im Anlauf mit der im BYTE 1 angegebenen Einrichtung. (T041) (Anlauf mit dem OGK: Unzulaessiger Prozesszustand bei Prozessanstoss mit beliebigem Ereignis)",
		5,
		{
			"Phys. Einr.-Nr. der FDS",
			"Phys. Einr.-Nr. (LODEAD)",
			"#Ereignis-Typ#",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"eigene Ident-Nr",
			"", "", "", "", "",
		},
	},

	{
		0x0674,
		"Datenverfaelschung im Anlauf mit der im BYTE 1 angegebenen Einrichtung. (T041) (Anlauf mit dem FME: Unzulaessiger Prozesszustand bei Prozessanstoss mit beliebigem Ereignis)",
		5,
		{
			"Phys. Einr.-Nr. der FDS",
			"Phys. Einr.-Nr. (LODEAD)",
			"#Ereignis-Typ#",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"eigene Ident-Nr",
			"", "", "", "", "",
		},
	},

	{
		0x0675,
		"Datenverfaelschung im Anlauf mit der im BYTE 1 angegebenen Einrichtung. (T041) (Anlauf mit dem PHE: Unzulaessiger Prozesszustand bei Prozessanstoss mit beliebigem Ereignis)",
		5,
		{
			"Phys. Einr.-Nr. der FDS",
			"Phys. Einr.-Nr. (LODEAD)",
			"#Ereignis-Typ#",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"eigene Ident-Nr",
			"", "", "", "", "",
		},
	},

	{
		0x0676,
		"Datenverfaelschung im Anlauf mit der im BYTE 1 angegebenen Einrichtung. (T041) (Anlauf mit dem PBR: Unzulaessiger Prozesszustand bei Prozessanstoss mit beliebigem Ereignis)",
		5,
		{
			"Phys. Einr.-Nr. der FDS",
			"Phys. Einr.-Nr. (LODEAD)",
			"#Ereignis-Typ#",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"eigene Ident-Nr",
			"", "", "", "", "",
		},
	},

	{
		0x0677,
		"Datenverfaelschung im Anlauf mit der im BYTE 1 angegebenen Einrichtung. (T041) (Anlauf mit dem SPK: Unzulaessiger Prozesszustand bei Prozessanstoss mit beliebigem Ereignis)",
		5,
		{
			"Phys. Einr.-Nr. der FDS",
			"Phys. Einr.-Nr. (LODEAD)",
			"#Ereignis-Typ#",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"eigene Ident-Nr",
			"", "", "", "", "",
		},
	},

	{
		0x0678,
		"Datenverfaelschung bei Pruefung der SW-Version des DKO (Anlauf der FDS: Unzulaessiger Prozesszustand bei Prozessanstoss mit beliebigem Ereignis)",
		5,
		{
			"Phys. Einr.-Nr. der FDS",
			"Phys. Einr.-Nr. (LODEAD)",
			"#Ereignis-Typ#",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"eigene Ident-Nr",
			"", "", "", "", "",
		},
	},

	{
		0x0679,
		"Die angegebene Einrichtung wurde waehrend der Pruefung der SW-Version nach UNA konfiguriert. (T045) (Die angegebene Einrichtung sendete 3 mal keine bzw.  eine negative SW-Versions-Quittung)",
		4,
		{
			"Phys. Einr.-Nr",
			"Prozesszustand (LODSTA): #Entwickler-Info#",
			"Anzahl der Wiederholungen wegen neg. Quittung",
			"Eigene Ident-Nr",
			"", "", "", "", "", "",
		},
	},

	{
		0x067a,
		"Die angegebene Einrichtung wurde waehrend ihres Anlaufs nach UNA konfiguriert. (T045) (Die SW-Version der angegebenen Einrichtung ist  nicht zulaessig)",
		8,
		{
			"Phys. Einr.-Nr",
			"HW-Variante der Einrichtung",
			"SW-Versionsnummer der Einrichtung",
			"SW-Zustandsnummer (1.Byte)",
			"SW-Zustandsnummer (2.Byte)",
			"SW-Patchversion (1.Byte)",
			"SW-Patchversion (2.Byte)",
			"SW-Versionsnummer der FDS",
			"", "",
		},
	},

	{
		0x067b,
		"Die SW-Version der angegebenen Einrichtung ist nicht zulaessig",
		8,
		{
			"Phys. Einr.-Nr",
			"HW-Variante der Einrichtung",
			"SW-Versionsnummer der Einrichtung",
			"SW-Zustandsnummer (1.Byte)",
			"SW-Zustandsnummer (2.Byte)",
			"SW-Patchversion (1.Byte)",
			"SW-Patchversion (2.Byte)",
			"SW-Versionsnummer der FDS",
			"", "",
		},
	},

	{
		0x067c,
		"Datenverfaelschung bei Pruefung der SW-Version des DKO (Anlauf der FDS: Unzulaessiger Prozesszustand bei Prozessanstoss mit beliebigem Ereignis)",
		5,
		{
			"Phys. Einr.-Nr. der FDS",
			"Phys. Einr.-Nr. (LODEAD)",
			"#Ereignis-Typ#",
			"Prozesszustand (LODSTA) : #Entwickler-Info#",
			"eigene Ident-Nr",
			"", "", "", "", "",
		},
	},

	{
		0x067d,
		"Die SW-Version der angegebenen Einrichtung ist nicht zulaessig",
		8,
		{
			"Phys. Einr.-Nr",
			"HW-Variante der Einrichtung",
			"SW-Versionsnummer der Einrichtung",
			"SW-Zustandsnummer (1.Byte)",
			"SW-Zustandsnummer (2.Byte)",
			"SW-Patchversion (1.Byte)",
			"SW-Patchversion (2.Byte)",
			"SW-Versionsnummer der FDS",
			"", "",
		},
	},

	{
		0x2001,
		"unplausibler Event.",
		6,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"unplausibles Byte",
			"Linknummer des erkennenden Prozesses, #SPILNK#",
			"Prozessdaten Byte 0",
			"Prozessdaten Byte 1",
			"Zielbyte der Signalisierung",
			"", "", "", "",
		},
	},

	{
		0x2002,
		"unplausibler Event.",
		6,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"unplausibles Byte",
			"Linknummer des erkennenden Prozesses, #SPILNK#",
			"Prozessdaten Byte 0",
			"Prozessdaten Byte 1",
			"Zielbyte der Signalisierung",
			"", "", "", "",
		},
	},

	{
		0x2003,
		"unplausibler Headingcode.",
		6,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"unplausibles Byte",
			"Linknummer des erkennenden Prozesses, #SPILNK#",
			"Prozessdaten Byte 0",
			"Prozessdaten Byte 1",
			"Zielbyte der Signalisierung",
			"", "", "", "",
		},
	},

	{
		0x2004,
		"unplausibler Event.",
		6,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"unplausibles Byte",
			"Linknummer des erkennenden Prozesses, #SPILNK#",
			"Prozessdaten Byte 0",
			"Prozessdaten Byte 1",
			"Zielbyte der Signalisierung",
			"", "", "", "",
		},
	},

	{
		0x2005,
		"unplausibler Headingcode.",
		6,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"unplausibles Byte",
			"Linknummer des erkennenden Prozesses, #SPILNK#",
			"Prozessdaten Byte 0",
			"Prozessdaten Byte 1",
			"Zielbyte der Signalisierung",
			"", "", "", "",
		},
	},

	{
		0x2006,
		"unplausibler Event.",
		6,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"unplausibles Byte",
			"Linknummer des erkennenden Prozesses, #SPILNK#",
			"Prozessdaten Byte 0",
			"Prozessdaten Byte 1",
			"Zielbyte der Signalisierung",
			"", "", "", "",
		},
	},

	{
		0x2007,
		"unplausibler Headingcode.",
		6,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"unplausibles Byte",
			"Linknummer des erkennenden Prozesses, #SPILNK#",
			"Prozessdaten Byte 0",
			"Prozessdaten Byte 1",
			"Zielbyte der Signalisierung",
			"", "", "", "",
		},
	},

	{
		0x2008,
		"unplausibler Event.",
		6,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"unplausibles Byte",
			"Linknummer des erkennenden Prozesses, #SPILNK#",
			"Prozessdaten Byte 0",
			"Prozessdaten Byte 1",
			"Zielbyte der Signalisierung",
			"", "", "", "",
		},
	},

	{
		0x2009,
		"unplausibler Headingcode.",
		6,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"unplausibles Byte",
			"Linknummer des erkennenden Prozesses, #SPILNK#",
			"Prozessdaten Byte 0",
			"Prozessdaten Byte 1",
			"Zielbyte der Signalisierung",
			"", "", "", "",
		},
	},

	{
		0x200a,
		"unplausibler Event.",
		6,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"unplausibles Byte",
			"Linknummer des erkennenden Prozesses, #SPILNK#",
			"Prozessdaten Byte 0",
			"Prozessdaten Byte 1",
			"Zielbyte der Signalisierung",
			"", "", "", "",
		},
	},

	{
		0x200b,
		"unplausibler Headingcode.",
		6,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"unplausibles Byte",
			"Linknummer des erkennenden Prozesses, #SPILNK#",
			"Prozessdaten Byte 0",
			"Prozessdaten Byte 1",
			"Zielbyte der Signalisierung",
			"", "", "", "",
		},
	},

	{
		0x200c,
		"unplausibler Event.",
		6,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"unplausibles Byte",
			"Linknummer des erkennenden Prozesses, #SPILNK#",
			"Prozessdaten Byte 0",
			"Prozessdaten Byte 1",
			"Zielbyte der Signalisierung",
			"", "", "", "",
		},
	},

	{
		0x200d,
		"unplausibler Headingcode.",
		6,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"unplausibles Byte",
			"Linknummer des erkennenden Prozesses, #SPILNK#",
			"Prozessdaten Byte 0",
			"Prozessdaten Byte 1",
			"Zielbyte der Signalisierung",
			"", "", "", "",
		},
	},

	{
		0x200e,
		"unplausibler Event.",
		6,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"unplausibles Byte",
			"Linknummer des erkennenden Prozesses, #SPILNK#",
			"Prozessdaten Byte 0",
			"Prozessdaten Byte 1",
			"Zielbyte der Signalisierung",
			"", "", "", "",
		},
	},

	{
		0x200f,
		"unplausible Herkunft der Signalisierung.",
		2,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"unplausibles Byte, #SPIORG#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x2010,
		"unplausibles Zielbyte.",
		2,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"unplausibles Byte",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x2011,
		"unplausibles Zielbyte.",
		2,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"unplausibles Byte",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x2012,
		"unplausibles Zielbyte.",
		2,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"unplausibles Byte",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x2013,
		"unplausibles Taskbyte.",
		2,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"unplausibles Byte",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x2014,
		"unplausibles Taskbyte.",
		2,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"unplausibles Byte",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x2015,
		"unplausibler Headingcode.",
		2,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"unplausibles Byte",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x2016,
		"unplausibles SIO-Byte.",
		2,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"unplausibles Byte",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x2017,
		"unplausibler Headingcode.",
		2,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"unplausibles Byte",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x2018,
		"unplausibles SIO-Byte.",
		2,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"unplausibles Byte",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x2019,
		"unplausibler Link State (RPO)",
		6,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"unplausibler #Link State#",
			"Linknummer des erkennenden Prozesses, #SPILNK#",
			"Prozessdaten Byte 0",
			"Prozessdaten Byte 1",
			"Zielbyte der Signalisierung",
			"", "", "", "",
		},
	},

	{
		0x201a,
		"unplausibler Link State (STA-REST)",
		6,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"unplausibler #Link State#",
			"Linknummer des erkennenden Prozesses, #SPILNK#",
			"Prozessdaten Byte 0",
			"Prozessdaten Byte 1",
			"Zielbyte der Signalisierung",
			"", "", "", "",
		},
	},

	{
		0x201b,
		"unplausibler Link State (COD-REC).",
		6,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"unplausibler #Link State#",
			"Linknummer des erkennenden Prozesses, #SPILNK#",
			"Prozessdaten Byte 0",
			"Prozessdaten Byte 1",
			"Zielbyte der Signalisierung",
			"", "", "", "",
		},
	},

	{
		0x201c,
		"unplausibler Link State (RPR)",
		6,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"unplausibler #Link State#",
			"Linknummer des erkennenden Prozesses, #SPILNK#",
			"Prozessdaten Byte 0",
			"Prozessdaten Byte 1",
			"Zielbyte der Signalisierung",
			"", "", "", "",
		},
	},

	{
		0x201d,
		"unplausibler Link State (SIFA).",
		6,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"unplausibler #Link State#",
			"Linknummer des erkennenden Prozesses, #SPILNK#",
			"Prozessdaten Byte 0",
			"Prozessdaten Byte 1",
			"Zielbyte der Signalisierung",
			"", "", "", "",
		},
	},

	{
		0x201e,
		"unplausibler Link State (SIAC).",
		6,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"unplausibler #Link State#",
			"Linknummer des erkennenden Prozesses, #SPILNK#",
			"Prozessdaten Byte 0",
			"Prozessdaten Byte 1",
			"Zielbyte der Signalisierung",
			"", "", "", "",
		},
	},

	{
		0x201f,
		"unplausibler Link State (OOS)",
		6,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"unplausibler #Link State#",
			"Linknummer des erkennenden Prozesses, #SPILNK#",
			"Prozessdaten Byte 0",
			"Prozessdaten Byte 1",
			"Zielbyte der Signalisierung",
			"", "", "", "",
		},
	},

	{
		0x2020,
		"unplausibler Event.",
		6,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"unplausibles Byte",
			"Linknummer des erkennenden Prozesses, #SPILNK#",
			"Prozessdaten Byte 0",
			"Prozessdaten Byte 1",
			"Zielbyte der Signalisierung",
			"", "", "", "",
		},
	},

	{
		0x2021,
		"unplausibler Event.",
		6,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"unplausibles Byte",
			"Linknummer des erkennenden Prozesses, #SPILNK#",
			"Prozessdaten Byte 0",
			"Prozessdaten Byte 1",
			"Zielbyte der Signalisierung",
			"", "", "", "",
		},
	},

	{
		0x2022,
		"unplausibler Event.",
		6,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"unplausibles Byte",
			"Linknummer des erkennenden Prozesses, #SPILNK#",
			"Prozessdaten Byte 0",
			"Prozessdaten Byte 1",
			"Zielbyte der Signalisierung",
			"", "", "", "",
		},
	},

	{
		0x2023,
		"unplausibler Event.",
		6,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"unplausibles Byte",
			"Linknummer des erkennenden Prozesses, #SPILNK#",
			"Prozessdaten Byte 0",
			"Prozessdaten Byte 1",
			"Zielbyte der Signalisierung",
			"", "", "", "",
		},
	},

	{
		0x2024,
		"unplausibler Event.",
		6,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"unplausibles Byte",
			"Linknummer des erkennenden Prozesses, #SPILNK#",
			"Prozessdaten Byte 0",
			"Prozessdaten Byte 1",
			"Zielbyte der Signalisierung",
			"", "", "", "",
		},
	},

	{
		0x2025,
		"unplausibler Event.",
		6,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"unplausibles Byte",
			"Linknummer des erkennenden Prozesses, #SPILNK#",
			"Prozessdaten Byte 0",
			"Prozessdaten Byte 1",
			"Zielbyte der Signalisierung",
			"", "", "", "",
		},
	},

	{
		0x2026,
		"unplausibler Event.",
		6,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"unplausibles Byte",
			"Linknummer des erkennenden Prozesses, #SPILNK#",
			"Prozessdaten Byte 0",
			"Prozessdaten Byte 1",
			"Zielbyte der Signalisierung",
			"", "", "", "",
		},
	},

	{
		0x2027,
		"unplausibler Event.",
		6,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"unplausibles Byte",
			"Linknummer des erkennenden Prozesses, #SPILNK#",
			"Prozessdaten Byte 0",
			"Prozessdaten Byte 1",
			"Zielbyte der Signalisierung",
			"", "", "", "",
		},
	},

	{
		0x2028,
		"unplausibler Event.",
		6,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"unplausibles Byte",
			"Linknummer des erkennenden Prozesses, #SPILNK#",
			"Prozessdaten Byte 0",
			"Prozessdaten Byte 1",
			"Zielbyte der Signalisierung",
			"", "", "", "",
		},
	},

	{
		0x2029,
		"Fehlermeldung vom SILT",
		5,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"Linknummer des erkennenden Prozesses, #SPILNK#",
			"Massnahme: 02, 06: SAE-Reset 01, 03, 04, 05, 07: an DKV-FBH melden",
			"Nr. des betroffenen Modul im SILT",
			"Fehlernr. im SILT",
			"", "", "", "", "",
		},
	},

	{
		0x202a,
		"Fehlermeldung vom SILT",
		5,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"Linknummer des erkennenden Prozesses, #SPILNK#",
			"Massnahme: 02, 06: SAE-Reset 01, 03, 04, 05, 07: an DKV-FBH melden",
			"Nr. des betroffenen Modul im SILT",
			"Fehlernr. im SILT",
			"", "", "", "", "",
		},
	},

	{
		0x202b,
		"unplausibler Event.",
		6,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"unplausibles Byte",
			"Linknummer des erkennenden Prozesses, #SPILNK#",
			"Prozessdaten Byte 0",
			"Prozessdaten Byte 1",
			"Zielbyte der Signalisierung",
			"", "", "", "",
		},
	},

	{
		0x202c,
		"unplausibler Headingcode.",
		6,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"unplausibles Byte",
			"Linknummer des erkennenden Prozesses, #SPILNK#",
			"Prozessdaten Byte 0",
			"Prozessdaten Byte 1",
			"Zielbyte der Signalisierung",
			"", "", "", "",
		},
	},

	{
		0x202d,
		"unplausibles Zielbyte.",
		6,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"unplausibles Byte",
			"Linknummer des erkennenden Prozesses, #SPILNK#",
			"Prozessdaten Byte 0",
			"Prozessdaten Byte 1",
			"Zielbyte der Signalisierung",
			"", "", "", "",
		},
	},

	{
		0x202e,
		"unplausibler Link State",
		6,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"unplausibler #Link State#",
			"Linknummer des erkennenden Prozesses, #SPILNK#",
			"Prozessdaten Byte 0",
			"Prozessdaten Byte 1",
			"Zielbyte der Signalisierung",
			"", "", "", "",
		},
	},

	{
		0x202f,
		"unplausibler Event.",
		6,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"unplausibles Byte",
			"Linknummer des erkennenden Prozesses, #SPILNK#",
			"Prozessdaten Byte 0",
			"Prozessdaten Byte 1",
			"Zielbyte der Signalisierung",
			"", "", "", "",
		},
	},

	{
		0x2030,
		"Fehler in der Interrupt-HW des DKO (DMA-Interrupt (INT-7) liegt nicht statisch an, nur. kurzer Impuls; oder Interrupt ist durch einen Impuls auf einem anderen Interrupteingang erzeugt worden)",
		6,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"Funkblock-Nr. #VFBLNR#",
			"Zaehlerstand im Funkblock, #DT0CR0# (low)",
			"Zaehlerstand im Funkblock, #DT0CR0# (high) Zeitp. im FBl. =37500 +21.4 - DT0CR0 * 3.9065 us",
			"INT-Bearbeitungsflag #SIBEAR#",
			"INT-Zaehler #VINTCO#",
			"", "", "", "",
		},
	},

	{
		0x2031,
		"Datenverfaelschung oder Fehler in der Timer-HW des DKO. (laut Interrupt-Zeit Raster wurde kein DMA-Interrupt (INT-7) erwartet, er ist zur falschen Zeit eingetreten)",
		6,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"Funkblock-Nr. #VFBLNR#",
			"Zaehlerstand im Funkblock, #DT0CR0# (low)",
			"Zaehlerstand im Funkblock, #DT0CR0# (high) Zeitp. im FBl. =37500 +21.4 - DT0CR0 * 3.9065 us",
			"INT-Bearbeitungsflag #SIBEAR#",
			"INT-Zaehler #VINTCO#",
			"", "", "", "",
		},
	},

	{
		0x2032,
		"Fehler in der DMA-HW oder Datenverfaelschung oder DKV-Fehler (Die DKV hat bei DMA-Uebertragung die DMA-Kontrollbytes nicht richtig beschrieben)",
		3,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"Kontrollbyte #VWKDP0#",
			"Kontrollbyte #VWKDP1#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x2033,
		"kein DMA-Interrupt (INT-7) am Funkblockende (Fehler im OS (Funkblock-Bearbeitungs-Modul WINTR0). an Fehlerzaehler der FBH)",
		3,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"Fehleranzahl (low)",
			"Fehleranzahl (high)",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x2034,
		"Fehler in der zentralen Taktversorgung oder Fehler in der Timer-HW des DKO oder Datenverfaelschung (laut Interrupt-Zeit-Raster wurde kein Funkblock- Interrupt (INT-0) erwartet).",
		6,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"Funkblock-Nr. #VFBLNR#",
			"Zaehlerstand im Funkblock, #DT0CR0# (low)",
			"Zaehlerstand im Funkblock, #DT0CR0# (high) Zeitp. im FBl. =37500 +21.4 - DT0CR0 * 3.9065 us",
			"INT-Bearbeitungsflag #SIBEAR#",
			"INT-Zaehler #VINTCO#",
			"", "", "", "",
		},
	},

	{
		0x2035,
		"Datenverfaelschung oder Fehler in der Timer-HW des DKO. (laut Interrupt-Zeit-Raster wurde kein Interrupt (INT-1) erwartet, er ist zur falschen Zeit eingetreten)",
		6,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"Funkblock-Nr. #VFBLNR#",
			"Zaehlerstand im Funkblock, #DT0CR0# (low)",
			"Zaehlerstand im Funkblock, #DT0CR0# (high) Zeitp. im FBl. =37500 +21.4 - DT0CR0 * 3.9065 us",
			"INT-Bearbeitungsflag #SIBEAR#",
			"INT-Zaehler #VINTCO#",
			"", "", "", "",
		},
	},

	{
		0x2036,
		"Signalisierung DKV -> PG bei Sperre 'Senden an PG' (Fehler im OS (Prozedur WFSECL im Modul WFPROC) an Fehlerzaehler der FBH)",
		2,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"Fehleranzahl",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x2037,
		"Der Inhalt des Process-Output-Buffer wird nicht abgeholt (Wartezeit 400 ms ueberschritten).",
		6,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"Funkblock-Nr. #VFBLNR#",
			"Rahmenzaehler #JUERAZ# (low)",
			"Rahmenzaehler #JUERAZ# (high)",
			"Destination-Byte #SPODST# der Signalisierung im Process-Output-Buffer",
			"#SPXGEL#",
			"", "", "", "",
		},
	},

	{
		0x2038,
		"Betriebsmittelmangel im DKO-OS. (Die Process-Input-Queue ist durch die zuletzt eingetragene Signalisierung ueberschrieben worden)",
		6,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"Funkblock-Nr. #VFBLNR#",
			"Rahmenzaehler #JUERAZ# (low)",
			"Rahmenzaehler #JUERAZ# (high)",
			"Zielbyte der Signalisierung",
			"Eventbyte der Signalisierung",
			"", "", "", "",
		},
	},

	{
		0x2039,
		"Betriebsmittelmangel im DKO-OS. (Die Process-Input-Queue fuer das OS ist durch die zuletzt eingetragene Signalisierung ueberschrieben worden)",
		6,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"Funkblock-Nr. #VFBLNR#",
			"Rahmenzaehler #JUERAZ# (low)",
			"Rahmenzaehler #JUERAZ# (high)",
			"Zielbyte der Signalisierung",
			"Eventbyte der Signalisierung",
			"", "", "", "",
		},
	},

	{
		0x203a,
		"Datenverfaelschung bei der Prozessverwaltung im DKO-OS. (Das Prozess-Timer-Feld ist voll belegt, aber. #Prozess-Kennung# des einzutragenden Timers nicht vorhanden)",
		6,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"Funkblock-Nr. #VFBLNR#",
			"#Prozess-Kennung# des einzutragenden Timers",
			"#Prozess-Kennung# des letzten Timers im Prozess-Timer-Feld",
			"#Prozess-Kennung# des vorletzten Timers im Prozess-Timer-Feld",
			"#Prozess-Kennung# des drittletzten Timers im Prozess-Timer-Feld",
			"", "", "", "",
		},
	},

	{
		0x203b,
		"Laengenindikator einer MSU-L4-Signalisierung ist falsch (Fehler im OS (Prozedur WTXMUP im Modul WTXMUP) an Fehlerspeicher der FBH)",
		2,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"Fehleranzahl",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x203c,
		"Ueberlauf in der Warteschlange zum FIFO an SAE 1. (Fehler im OS (Prozedur WUSTQ im Modul WTXMUP) an Fehlerspeicher der FBH)",
		2,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"Fehleranzahl",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x203d,
		"Ueberlauf in der Warteschlange zum FIFO an SAE-2. (Fehler im OS (Prozedur WUSTQ im Modul WTXMUP) an Fehlerspeicher der FBH)",
		2,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"Fehleranzahl",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x203e,
		"HW-Status des Interface zu SAE-1 nicht setzbar",
		3,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"Funkblock-Nr. #VFBLNR#",
			"HW-Status, Port #DFI0IN#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x203f,
		"Checksumfehler in einer Signalisierung von SAE-1.",
		4,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"Funkblock-Nr. #VFBLNR#",
			"ermittelte Checksum",
			"Checksum in der Signalisierung",
			"", "", "", "", "", "",
		},
	},

	{
		0x2040,
		"unzulaessiger HW-Status des Interface zu SAE-1 in Empfangsrichtung",
		3,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"Funkblock-Nr. #VFBLNR#",
			"HW-Status, Port #DFI0IN#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x2041,
		"HW-Status des Interface zu SAE-2 nicht setzbar",
		3,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"Funkblock-Nr. #VFBLNR#",
			"HW-Status, Port #DFI1IN#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x2042,
		"Checksumfehler in einer Signalisierung von SAE-2.",
		4,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"Funkblock-Nr. #VFBLNR#",
			"ermittelte Checksum",
			"Checksum in der Signalisierung",
			"", "", "", "", "", "",
		},
	},

	{
		0x2043,
		"unzulaessiger HW-Status des Interface zu SAE-2 in Empfangsrichtung",
		3,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"Funkblock-Nr. #VFBLNR#",
			"HW-Status, Port #DFI1IN#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x2044,
		"falsches SIO-Byte in Signalisierung von SAE",
		5,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"Funkblock-Nr. #VFBLNR#",
			"SIO-Byte der Signalisierung",
			"Headingcode der Signalisierung",
			"Adresse des Signalisierungsspeichers (low) (SRXSS0 (low): Speicher von SAE-1 SRXSS1 (low): Speicher von SAE-2)",
			"", "", "", "", "",
		},
	},

	{
		0x2045,
		"falsche MSC-Nummer in Signalisierung von SAE (falscher OPC)",
		6,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"Funkblock-Nr. #VFBLNR#",
			"OPC der Signalisierung (low)",
			"OPC der Signalisierung (high)",
			"Headingcode der Signalisierung",
			"Adresse des Signalisierungsspeichers (low) (SRXSS0 (low): Speicher von SAE-1 SRXSS1 (low): Speicher von SAE-2)",
			"", "", "", "",
		},
	},

	{
		0x2046,
		"falsche BS-Nummer in Signalisierung von SAE (falscher DPC)",
		6,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"Funkblock-Nr. #VFBLNR#",
			"DPC der Signalisierung (low)",
			"DPC der Signalisierung (high)",
			"Headingcode der Signalisierung",
			"Adresse der Signalisierungsspeichers (SRXSS0 (low): Speicher von SAE-1 SRXSS1 (low): Speicher von SAE-2)",
			"", "", "", "",
		},
	},

	{
		0x2047,
		"falscher Laengenindikator in Signalisierung von SAE.",
		5,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"Funkblock-Nr. #VFBLNR#",
			"Laengenindikator der Signalisierung",
			"Headingcode der Signalisierung",
			"Adresse des Signalisierungsspeichers (low) (SRXSS0 (low): Speicher von SAE-1 SRXSS1 (low): Speicher von SAE-2)",
			"", "", "", "", "",
		},
	},

	{
		0x2048,
		"Signalisierung von SAE geloescht: Sign. an DKV im Empfangs-Zwischenspeicher, obwohl Empfang vom MSC durch die DKV nicht freigegeben ist",
		4,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"Funkblock-Nr. #VFBLNR#",
			"#Ident-Nummer#",
			"#Opcode#",
			"", "", "", "", "", "",
		},
	},

	{
		0x2049,
		"unzulaessiger HW-Status des Interface zu SAE-1 in Senderichtung",
		3,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"Funkblock-Nr. #VFBLNR#",
			"HW-Status, Port #DFI0IN#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x204a,
		"Blockierung des FIFO zu SAE-1,. SAE-1 holt Signalisierung nicht ab",
		2,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"Funkblock-Nr. #VFBLNR#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x204b,
		"Zu langer SAE-1 Schreib- oder Lesezugriff",
		3,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"Funkblock-Nr. #VFBLNR#",
			"HW-Status, Port #DFI0IN#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x204c,
		"unzulaessiger HW-Status des Interface zu SAE-2 in Senderichtung",
		3,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"Funkblock-Nr. #VFBLNR#",
			"HW-Status, Port #DFI1IN#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x204d,
		"Blockierung des FIFO zu SAE-2,. SAE-2 holt Signalisierung nicht ab",
		2,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"Funkblock-Nr. #VFBLNR#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x204e,
		"Zu langer SAE-2 Schreib- oder Lesezugriff",
		3,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"Funkblock-Nr. #VFBLNR#",
			"HW-Status, Port #DFI1IN#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x204f,
		"Nicht plausibler Event in Signalisierung",
		2,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"Event",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x2050,
		"Nicht plausibler Event in Signalisierung",
		2,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"Event",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x2051,
		"Datenverfaelschung. (Test auf Stackueberlauf: Das Kontrollwort fuer die Stackgrenze ist ueberschrieben).",
		5,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"Inhalt des Kontrollwortes an der Stackunter- grenze (low)",
			"Inhalt des Kontrollwortes an der Stackunter- grenze (high)",
			"Inhalt von Stackobergrenze +1 (low)",
			"Inhalt von Stackobergrenze +1 (high)",
			"", "", "", "", "",
		},
	},

	{
		0x2052,
		"Datenverfaelschung oder DKV-Fehler (Checksumfehler in den Betriebsparametern).",
		6,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"MSC-Nummer (low)",
			"MSC-Nummer (high)",
			"BS-Nummer (low)",
			"BS-Nummer (high)",
			"Checksum",
			"", "", "", "",
		},
	},

	{
		0x2053,
		"Datenverfaelschung. (es wurde ein RETURN mit falschem Stackpointerstand. ausgefuehrt; keine Indizien)",
		1,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x2054,
		"Meldung ueber DKO-Fehler, der von der DKV erkannt wurde (Systemmeldungs-Nummer fuer den Fall: 'Kein Fehler erkannt, undefinierter RESET'. bei der Uebertragung des Indizienbereichs zur DKV)",
		10,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"Anlauf-Verfolger #VYKAVF#",
			"INTERRUPT-Bearbeitungsanzeige #SIBEAR#",
			"Interruptzaehler #VINTCO#",
			"Funkblocknummer #VFBLNR#",
			"Kennzeichen fuer den Ablauf der INT-Ebene 1 #SABIE1#",
			"Kennzeichen fuer den Ablauf der INT-Ebene 2 #SABIE2#",
			"Prozess-Input-Buffer-Zustand #SPIST#",
			"Inhalt der Adresse in SP-2 (low)",
			"Inhalt der Adresse in SP-2 (high)",
		},
	},

	{
		0x2055,
		"Datenverfaelschung bei der Fehlerbehandlung (Fuer die Systemmeldungs-Nummer gibt es keinen Fehlerzaehler)",
		3,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"Systemmeldungs-Nummer (low)",
			"Systemmeldungs-Nummer (high)",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x2056,
		"Datenverfaelschung. (Watchdogzaehler abgelaufen)",
		4,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"Anlaufverfolgerstand",
			"#Prozess-Kennung# des letzten Prozesses",
			"Linknummer #SPILNK# des letzten Prozesses",
			"", "", "", "", "", "",
		},
	},

	{
		0x2057,
		"Datenverfaelschung oder HW-Fehler. (Fehler-Routine ist angesprungen worden, da undefinierter Sprung im Ablauf oder RESTART vorliegt)",
		5,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"Inhalt der Adresse in SP (low)",
			"Inhalt der Adresse in SP (high)",
			"Inhalt der Adresse in SP+2 (low)",
			"Inhalt der Adresse in SP+2 (high)",
			"", "", "", "", "",
		},
	},

	{
		0x2058,
		"Datenverfaelschung bei der Uebergabe der System-. meldungs-Nummer. (Die Massnahme, die der zu bearbeitenden System-. meldungs-Nummer zugewiesen ist, gibt es nicht)",
		3,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"Systemmeldungs-Nummer (low)",
			"Systemmeldungs-Nummer (high)",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x2059,
		"Datenverfaelschung bei der Uebergabe der Systemmeldungs-Nummer. (Aufruf der FBH mit falscher Indizienlaenge)",
		4,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"Systemmeldungs-Nummer (low)",
			"Systemmeldungs-Nummer (high)",
			"Indizienlaenge",
			"", "", "", "", "", "",
		},
	},

	{
		0x205a,
		"Datenverfaelschung bei der Uebergabe der Systemmeldungs-Nummer. (Kontrollfeld fuer Indizienspeicher wurde ueber-. schrieben)",
		3,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"Systemmeldungs-Nummer (low)",
			"Systemmeldungs-Nummer (high)",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x205b,
		"Datenverfaelschung bei der Uebergabe der Systemmeldungs-Nummer. (Der zu bearbeitenden Systemmeldungs-Nummer ist keine Massnahme zugewiesen).",
		5,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"Systemmeldungs-Nummer (low)",
			"Systemmeldungs-Nummer (high)",
			"Inhalt der Adresse in SP (low)",
			"Inhalt der Adresse in SP (high)",
			"", "", "", "", "",
		},
	},

	{
		0x205c,
		"Fehler in der DKO-Timer-HW (Synchronisierung auf neuen Funkblock: neuer Funkblock tritt nicht ein)",
		4,
		{
			"nicht definiert",
			"#Testnummer#",
			"#Testfehlernummer#",
			"Anlauf-Verfolger #VYKAVF#",
			"", "", "", "", "", "",
		},
	},

	{
		0x205d,
		"Watchdogtest negativ verlaufen. Watchdog abgeschaltet oder defekt. (DKO-Watchdogtest: Watchdog(DKO) laeuft nicht innerhalb 30s ab)",
		4,
		{
			"nicht definiert",
			"#Testnummer#",
			"#Testfehlernummer#",
			"Anlauf-Verfolger #VYKAVF#",
			"", "", "", "", "", "",
		},
	},

	{
		0x205e,
		"DKV-Fehler oder HW-Fehler im DKO (Warten auf neuen Synchronisierungsschritt: Quittungssignal (INT-7) der DKV zum DKO-Watchdog-Test ist noch nicht zurueckgesetzt).",
		4,
		{
			"nicht definiert",
			"#Testnummer#",
			"#Testfehlernummer#",
			"Anlauf-Verfolger #VYKAVF#",
			"", "", "", "", "", "",
		},
	},

	{
		0x205f,
		"Fehler im Kernanlauf: DKV-Fehler oder DKO-HW-Fehler. (Warten auf DKV-Watchdog-Test-Ende: Quittung der DKV fuer Watchdog-Test-Ende kommt nicht)",
		4,
		{
			"nicht definiert",
			"#Testnummer#",
			"#Testfehlernummer#",
			"Anlauf-Verfolger #VYKAVF#",
			"", "", "", "", "", "",
		},
	},

	{
		0x2060,
		"Fehler in der DKO-HW (Fehler beim nichtzerstoerenden RAM-Test (HW-Pruefung YPRAMN) fuer Stack und Fehlerstatistik).",
		4,
		{
			"nicht definiert",
			"#Testnummer#",
			"#Testfehlernummer#",
			"Anlauf-Verfolger #VYKAVF#",
			"", "", "", "", "", "",
		},
	},

	{
		0x2061,
		"Fehler in der DKO-HW (Fehler beim ROM-Summentest (HW-Pruefung YPROME))",
		4,
		{
			"nicht definiert",
			"#Testnummer#",
			"#Testfehlernummer#",
			"Anlauf-Verfolger #VYKAVF#",
			"", "", "", "", "", "",
		},
	},

	{
		0x2062,
		"Fehler in der DKO-HW (Fehler beim zerstoerenden RAM-Test (HW-Pruefung YPRAMZ)).",
		4,
		{
			"nicht definiert",
			"#Testnummer#",
			"#Testfehlernummer#",
			"Anlauf-Verfolger #VYKAVF#",
			"", "", "", "", "", "",
		},
	},

	{
		0x2063,
		"Fehler beim Test der Interface-HW zur SAE (HW-Pruefung YPSAEK)",
		4,
		{
			"nicht definiert",
			"#Testnummer#",
			"#Testfehlernummer#",
			"Anlauf-Verfolger #VYKAVF#",
			"", "", "", "", "", "",
		},
	},

	{
		0x2064,
		"Fehler beim Test der Taktaufbereitung mit Warten auf QSET oder Fehler in der zentralen Taktversorgung. (HW-Pruefung YPCK2K) (Evt. kein QSET oder kein 6.4 MHz Takt).",
		4,
		{
			"nicht definiert",
			"#Testnummer#",
			"#Testfehlernummer#",
			"Anlauf-Verfolger #VYKAVF#",
			"", "", "", "", "", "",
		},
	},

	{
		0x2065,
		"Fehler im Kernanlauf: DKV-Fehler oder DKO-HW-Fehler. Evt. ist ein Fehler in den vorhergehenden DKV-HW-Pruefungen aufgetreten, d.h. evt. Folgefehler (Quittung der DKV auf Signal 'QSET vorhanden', 'Taktaufbereitung laeuft' kommt nicht)",
		4,
		{
			"nicht definiert",
			"#Testnummer#",
			"#Testfehlernummer#",
			"Anlauf-Verfolger #VYKAVF#",
			"", "", "", "", "", "",
		},
	},

	{
		0x2066,
		"Fehler beim Test des Timers 8253/54 (HW-Pruefung YPTIMK)",
		4,
		{
			"nicht definiert",
			"#Testnummer#",
			"#Testfehlernummer#",
			"Anlauf-Verfolger #VYKAVF#",
			"", "", "", "", "", "",
		},
	},

	{
		0x2067,
		"Fehler beim Test des Interruptcontrollers 8259 (HW-Pruefung YPINTK)",
		4,
		{
			"nicht definiert",
			"#Testnummer#",
			"#Testfehlernummer#",
			"Anlauf-Verfolger #VYKAVF#",
			"", "", "", "", "", "",
		},
	},

	{
		0x2068,
		"Fehler beim Test des USART 2661 (HW-Pruefung YPUSAK)",
		4,
		{
			"nicht definiert",
			"#Testnummer#",
			"#Testfehlernummer#",
			"Anlauf-Verfolger #VYKAVF#",
			"", "", "", "", "", "",
		},
	},

	{
		0x2069,
		"Fehler im Kernanlauf: DKV-Fehler oder DKO-HW-Fehler. Evt. ist ein Fehler in den vorhergehenden HW-Pruefungen aufgetreten, d.h. evt. Folgefehler (Quittung der DKV auf 'DKO fuer DMA-Pruefung bereit' kommt nicht).",
		4,
		{
			"nicht definiert",
			"#Testnummer#",
			"#Testfehlernummer#",
			"Anlauf-Verfolger #VYKAVF#",
			"", "", "", "", "", "",
		},
	},

	{
		0x206a,
		"Fehler in der DMA-Pruefung DKV-DKO (HW-Pruefung YPDMKK)",
		4,
		{
			"nicht definiert",
			"#Testnummer#",
			"#Testfehlernummer#",
			"Anlauf-Verfolger #VYKAVF#",
			"", "", "", "", "", "",
		},
	},

	{
		0x206b,
		"Fehler im Kernanlauf: DKV-Fehler oder DKO-HW-Fehler. (DKO wartet auf DKV-Betriebsparameterliste)",
		4,
		{
			"nicht definiert",
			"#Testnummer#",
			"#Testfehlernummer#",
			"Anlauf-Verfolger #VYKAVF#",
			"", "", "", "", "", "",
		},
	},

	{
		0x206c,
		"Fehler im Kernanlauf: DKV-Fehler oder DKO-HW-Fehler. (DKO wartet auf Uebertragungsende der Betriebsparameterliste: DKV sendet kein Uebertragungsende-Signal)",
		4,
		{
			"nicht definiert",
			"#Testnummer#",
			"#Testfehlernummer#",
			"Anlauf-Verfolger #VYKAVF#",
			"", "", "", "", "", "",
		},
	},

	{
		0x206d,
		"Fehler im Kernanlauf: DKV-Fehler oder DKO-HW-Fehler. (DKV Sendet keine Quittung auf 'Fertig fuer BS-Start' des DKO)",
		4,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"#Testnummer#",
			"#Testfehlernummer#",
			"Anlauf-Verfolger #VYKAVF#",
			"", "", "", "", "", "",
		},
	},

	{
		0x206e,
		"Nicht plausibler Event in Signalisierung",
		2,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"Event",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x206f,
		"PG-Fehlerstatistik: Fehler auf der Schnittstelle PG ->. DKO; Wird direkt an DKV-ST gemeldet Keine Massnahmenermittlung im DKO. Die Schnittstellenfehler werden im DKO gezaehlt (Hinweis: Kann auch Fehler in Gestelladresse sein)",
		2,
		{
			"#Phys. Einr.-Nr.# der FUPEF-Einrichtung",
			"Bit 0 bis 5: Fehlerzaehler Bit 6,7:     Art des Fehlers 00 Geraetefehler (Gestelladressenfehler) Zeitueberlauf Checksumfehler statisches BREAK",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x2070,
		"Die Anzahl der auf der Schnittstelle PG -> DKO aufgetretenen Fehler hat Toleranzgrenze erreicht. (wird direkt an DKV-ST gemeldet, keine Massnahmenermittlung im DKO) (Hinweis: Kann auch Fehler in Gestelladresse sein)",
		2,
		{
			"#Phys. Einr.-Nr.# der FUPEF-Einrichtung",
			"Bit 0 bis 5: Fehlerzaehler Bit 6,7:     Art des Fehlers 00 Geraetefehler (Gestelladressenfehler) Zeitueberlauf Checksumfehler statisches BREAK",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x2071,
		"Datenverfaelschung bei der Uebergabe der Systemmeldungs-Nummer (Die zu bearbeitende Systemmeldungs-Nummer ist gestrichen worden; Fehlerleiche)",
		5,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"Systemmeldungs-Nummer (low)",
			"Systemmeldungs-Nummer (high)",
			"Inhalt der Adresse in SP (low)",
			"Inhalt der Adresse in SP (high)",
			"", "", "", "", "",
		},
	},

	{
		0x2072,
		"Datenverfaelschung (Die Loadsharing-Zustaende fuer den Verkehr ueber Link-0 und Link-1 stimmen nicht)",
		5,
		{
			"#Phys. Einr.-Nr.# des DKO",
			"#Loadsharing-Zustand# der Link 0",
			"Komplement",
			"#Loadsharing-Zustand# der Link 1",
			"Komplement",
			"", "", "", "", "",
		},
	},

	{
		0x3000,
		"Sende- Empfangsteilerketten laenger als 1 Rahmen asynchron (FTAK).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#+1",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#+1",
			"", "", "", "", "",
		},
	},

	{
		0x3001,
		"Watch-Dog hat angesprochen (WADOG).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#+1",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#+1",
			"", "", "", "", "",
		},
	},

	{
		0x3002,
		"Rahmensetz-Signal QSET ist laenger als 1 Rahmen ausgefallen (FQSET).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#+1",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#+1",
			"", "", "", "", "",
		},
	},

	{
		0x3003,
		"Sendeteilerkette laenger als 1 Rahmen ausgefallen (FSTK).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#+1",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#+1",
			"", "", "", "", "",
		},
	},

	{
		0x3004,
		"Nicht alle Baugruppen gesteckt oder ein Kontaktfehler (BGOK)",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#+1",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#+1",
			"", "", "", "", "",
		},
	},

	{
		0x3005,
		"Synthesizer-Lockkriterium hat angesprochen (SYLOK0).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#+1",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#+1",
			"", "", "", "", "",
		},
	},

	{
		0x3006,
		"Lockkriterium Synthesizer 1 hat angesprochen (SYLOK1)",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#+1",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#+1",
			"", "", "", "", "",
		},
	},

	{
		0x3007,
		"Lockkriterium Synthesizer 2 hat angesprochen (SYLOK2)",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#+1",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x3008,
		"Lockkriterium Synthesizer 3 hat angesprochen (SYLOK3)",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#+1",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#+1",
			"", "", "", "", "",
		},
	},

	{
		0x3009,
		"Lockkriterium Modulator hat angesprochen (MODLOK)",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#+1",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#+1",
			"", "", "", "", "",
		},
	},

	{
		0x300a,
		"Ruecklauf der Sendeleistung > 8dB (SERUE)",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#+1",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#+1",
			"", "", "", "", "",
		},
	},

	{
		0x300b,
		"Vorlauf der Sendeleistung ist zu klein (SEVOR)",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#+1",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#+1",
			"", "", "", "", "",
		},
	},

	{
		0x300c,
		"Temperatur in der Sendeendstufe ist zu gross (TEMES)",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#+1",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#+1",
			"", "", "", "", "",
		},
	},

	{
		0x300d,
		"Leistungsregelung der Sendeendstufe defekt (SELEI)",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#+1",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#+1",
			"", "", "", "", "",
		},
	},

	{
		0x300e,
		"HF-Pegel am Eingang der Sendeendstufe ist zu klein (HFPEG)",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#+1",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#+1",
			"", "", "", "", "",
		},
	},

	{
		0x3018,
		"Waehrend des SPK-Anlaufes sind nicht alle HW-Stoerungen abgeklungen.",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#+1",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x3019,
		"Der Decoder ist defekt.",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Rahmenzaehlerstand (High Byte)",
			"Rahmenzaehlerstand (Low Byte)",
			"Funkblockzaehlerstand #FBZAE#",
			"", "", "", "", "", "",
		},
	},

	{
		0x301a,
		"Der Coder ist defekt.",
		1,
		{
			"Phys. Einrichtungsnummer",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x301b,
		"Waehrend des OGK-Anlauf sind nicht alle HW-Stoerungen abgeklungen.",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#+1",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x301c,
		"HW-Mehrfach-Fehler wurde erkannt.",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#+1",
			"Massnahmenverursachender Fehler #MVF#",
			"", "", "", "", "", "",
		},
	},

	{
		0x3028,
		"Betriebsmittelmangel (alle Fehlermeldefaecher sind belegt).",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Systemmeldungs-Nummer (High Byte)",
			"Systemmeldungs-Nummer (Low Byte)",
			"Anzahl der momentan aktiven Prozesse",
			"", "", "", "", "", "",
		},
	},

	{
		0x3029,
		"Betriebsmittelmangel (FIFO-Ueberlauf fuer Puffer zur FDS).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Fortsetzungsadresse des aufrufenden Prozesses (High Byte)",
			"Fortsetzungsadresse des aufrufenden Prozesses (Low Byte)",
			"Speicherbanknummer des aufrufenden Prozesses #BNR#",
			"Anzahl der momentan aktiven Prozesse",
			"", "", "", "", "",
		},
	},

	{
		0x302a,
		"Betriebsmittelmangel (kein VT-Puffer fuer Ausgabe zur FDS frei).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Fortsetzungsadresse des aufrufenden Prozesses (Low Byte)",
			"Fortsetzungsadresse des aufrufenden Prozesses (High Byte)",
			"Speicherbanknummer des aufrufenden Prozesses #BNR#",
			"Anzahl der momentan aktiven Prozesse",
			"", "", "", "", "",
		},
	},

	{
		0x302b,
		"Betriebsmittelmangel (FIFO-Ueberlauf fuer interne Puffer).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Fortsetzungsadresse des aufrufenden Prozesses (High Byte)",
			"Fortsetzungsadresse des aufrufenden Prozesses (Low Byte)",
			"Speicherbanknummer des aufrufenden Prozesses #BNR#",
			"Anzahl der momentan aktiven Prozesse",
			"", "", "", "", "",
		},
	},

	{
		0x302c,
		"Betriebsmittelmangel (FIFO fuer Warten auf FDS-Meldung uebergelaufen).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Fortsetzungsadresse des vertagten Prozesses (High Byte)",
			"Fortsetzungsadresse des vertagten Prozesses (Low Byte)",
			"Speicherbanknummer des vertagten Prozesses #BNR#",
			"Anzahl der momentan aktiven Prozesse",
			"", "", "", "", "",
		},
	},

	{
		0x302d,
		"Betriebsmittelmangel (Tabellenueberlauf fuer DE-Vertagung).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Fortsetzungsadresse des vertagten Prozesses (High Byte)",
			"Fortsetzungsadresse des vertagten Prozesses (Low Byte)",
			"Speicherbanknummer des vertagten Prozesses #BNR#",
			"Anzahl der momentan aktiven Prozesse",
			"", "", "", "", "",
		},
	},

	{
		0x302e,
		"Betriebsmittelmangel (Tabellenueberlauf fuer FR-Vertagung)",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Fortsetzungsadresse des vertagten Prozesses (High Byte)",
			"Fortsetzungsadresse des vertagten Prozesses (Low Byte)",
			"Speicherbanknummer des vertagten Prozesses #BNR#",
			"Anzahl der momentan aktiven Prozesse",
			"", "", "", "", "",
		},
	},

	{
		0x302f,
		"Betriebsmittelmangel (Tabellenueberlauf fuer IR-Vertagung)",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Fortsetzungsadresse des vertagten Prozesses (High Byte)",
			"Fortsetzungsadresse des vertagten Prozesses (Low Byte)",
			"Speicherbanknummer des vertagten Prozesses #BNR#",
			"Anzahl der momentan aktiven Prozesse",
			"", "", "", "", "",
		},
	},

	{
		0x3030,
		"Datenverfaelschung (unzulaessige Vertagung bei Taskrueckkehr).",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Fortsetzungsadresse des vertagten Prozesses (High Byte)",
			"Fortsetzungsadresse des vertagten Prozesses (Low Byte)",
			"Speicherbanknummer des vertagten Prozesses #BNR#",
			"", "", "", "", "", "",
		},
	},

	{
		0x3031,
		"Sporadischer HW-Fehler (unzulaessiger Interrupt RST 7.5).",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Rahmenzaehlerstand (High Byte)",
			"Rahmenzaehlerstand (Low Byte)",
			"Funkblockzaehlerstand #FBZAE#",
			"", "", "", "", "", "",
		},
	},

	{
		0x3032,
		"Datenverfaelschung (Anstoss fuer Taskstart mit einer unzulaessigen Startquelle).",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Unzulaessige Startquelle",
			"Uebergebener #Opcode#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x3033,
		"Sporadischer HW-Fehler (Dauer-Break auf der seriellen Schnittstelle).",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Rahmenzaehlerstand (High Byte)",
			"Rahmenzaehlerstand (Low Byte)",
			"Funkblockzaehlerstand #FBZAE#",
			"", "", "", "", "", "",
		},
	},

	{
		0x3034,
		"Datenverfaelschung (3 Zeitplaetze wurden aus der Zuteilungsmeldung ermittelt).",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Rahmenzaehlerstand (High Byte)",
			"Rahmenzaehlerstand (Low Byte)",
			"Funkblockzaehlerstand #FBZAE#",
			"", "", "", "", "", "",
		},
	},

	{
		0x3035,
		"Datenverfaelschung (Empfang einer FDS-Meldung mit einer falschen FKS-Nummer).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Falsche FKS-Nummer aus der FDS-Meldung",
			"Rahmenzaehlerstand (High Byte)",
			"Rahmenzaehlerstand (Low Byte)",
			"Funkblockzaehlerstand #FBZAE#",
			"", "", "", "", "",
		},
	},

	{
		0x3036,
		"Datenverfaelschung (interner Taskstart mit einem unzulaessigen Opcode).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Unzulaessiger #Opcode#",
			"Rahmenzaehlerstand (High Byte)",
			"Rahmenzaehlerstand (Low Byte)",
			"Funkblockzaehlerstand #FBZAE#",
			"", "", "", "", "",
		},
	},

	{
		0x3037,
		"Datenverfaelschung (unzulaessiger Opcode fuer Taskstart von der FDS).",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Unzulaessiger #Opcode#",
			"#Ident-Nummer#",
			"Funkblockzaehlerstand #FBZAE#",
			"", "", "", "", "", "",
		},
	},

	{
		0x3038,
		"Datenverfaelschung (unzulaessiger Opcode fuer Taskstart vom Funk - gilt nur fuer OSK ).",
		1,
		{
			"Phys. Einrichtungsnummer",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x3039,
		"Betriebsmittelmangel (kein Prozesspeicher fuer den zu startenden Prozess frei).",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Startadresse des Prozesses (High Byte)",
			"Startadresse des Prozesses (Low Byte)",
			"Funkblockzahlerstand #FBZAE#",
			"", "", "", "", "", "",
		},
	},

	{
		0x303a,
		"FDS-Meldung nicht im erwarteten Zeitraum empfangen (Timeout bei Empfang auf der seriellen Schnittstelle).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Rahmenzaehlerstand (High Byte)",
			"Rahmenzaehlerstand (Low Byte)",
			"Funkblockzaehlerstand #FBZAE#",
			"Interruptzaehler des OS",
			"", "", "", "", "",
		},
	},

	{
		0x303b,
		"Datenverfaelschung (Checksum Fehler in einer Signalisierung von der FDS).",
		3,
		{
			"Phys. Einrichtungsnummer",
			"#Opcode#",
			"#Ident-Nummer#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x303c,
		"Sporadischer HW-Fehler (Parity oder Frame Fehler des USARTs bei Empfang einer FDS-Signalisierung).",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Rahmenzaehlerstand (High Byte)",
			"Rahmenzaehlerstand (Low Byte)",
			"Funkblockzaehlerstand #FBZAE#",
			"", "", "", "", "", "",
		},
	},

	{
		0x303d,
		"Betriebsmittelmangel (Pool-Ueberlauf bei den Zusatzspeichern).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Fortsetzungsadresse des aufrufenden Prozesses (High Byte)",
			"Fortsetzungsadresse des aufrufenden Prozesses (Low Byte)",
			"Speicherbanknummer des aufrufenden Prozesses #BNR#",
			"Anzahl der momentan aktiven Prozesse",
			"", "", "", "", "",
		},
	},

	{
		0x303e,
		"Sporadischer HW-Fehler oder Datenverfaelschung (HW- und SW-Funkblockzaehler sind asynchron).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Zaehlerstand des SW-Funkblockzaehlers #FBZAE#",
			"Zaehlerstand des HW-Funkblockzaehlers #FRBZAE#",
			"Rahmenzaehlerstand (High Byte)",
			"Rahmenzaehlerstand (Low Byte)",
			"", "", "", "", "",
		},
	},

	{
		0x3044,
		"Wiederholt wurde ein falscher Opcode in der Funksignalisierung eines Teilnehmers erkannt (gilt nur fuer OSK im OGK-Betrieb - Zusatzindizien siehe EW0029)",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Byte 1 der Funksignalisierung (TLN-Restnummer Low Byte)",
			"Byte 2 der Funksignalisierung (TLN-Restnummer High Byte)",
			"Byte 3 der Funksignalisierung (TLN-Nationalitaet u. -UELE-Nummer)",
			"Byte 4 der Funksignalisierung (FUZ-Restnummer)",
			"", "", "", "", "",
		},
	},

	{
		0x3045,
		"Zusatzindizien zu Systemmeldung EW0028",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Byte 5 der Funksignalisierung (FUZ-Nationalitaet und Uele-Nummer)",
			"Byte 9 der Funksignalisierung #Opcode#",
			"Funkblockzaehlerstand #FBZAE#",
			"", "", "", "", "", "",
		},
	},

	{
		0x3046,
		"Verlust der OGK-Funktion durch eine HW-Stoerung (Relais-Umschaltung durch den Verlust der HW-Verfuegbarkeit)",
		5,
		{
			"Phys. Einrichtungsnummer",
			"HW-Kennzeichen #HWTYP#",
			"HW-Stoerungsregister (FRSTR1) #FRSTRX#",
			"HW-Stoerungsregister (FRSTR2) #FRSTRX#",
			"HW-Stoerungsregister (FRSTR3) #FRSTRX#",
			"", "", "", "", "",
		},
	},

	{
		0x3047,
		"Unterprogramm fuer Uebergabe des Data-Recording-Puffers hat eine Veraenderung der Checksum ueber den Pufferkopf erkannt.",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Byte 1 des Data-Recording-Puffer-Kopfes",
			"Byte 2 des Data-Recording-Puffer-Kopfes",
			"Byte 3 des Data-Recording-Puffer-Kopfes",
			"", "", "", "", "", "",
		},
	},

	{
		0x3048,
		"Die Durchlaufe des Organisationsprogramms des OS werden auf 30 begrenzt. Wird das Organisationsprogramm oefters durchlaufen, dann wird die Fehlermeldung generiert.",
		4,
		{
			"Phys. Einrichtungsnummer",
			"#FBZAE#",
			"#HWSTRN#",
			"#HWSTRN#+1",
			"", "", "", "", "", "",
		},
	},

	{
		0x3054,
		"Bei der Kommunikationspruefung zwischen dem OSK und der FDS langt kein Auftrag der FDS ein (2x hintereinander).",
		1,
		{
			"Phys. Einrichtungsnummer",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x3055,
		"Anlauf-Anforderung (YAAV) von der FDS erhalten.",
		1,
		{
			"Phys. Einrichtungsnummer",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x3056,
		"Die Funktionsfaehigkeit des OSK ist innerhalb von 4 Minuten nicht erreicht worden (Anlauf mit der FDS nicht abgeschlossen).",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#+1",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x3057,
		"Power-On oder Baugruppen-Reset",
		2,
		{
			"Phys. Einrichtungsnummer",
			"Kennzeichen fuer Art des Resets #VSTANL#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x3058,
		"Wiederholt sporadische HW-Fehler oder Datenverfaelschung auf der seriellen Schnittstelle zwischen FDS und OSK festgestellt.",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Systemmeldungs-Nummer  (High Byte)",
			"Systemmeldungs-Nummer  (Low Byte)",
			"Eskalationsschwellwert",
			"", "", "", "", "", "",
		},
	},

	{
		0x3059,
		"Ein unbekannter Anlaufgrund wurde im OSK festgestellt.",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Abbild der HW-Stoerungsregister bei Fehlererkennung #HWSTRN#",
			"Abbild der HW-Stoerungsregister bei Fehlererkennung #HWSTRN#+1",
			"Fortsetzungsadresse des laufenden Tasks (High Byte)",
			"Fortsetzungsadresse des laufenden Tasks (Low Byte)",
			"", "", "", "", "",
		},
	},

	{
		0x305a,
		"Tarifdatenuebertragung fehlerhaft auch nach Wieder- holung (Checksum ueber Tarifdaten stimmt nicht mit Uebertragener Checksum aus der FDS ueberein).",
		1,
		{
			"Phys. Einrichtungsnummer",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x305b,
		"Empfangene Tarifdaten-Sammelsignalisierung (#Opcode#: OXTSAV) steht im Widerspruch zum aktuellen Tarifdaten-Zustand trotz abgeschlossenen SPK-Anlaufes.",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Aktueller Tarifdatenzustand",
			"Erwarteter Tarifdatenzustand (KXTDLF)",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x305c,
		"Empfangene Tarifdaten-Einzelsignalisierung der DKV (#Opcode#: OXTEAV) steht im Widerspruch zum aktuellen Tarifdatenzustand.",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Aktueller Tarifdatenzustand",
			"Erwarteter Tarifdatenzustand (KXTDWD)",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x305d,
		"Empfangene Tarifumschalte-Sammelsignalisierung der DKV (#Opcode#: OXTUAV) steht im Widerspruch zum aktuellen Tarifdatenzustand.",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Aktueller Tarifdatenzustand",
			"Erwarteter Tarifdaten-Zustand (KXTDLF)",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x305e,
		"Tarifierungsprozess wurde durch einen unzulaessigen #Opcode# gestartet. (zulaessig: OXTSAV, OXTEAV, OXTUAV)",
		2,
		{
			"Phys. Einrichtungsnummer",
			"Falscher #Opcode#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x305f,
		"Tarifdatenuebertragung fehlerhaft (Checksum ueber Tarifdaten stimmt nicht mit uebertragener Checksum aus der FDS ueberein).",
		1,
		{
			"Phys. Einrichtungsnummer",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x3062,
		"Anlaufquittung (YAQV) nicht innerhalb von 5 Sekunden nach Absenden der Anlaufanforderung (YAAx) erhalten.",
		1,
		{
			"Phys. Einrichtungsnummer",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x3063,
		"Unterbrechung der Verbindung zur FDS (Break-Ausfall > 3 sec).",
		1,
		{
			"Phys. Einrichtungsnummer",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x3064,
		"In den Betriebsparametern von der DKV ist die falsche Synthesizer-Anzahl  4  enthalten, obwohl der Einsatz (die HW) ueber nur einen Synthesizer verfuegt.",
		4,
		{
			"Phys. Einrichtungsnummer",
			"falsche Synthesizeranzahl",
			"HW-Stoerungsregister #HWSTRN#",
			"HW-Stoerungsregister #HWSTRN#+1",
			"", "", "", "", "", "",
		},
	},

	{
		0x3067,
		"Datenverfaelschung (Veraenderung des im OGK und SPK gemeinsamen Teils der Einrichtungsliste wurde erkannt)",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Ist-Checksum",
			"Soll-Checksum",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x3068,
		"Datenverfaelschung (Eine Veraenderung der Frequenztabelle FRQTAB wurde erkannt)",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Ist-Checksum",
			"Soll-Checksum",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x3069,
		"Datenverfaelschung (Eine Veraenderung der Tabelle der Synthesizer-Einstelldaten FRQED wurde erkannt)",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Ist-Checksum",
			"Soll-Checksum",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x306a,
		"Datenverfaelschung (Eine Veraenderung des OGK-Teils der Einrichtungsliste wurde erkannt)",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Ist-Checksum",
			"Soll-Checksum",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x306b,
		"Datenverfaelschung (Eine Veraenderung der Zeitschlitz- Kettungstabelle ZSKTAB wurde erkannt)",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Ist-Checksum",
			"Soll-Checksum",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x306c,
		"Datenverfaelschung (Eine Veraenderung der Zentralen Daten FKSZD wurde erkannt)",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Ist-Checksum",
			"Soll-Checksum",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x306d,
		"Datenverfaelschung (Eine Veraenderung des SPK-Teils der Einrichtungsliste wurde erkannt)",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Ist-Checksum",
			"Soll-Checksum",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x306f,
		"Datenverfaelschung (Eine Veraenderung des Laufzeit- korrekturwertes VLZKOR wurde festgestellt)",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Ist-Checksum",
			"Soll-Checksum",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x3073,
		"Datenverfaelschung (unzulaessiger Index fuer die Fehlerklasse).",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Systemmeldungs-Nummer  (High Byte)",
			"Systemmeldungs-Nummer  (Low Byte)",
			"Falscher #Fehlerklassen-Index#",
			"", "", "", "", "", "",
		},
	},

	{
		0x3074,
		"Datenverfaelschung (unzulaessiger Index fuer Fehlermassnahmen).",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Systemmeldungs-Nummer  (High Byte)",
			"Systemmeldungs-Nummer  (Low Byte)",
			"Falscher #Fehlermassnahmen-Index#",
			"", "", "", "", "", "",
		},
	},

	{
		0x3075,
		"Bei wiederholtem Aussenden einer Systemmeldung (YALAY) keine Quittung von der FDS erhalten (FBH 2).",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Systemmeldungs-Nummer  (High Byte)",
			"Systemmeldungs-Nummer  (Low Byte)",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x307d,
		"Gewolltes Reset im OSK (zB: Anlauf-Anforderung erhalten oder Ausfall der seriellen Schnittstelle zur FDS erkannt)",
		1,
		{
			"Phys. Einrichtungsnummer",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x307e,
		"Bei wiederholtem Aussenden einer Systemmeldung (YALAY) keine Quittung von der FDS erhalten.",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Systemmeldungs-Nummer  (High Byte)",
			"Systemmeldungs-Nummer  (Low Byte)",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x307f,
		"Plausibilitaetsfehler (falsches Aufrufkennzeichen fuer Zeitbewertung eines HW-Fehlers wurde erkannt).",
		2,
		{
			"Phys. Einrichtungsnummer",
			"Falsches #Aufrufkennzeichen#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x3080,
		"Trotz angeforderter Blockbereitstellung wird eine VT-Signalisierung von der FDS empfangen (Fehlerhafte Koordination zwischen OSK und FDS).",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Byte 0 der FDS-Signalisierung ( #Opcode# )",
			"Zeitschlitznummer",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x3081,
		"Mehrfach wurde in einer Funksignalisierung eine falsche Funkzonennummer empfangen",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Byte 1 der Funksignalisierung (Teilnehmer-Restnummer Low Byte)",
			"Byte 2 der Funksignalisierung (Teilnehmer-Restnummer High Byte)",
			"Byte 3 der Funksignalisierung (Teilnehmer-Nationalitaet, Teilnehmer-MSC-Nummer)",
			"Byte 4 der Funksignalisierung (Funkzonen-Restnummer)",
			"", "", "", "", "",
		},
	},

	{
		0x3082,
		"Zusatzindizien fuer Systemmeldung EZ0001",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Byte 5 der Funksignalisierung (Funkzonen-Nationalitaet, Funkzonen-MSC-Nummer)",
			"Byte 9 der Funksignalisierung ( #Opcode# )",
			"Funkblockzaehlerstand",
			"", "", "", "", "", "",
		},
	},

	{
		0x3083,
		"In einer empfangenen Funksignalisierung ist die Funkzonen-Nummer falsch",
		1,
		{
			"Phys. Einrichtungsnummer",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x3084,
		"Anlauf-Wunsch-Auftrag von der FDS erhalten (Aenderung des Betriebsparameter).",
		1,
		{
			"Phys. Einrichtungsnummer",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x3085,
		"Datenverfaelschung (Eine Veraenderung der Tabelle der aktiven Tarifdaten wurde festgestellt)",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Ist-Checksum",
			"Soll-Checksum",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x3086,
		"Datenverfaelschung (Eine Veraenderung der Tabelle der passiven Tarifdaten wurde festgestellt)",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Ist-Checksum",
			"Soll-Checksum",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x3087,
		"Datenverfaelschung ( Eine Veraenderung der Tabelle fuer das Frequenznennungsverfahren wurde erkannt )",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Ist-Checksum",
			"Soll-Checksum",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x3088,
		"Betriebsmittelmangel Anzahl Plaetze in der #Wahltabelle# zu klein",
		1,
		{
			"Phys. Einrichtungsnummer",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x308a,
		"Wiederholt sporadischer Fehler bei der Eigenpruefung im OSK festgestellt (Statistikueberlauf FBH 8).",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Systemmeldungs-Nummer (High Byte)",
			"Systemmeldungs-Nummer (Low Byte)",
			"Eskalationsschwellwert",
			"", "", "", "", "", "",
		},
	},

	{
		0x308b,
		"In den Betriebsparametern ist eine ungueltige Frequenz (00) fuer den SPK enthalten.",
		1,
		{
			"Phys. Einrichtungsnummer",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x308c,
		"Eine Veraenderung in der Tabelle der Statistikschwell= werte wurde festgestellt.",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Ist-Checksum",
			"Soll-Checksum",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x308d,
		"Wiederholt sporadischer Fehler erkannt",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Systemmeldungs-Nummer (High Byte)",
			"Systemmeldungs-Nummer (Low Byte)",
			"Eskalationsschwellwert",
			"", "", "", "", "", "",
		},
	},

	{
		0x308e,
		"Die SPK-FEP stellt fest, dass der Verbindungsaufbau mit dem Prueffunkgeraet negativ verlaufen ist.",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Anzahl der empfangenen Belegungsmeldungen",
			"#Qualitaetsbewertung# der Belegung",
			"gemittelte Feldstaerke",
			"gemittelter Jitterwert",
			"", "", "", "", "",
		},
	},

	{
		0x308f,
		"Verfaelschung der auszusendenden Systemmeldungs-Nummer erkannt.",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Unzulaessige Systemmeldungs-Nummer (High Byte)",
			"Unzulaessige Systemmeldungs-Nummer (Low Byte)",
			"Ist-Checksum",
			"Soll-Checksum",
			"", "", "", "", "",
		},
	},

	{
		0x3090,
		"Verfaelschung des Feldes mit der auszusendenden Systemmeldungs-Nummer erkannt.",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Unzulaessige Systemmeldungs-Nummer (High Byte)",
			"Unzulaessige Systemmeldungs-Nummer (Low Byte)",
			"Ist-Checksum",
			"Soll-Checksum",
			"", "", "", "", "",
		},
	},

	{
		0x3091,
		"Unterbrechung der Verbindung zur FDS ( Break-Ausfall < 3 sec. )",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Abbild HW-Stoerungsregister #HWSTRN#",
			"Abbild HW-Stoerungsregister #HWSTRN#+1",
			"Funkblockzaehler #FBZAE#",
			"", "", "", "", "", "",
		},
	},

	{
		0x3092,
		"Mehrfach in einer Funksignalisierung die Teilnehmer- nummer 0 empfangen",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Byte 9 der Funksignalisierung ( #Opcode# )",
			"Funkblockzaehlerstand",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x3093,
		"In einer empfangenen Funksignalisierung ist die Teilnehmernummer falsch",
		1,
		{
			"Phys. Einrichtungsnummer",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x3095,
		"Datenverfaelschung (kein Fehlermeldefach adressiert).",
		1,
		{
			"Phys. Einrichtungsnummer",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x3096,
		"Datenverfaelschung (falsche Massnahmen fuer Fehlerbehandlung gefordert; Massnahmen 6, 9 und 10 werden in der FUPEF nicht gefuehrt, Massnahme 12 ist nicht in diesem Modul realisiert (UP YSTAT)).",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Systemmeldungs-Nummer  (High Byte)",
			"Systemmeldungs-Nummer  (Low Byte)",
			"Falscher #Fehlermassnahmen-Index#",
			"", "", "", "", "", "",
		},
	},

	{
		0x3097,
		"unzulaessiger Mehrfach-Prozess-start der Vermittlunstechnik",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Opcode der prozess-startenden Signalisierung",
			"aktueller #VT-Zustand#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x3098,
		"Wiederholt sporadischer Fehler bei der Eigenpruefung im PFG festgestellt ( Statistikueberlauf FBH 14)",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Systemmeldungsnummer (High Byte)",
			"Systemmeldungsnummer (Low Byte)",
			"Eskalationsschwellwert",
			"", "", "", "", "", "",
		},
	},

	{
		0x3099,
		"Bei Dachleistungsstufe 0 wurde eine spezielle Leistungs- differenz eingestellt. Es wird Dachleistungsstufe 0 eingestellt.",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Dachleistungsstufe",
			"Spezielle Leistungsdifferenz",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x309a,
		"Es wurde ein Ueber-/Unterlauf der Jitterkennlinie im relevanten Bereich erkannt. Die Leistungsregelung ist nicht eingeschaltet, oder Leistungsregelung nach Feldstaerke.",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Oberer Pegelschwellwert",
			"Unterer Pegelschwellwert",
			"Steigung der Kennlinie (64 facher Wert!)",
			"Offset der Kennlinie",
			"", "", "", "", "",
		},
	},

	{
		0x309b,
		"Es wurde ein Ueber-/Unterlauf der Jitterkennlinie im relevanten Bereich erkannt. Leistungsregelung nach Qualitaet ist eingeschaltet.",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Oberer Pegelschwellwert",
			"Unterer Pegelschwellwert",
			"Steigung der Kennlinie (64 facher Wert!)",
			"Offset der Kennlinie",
			"", "", "", "", "",
		},
	},

	{
		0x309c,
		"Die errechnete Dachleistung ist kleiner als Null. Es wird Leistungsstufe Null eingestellt.",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Berechnete Leistungsstufe",
			"Einstellwert von RFLDIF",
			"Eingestellte Dachleistung (DACHL)",
			"Spezielle Leistungsdifferenz (SPZLDSP)",
			"", "", "", "", "",
		},
	},

	{
		0x309d,
		"Die errechnete Dachleistungsstufe ist groesser als 7. Es wird die maximale Sendeleistung eingestellt.",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Berechnete Leistungsstufe",
			"Einstellwert von RFLDIF",
			"Eingestellte Dachleistung (DACHL)",
			"Spezielle Leistungsdifferenz (SPZLDSP)",
			"", "", "", "", "",
		},
	},

	{
		0x5000,
		"Sende- Empfangsteilerketten laenger als 1 Rahmen asynchron (FTAK).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#+1",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#+1",
			"", "", "", "", "",
		},
	},

	{
		0x5001,
		"Watch-Dog hat angesprochen (WADOG).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#+1",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#+1",
			"", "", "", "", "",
		},
	},

	{
		0x5002,
		"Rahmensetz-Signal QSET ist laenger als 1 Rahmen ausgefallen (FQSET).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#+1",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#+1",
			"", "", "", "", "",
		},
	},

	{
		0x5003,
		"Sendeteilerkette laenger als 1 Rahmen ausgefallen (FSTK).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#+1",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#+1",
			"", "", "", "", "",
		},
	},

	{
		0x5004,
		"Nicht alle Baugruppen gesteckt oder ein Kontaktfehler (BGOK)",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#+1",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#+1",
			"", "", "", "", "",
		},
	},

	{
		0x5005,
		"Synthesizer-Lockkriterium hat angesprochen (SYLOK0).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#+1",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#+1",
			"", "", "", "", "",
		},
	},

	{
		0x500f,
		"Stoerung 6,4 MHZ extern (aus FV) hat angesprochen (P64EXT).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#+1",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#+1",
			"", "", "", "", "",
		},
	},

	{
		0x5010,
		"Stoerung 6,4 MHZ intern hat angesprochen (P64INT).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#+1",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#+1",
			"", "", "", "", "",
		},
	},

	{
		0x5011,
		"Stoerung Betriebsspannung D/A-Wandler hat angesprochen (BSDAW).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#+1",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#+1",
			"", "", "", "", "",
		},
	},

	{
		0x5012,
		"Lock-Kriterium 6,4 MHZ hat angesprochen (LOK64).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#+1",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRA#",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRA#+1",
			"", "", "", "", "",
		},
	},

	{
		0x5013,
		"Temperaturueberwachung Quarzoszillator (6,4 MHZ) hat angesprochen (TEMOSZ).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#+1",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#+1",
			"", "", "", "", "",
		},
	},

	{
		0x5014,
		"Temperaturueberwachung des Quarzoszillator (TEMOSZ) ist 45 Min. nach Anlauf noch nicht abgeklungen.",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#+1",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x5015,
		"Ausfall der externen Fuehrungsgroesse im aktiven PHE bei verfuegbarer Redundanz (NFEST).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#+1",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#+1",
			"", "", "", "", "",
		},
	},

	{
		0x5016,
		"Ausfall der externen Fuehrungsgroesse bei nicht verfuegbarer Redundanz oder im passiven PHE (NFEST).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#+1",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#+1",
			"", "", "", "", "",
		},
	},

	{
		0x5017,
		"Rahmensetz-Signal QSET im aktiven PHE ausgefallen (FQSET).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#+1",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#+1",
			"", "", "", "", "",
		},
	},

	{
		0x5018,
		"Waehrend des PHE-Anlaufes sind nicht alle HW-Stoerungen abgeklungen.",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#+1",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x5019,
		"Der Decoder ist defekt.",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Rahmenzaehlerstand (High Byte)",
			"Rahmenzaehlerstand (Low Byte)",
			"Funkblockzaehlerstand #FBZAE#",
			"", "", "", "", "", "",
		},
	},

	{
		0x501c,
		"HW-Mehrfach-Fehler wurde erkannt.",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#+1",
			"Massnahmenverursachender Fehler #MVF#",
			"", "", "", "", "", "",
		},
	},

	{
		0x5028,
		"Betriebsmittelmangel (alle Fehlermeldefaecher sind belegt-FETAB).",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Systemmeldungs-Nummer (Low Byte)",
			"",
			"Anzahl der momentan aktiven Prozesse",
			"", "", "", "", "", "",
		},
	},

	{
		0x5029,
		"Betriebsmittelmangel (FIFO-Ueberlauf fuer Puffer zur FDS).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Fortsetzungsadresse des aufrufenden Prozesses (High Byte)",
			"Fortsetzungsadresse des aufrufenden Prozesses (Low Byte)",
			"Speicherbanknummer des aufrufenden Prozesses #BNR#",
			"Anzahl der momentan aktiven Prozesse",
			"", "", "", "", "",
		},
	},

	{
		0x502a,
		"Betriebsmittelmangel (kein VT-Puffer fuer Ausgabe zur FDS frei).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Fortsetzungsadresse des aufrufenden Prozesses (Low Byte)",
			"Fortsetzungsadresse des aufrufenden Prozesses (High Byte)",
			"Speicherbanknummer des aufrufenden Prozesses #BNR#",
			"Anzahl der momentan aktiven Prozesse",
			"", "", "", "", "",
		},
	},

	{
		0x502b,
		"Betriebsmittelmangel (FIFO-Ueberlauf fuer interne Puffer).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Fortsetzungsadresse des aufrufenden Prozesses (High Byte)",
			"Fortsetzungsadresse des aufrufenden Prozesses (Low Byte)",
			"Speicherbanknummer des aufrufenden Prozesses #BNR#",
			"Anzahl der momentan aktiven Prozesse",
			"", "", "", "", "",
		},
	},

	{
		0x502c,
		"Betriebsmittelmangel (FIFO fuer Warten auf FDS-Meldung uebergelaufen).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Fortsetzungsadresse des vertagten Prozesses (High Byte)",
			"Fortsetzungsadresse des vertagten Prozesses (Low Byte)",
			"Speicherbanknummer des vertagten Prozesses #BNR#",
			"Anzahl der momentan aktiven Prozesse",
			"", "", "", "", "",
		},
	},

	{
		0x502d,
		"Betriebsmittelmangel (Tabellenueberlauf fuer DE-Vertagung).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Fortsetzungsadresse des vertagten Prozesses (High Byte)",
			"Fortsetzungsadresse des vertagten Prozesses (Low Byte)",
			"Speicherbanknummer des vertagten Prozesses #BNR#",
			"Anzahl der momentan aktiven Prozesse",
			"", "", "", "", "",
		},
	},

	{
		0x502e,
		"Betriebsmittelmangel (Tabellenueberlauf fuer FR-Vertagung)",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Fortsetzungsadresse des vertagten Prozesses (High Byte)",
			"Fortsetzungsadresse des vertagten Prozesses (Low Byte)",
			"Speicherbanknummer des vertagten Prozesses #BNR#",
			"Anzahl der momentan aktiven Prozesse",
			"", "", "", "", "",
		},
	},

	{
		0x502f,
		"Betriebsmittelmangel (Tabellenueberlauf fuer IR-Vertagung)",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Fortsetzungsadresse des vertagten Prozesses (High Byte)",
			"Fortsetzungsadresse des vertagten Prozesses (Low Byte)",
			"Speicherbanknummer des vertagten Prozesses #BNR#",
			"Anzahl der momentan aktiven Prozesse",
			"", "", "", "", "",
		},
	},

	{
		0x5030,
		"Datenverfaelschung (unzulaessige Vertagung bei Prozessrueckkehr).",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Fortsetzungsadresse des vertagten Prozesses (High Byte)",
			"Fortsetzungsadresse des vertagten Prozesses (Low Byte)",
			"Speicherbanknummer des vertagten Prozesses #BNR#",
			"", "", "", "", "", "",
		},
	},

	{
		0x5031,
		"Sporadischer HW-Fehler (unzulaessiger Interrupt RST 7.5).",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Rahmenzaehlerstand (High Byte)",
			"Rahmenzaehlerstand (Low Byte)",
			"Funkblockzaehlerstand #FBZAE#",
			"", "", "", "", "", "",
		},
	},

	{
		0x5032,
		"Datenverfaelschung (Anstoss fuer Prozessstart mit einer unzulaessigen Startquelle).",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Unzulaessige Startquelle",
			"Uebergebener #Opcode#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x5033,
		"Sporadischer HW-Fehler (Dauer-Break auf der seriellen Schnittstelle).",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Rahmenzaehlerstand (High Byte)",
			"Rahmenzaehlerstand (Low Byte)",
			"Funkblockzaehlerstand #FBZAE#",
			"", "", "", "", "", "",
		},
	},

	{
		0x5034,
		"Datenverfaelschung (3 Zeitplaetze wurden aus der Zuteilungsmeldung ermittelt).",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Rahmenzaehlerstand (High Byte)",
			"Rahmenzaehlerstand (Low Byte)",
			"Funkblockzaehlerstand #FBZAE#",
			"", "", "", "", "", "",
		},
	},

	{
		0x5035,
		"Datenverfaelschung (Empfang einer FDS-Meldung mit einer falschen FKS-Nummer).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Falsche FKS-Nummer aus der FDS-Meldung",
			"Rahmenzaehlerstand (High Byte)",
			"Rahmenzaehlerstand (Low Byte)",
			"Funkblockzaehlerstand #FBZAE#",
			"", "", "", "", "",
		},
	},

	{
		0x5036,
		"Datenverfaelschung (interner Prozessstart mit einem unzulaessigen Opcode).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Unzulaessiger #Opcode#",
			"Rahmenzaehlerstand (High Byte)",
			"Rahmenzaehlerstand (Low Byte)",
			"Funkblockzaehlerstand #FBZAE#",
			"", "", "", "", "",
		},
	},

	{
		0x5037,
		"Datenverfaelschung (unzulaessiger Opcode fuer Prozessstart von der FDS).",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Unzulaessiger #Opcode#",
			"#Ident-Nummer#",
			"Funkblockzaehlerstand #FBZAE#",
			"", "", "", "", "", "",
		},
	},

	{
		0x5038,
		"Datenverfaelschung (unzulaessiger Opcode fuer Prozessstart vom Funk - gilt nur fuer OSK ).",
		1,
		{
			"Phys. Einrichtungsnummer",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x5039,
		"Betriebsmittelmangel (kein Prozesspeicher fuer den zu startenden Prozess frei).",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Startadresse des Prozesses (High Byte)",
			"Startadresse des Prozesses (Low Byte)",
			"Funkblockzaehlerstand #FBZAE#",
			"", "", "", "", "", "",
		},
	},

	{
		0x503a,
		"FDS-Meldung nicht im erwarteten Zeitraum empfangen (Timeout bei Empfang auf der seriellen Schnittstelle).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Rahmenzaehlerstand (High Byte)",
			"Rahmenzaehlerstand (Low Byte)",
			"Funkblockzaehlerstand #FBZAE#",
			"Interruptzaehler des OS",
			"", "", "", "", "",
		},
	},

	{
		0x503b,
		"Datenverfaelschung (Checksum Fehler in einer Signalisierung von der FDS).",
		3,
		{
			"Phys. Einrichtungsnummer",
			"#Opcode#",
			"#Ident-Nummer#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x503c,
		"Sporadischer HW-Fehler (Parity oder Frame Fehler des USARTs bei Empfang einer FDS-Signalisierung).",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Rahmenzaehlerstand (High Byte)",
			"Rahmenzaehlerstand (Low Byte)",
			"Funkblockzaehlerstand #FBZAE#",
			"", "", "", "", "", "",
		},
	},

	{
		0x503d,
		"Betriebsmittelmangel (Pool-Ueberlauf bei den Zusatzspeichern).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Fortsetzungsadresse des aufrufenden Prozesses (High Byte)",
			"Fortsetzungsadresse des aufrufenden Prozesses (Low Byte)",
			"Speicherbanknummer des aufrufenden Prozesses #BNR#",
			"Anzahl der momentan aktiven Prozesse",
			"", "", "", "", "",
		},
	},

	{
		0x503e,
		"Sporadischer HW-Fehler oder Datenverfaelschung (HW- und SW-Funkblockzaehler sind asynchron).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Zaehlerstand des SW-Funkblockzaehlers #FBZAE#",
			"Zaehlerstand des HW-Funkblockzaehlers #FRBZAE#",
			"Rahmenzaehlerstand (High Byte)",
			"Rahmenzaehlerstand (Low Byte)",
			"", "", "", "", "",
		},
	},

	{
		0x5044,
		"Wiederholt wurde ein falscher Opcode in der Funksignalisierung eines Teilnehmers erkannt (gilt nur fuer OSK im OGK-Betrieb - Zusatzindizien siehe EW0029)",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Byte 1 der Funksignalisierung (TLN-Restnr. Low Byte)",
			"Byte 2 der Funksignalisierung (TLN-Restnr. High Byte)",
			"Byte 3 der Funksignalisierung (TLN-Nationalitaet und MSC-Nummer)",
			"Byte 4 der Funksignalisierung (FUZ-Restnummer)",
			"", "", "", "", "",
		},
	},

	{
		0x5045,
		"Zusatzindizien zu Systemmeldung EW0028",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Byte 5 der Funksignalisierung (FUZ-Nationalitaet und MSC-Nummer)",
			"Byte 9 der Funksignalisierung #Opcode#",
			"Funkblockzaehlerstand #FBZAE#",
			"", "", "", "", "", "",
		},
	},

	{
		0x5047,
		"Unterprogramm fuer Uebergabe des Data-Recording-Puffers hat eine Veraenderung der Checksum ueber den Pufferkopf erkannt.",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Byte 1 des Data-Recording-Puffer-Kopfes",
			"Byte 2 des Data-Recording-Puffer-Kopfes",
			"Byte 3 des Data-Recording-Puffer-Kopfes",
			"", "", "", "", "", "",
		},
	},

	{
		0x5054,
		"Bei der Kommunikationspruefung zwischen dem PHE und der FDS langt kein Auftrag der FDS ein (2x hintereinander).",
		1,
		{
			"Phys. Einrichtungsnummer",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x5055,
		"Anlauf-Anforderung (YAAV) von der FDS mit Kennzeichen 'KERNANLAUF' erhalten.",
		1,
		{
			"Phys. Einrichtungsnummer",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x5056,
		"Die Funktionsfaehigkeit des PHE ist innerhalb von 4 Minuten nicht erreicht worden (Anlauf mit der FDS nicht abgeschlossen).",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#+1",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x5057,
		"Power-On oder Baugruppen-Reset",
		2,
		{
			"Phys. Einrichtungsnummer",
			"Kennzeichen fuer Art des Resets #VSTANL#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x5058,
		"Wiederholt sporadischer HW-Fehler oder Datenverfaelschung auf der seriellen Schnittstelle zwischen FDS und PHE festgestellt.",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Systemmeldungs-Nummer  (High Byte)",
			"Systemmeldungs-Nummer  (Low Byte)",
			"Eskalationsschwellwert",
			"", "", "", "", "", "",
		},
	},

	{
		0x5059,
		"Ein unbekannter Anlaufgrund wurde im PHE festgestellt.",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Abbild der HW-Stoerungsregister bei Fehlererkennung #HWSTRN#",
			"Abbild der HW-Stoerungsregister bei Fehlererkennung #HWSTRN#+1",
			"Fortsetzungsadresse des laufenden Tasks (High Byte)",
			"Fortsetzungsadresse des laufenden Tasks (Low Byte)",
			"", "", "", "", "",
		},
	},

	{
		0x505a,
		"PHE wird trotz erfolgter Freigabe der Verfuegbarkeit und QSET nicht aktiv.",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#+1",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#",
			"relative Uhrzeit (Minuten)  #RUZEIT#+1",
			"relative Uhrzeit (Stunden)  #RUZEIT#",
			"", "", "", "", "",
		},
	},

	{
		0x505b,
		"Reset nach erstmaligen Erhalt der Betriebsparameter zur gezielten Synchronisation auf eine Phasenbezugs-BS",
		1,
		{
			"Phys. Einrichtungsnummer",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x505c,
		"Datenverfaelschung (im FT-Anlauf auf falschen Funkblock vertagt)",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Soll-Funkblock (1. FB der Messung)",
			"Ist-Funkblock  #FBZAE# (um 1 erhoeht)",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x505d,
		"Asynchrone QSET-Freigabe als Normal-BS",
		3,
		{
			"Phys. Einrichtungsnummer",
			"BS - Typ fuer PHE aus Anlagenliste #FUKOTY#",
			"Schalterstellung (bzw. Bruecke) fuer Initial-BS #TASTEA#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x505e,
		"Datenverfaelschung (im FT-Anlauf bei Kontrolle der Blockzaehler-Synchronitaet auf falschen FB vertagt)",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Soll-Funkblock (1. FB der Messung)",
			"Ist-Funkblock #FBZAE# (erhoeht um 1)",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x505f,
		"Asynchrone QSET-Freigabe (Nach Umschaltung war Funknetzzustand noch unbekannt)",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Abbild des Funknetz-Zustandes #FKORSE#",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#+1",
			"Relative Uhrzeit (Minuten)  #RUZEIT#+1",
			"Relative Uhrzeit (Stunden)  #RUZEIT#",
			"", "", "", "", "",
		},
	},

	{
		0x5060,
		"Nach max. 8x4 Rahmen Suchlauf keinen  PBF empfangen.",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Absolute Uhrzeit Stunden #UZEIT#",
			"Absolute Uhrzeit Minuten #UZEIT#+1",
			"MSC- Nummer der letzten empfangenen BS ( 0 wenn keine BS empfangen wurde )",
			"BS- Nummer der letzten empfangenen BS ( 0 wenn keine BS empfangen wurde )",
			"", "", "", "", "",
		},
	},

	{
		0x5061,
		"Datenverfaelschung (Nicht auf 1 FB vor Messung vertagt)",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Soll-Funkblock (1. FB der Messung)",
			"Ist-Funkblock #FBZAE# (erhoeht um 1)",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x5062,
		"Der passive PHE erkennt kein QSET vom aktiven PHE.",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#+1",
			"Relative Uhrzeit (Minuten)  #RUZEIT#+1",
			"Relative Uhrzeit (Stunden)  #RUZEIT#",
			"", "", "", "", "",
		},
	},

	{
		0x5063,
		"Passiver PHE wird trotz versuchter Freigabe der Verfuegbarkeit und QSET nicht verfuegbar.",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#+1",
			"Relative Uhrzeit (Minuten)  #RUZEIT#+1",
			"Relative Uhrzeit (Stunden)  #RUZEIT#",
			"", "", "", "", "",
		},
	},

	{
		0x5064,
		"Trotz wiederholt an die FDS gesendetem Sendepause-Auftrag (TSPAE) wurde keine Quittung (TSPAV) empfangen. (Phasenmessung kann nicht durchgefuehrt werden)",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Anlaufzustand des PHE mit der FDS #ANLFDS#",
			"Zustand der Schnittstelle zur FDS #SSAKT#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x5065,
		"Datenverfaelschung (Nicht auf einen Funkblock vor Messung vertagt)",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Soll-Funkblock (1. FB der Messung)",
			"Ist-Funkblock #FBZAE# (erhoeht um 1)",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x5066,
		"Zu grosse Phasenabweichung im passiven PHE.",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Phasenmesswert #MWPASS#",
			"Anlaufverfolger der FT #ANLVER#+1",
			"Generatoreinstellwert  (High Byte) #GENEIW#+1",
			"Generatoreinstellwert  (Low Byte) #GENEIW#",
			"", "", "", "", "",
		},
	},

	{
		0x5067,
		"Es konnte 2 Stunden kein PBF empfangen werden.",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Anlaufverfolger der Funktechnik #ANLVER#+1",
			"Genauigkeit der Phasen- und Frequenznachfuehrung #PGEN.FGEN#",
			"Grund der letzten negativen Phasenmessung #PAUSST#+1",
			"Grund der ersten negativen Phasenmessung #PAUSST#",
			"", "", "", "", "",
		},
	},

	{
		0x5068,
		"Nach laengerem Nichtempfang des PBF ( > 2 Stunden ) konnte dieser wieder empfangen werden.",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Anlaufverfolger der Funktechnik #ANLVER#+1",
			"Genauigkeit der Phasen- und Frequenznachfuehrung #PGEN.FGEN#",
			"Grund der letzten negativen Phasenmessung #PAUSST#+1",
			"Grund der ersten negativen Phasenmessung #PAUSST#",
			"", "", "", "", "",
		},
	},

	{
		0x5069,
		"Die Laufzeitmessung kann wegen dauernd belegter HW nicht durchgefuehrt werden.",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Anlaufverfolger der Funktechnik #ANLVER#+1",
			"Funkblockzaehler #FBZAE#",
			"Nummer der zu messenden OGK-Frequenz #OGKNR#+2",
			"Nummer des zu messenden PBF #PBFNR#+2",
			"", "", "", "", "",
		},
	},

	{
		0x506a,
		"Datenverfaelschung (Nicht auf einen Funkblock vor Messung vertagt)",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Soll-Funkblock (1. FB der Messung)",
			"Ist-Funkblock #FBZAE# (erhoeht um 1)",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x506b,
		"Es konnte 4 Stunden lang keine Laufzeitmessung mit positiven Ergebnis durchgefuehrt werden oder ausserhalb vom Wertebereich (Messung des eigenen OGK).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Anlaufverfolger der Funktechnik #ANLVER#+1",
			"Laufzeitwert des eigenen Empfaengers",
			"Grund der letzten neg. Laufzeitmessung #LAUSST#+1",
			"Grund der ersten neg. Laufzeitmessung #LAUSST#",
			"", "", "", "", "",
		},
	},

	{
		0x506c,
		"Trotz 5-maligem Aussenden der Meldung '2 Stunden PBF-Ausfall' (TFBAE) wurde von der FDS keine Quittung erhalten.",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Anlaufverfolger der Funktechnik #ANLVER#+1",
			"Genauigkeit d. Phasen- und Frequenznachfuehrung #PGEN.FGEN#",
			"Grund der letzten negativen Phasenmessung #PAUSST#+1",
			"Grund der ersten negativen Phasenmessung #PAUSST#",
			"", "", "", "", "",
		},
	},

	{
		0x506d,
		"Trotz 5-maligem Aussenden des Frequenzgenauigkeits- Auftrages (TFGAE) konnte keine Quittung von der FDS empfangen werden.",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Anlaufverfolger der Funktechnik #ANLVER#+1",
			"Frequenzgenauigkeit #FRQGEN#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x506e,
		"Plausibilitaetsfehler (Die Frequenzgenauigkeit besitzt undefinierten Wert)",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Frequenzgenauigkeit #FRQGEN#",
			"Genauigkeit der BS #FUKGEN#",
			"Genauigkeit der Frequenznachfuehrung #FGEN#",
			"Genauigkeit der Phasennachfuehrung #PGEN#",
			"", "", "", "", "",
		},
	},

	{
		0x5071,
		"Datenverfaelschung (falsches Unterprogramm fuer die Betriebsparameteruebernahme wurde aufgerufen).",
		1,
		{
			"Phys. Einrichtungsnummer",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x5072,
		"Fehler in Einrichtungsliste oder Datenverfaelschung (keinen gueltigen PBF in den Betriebsparametern erkannt)",
		1,
		{
			"Phys. Einrichtungsnummer",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x5073,
		"Datenverfaelschung in der Einrichtungsliste erkannt.",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Funktechnik Anlaufverfolger #ANLVER#+1",
			"Ist-Checksum",
			"Soll-Checksum",
			"", "", "", "", "", "",
		},
	},

	{
		0x5074,
		"Verfaelschung des Generatoreinstellwerts erkannt",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Falscher Generatoreinstellwert (High Byte) #GENEIW#+1",
			"Falscher Generatoreinstellwert (Low Byte)  #GENEIW#",
			"Checksum des Generatoreinstellwertes #GENEIC#",
			"", "", "", "", "", "",
		},
	},

	{
		0x5075,
		"Generatoreinstellwert ist groesser als der erlaubte Maximalwert.",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Frequenzmesswert #FMESSW#",
			"FT-Anlaufverfolger #ANLVER#+1",
			"Falscher Generatoreinstellwert (High Byte) #GENEIW#+1",
			"Falscher Generatoreinstellwert (Low Byte) #GENEIW#",
			"", "", "", "", "",
		},
	},

	{
		0x5076,
		"Generatoreinstellwert ist kleiner als der erlaubte Minimalwert.",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Frequenzmesswert #FMESSW#",
			"FT-Anlaufverfolger #ANLVER#+1",
			"Falscher Generatoreinstellwert (High Byte) #GENEIW#+1",
			"Falscher Generatoreinstellwert (Low Byte) #GENEIW#",
			"", "", "", "", "",
		},
	},

	{
		0x5078,
		"Veraenderung des Laufzeitwertes des eigenen Empfaengers erkannt.",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Laufzeitwert #LAUFZW#",
			"Checksum des Laufzeitwertes #LAUFZC#",
			"FT-Anlaufverfolger #ANLVER#+1",
			"", "", "", "", "", "",
		},
	},

	{
		0x507a,
		"Betriebsmaessige Umschaltung trotz Verfuegbarkeit des zweiten PHE nicht moeglich.",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#+1",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x507b,
		"Plausibilitaetsfehler (Frequenzgenauigkeitsaenderung auf 'KEINE' Genauigkeit).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#+1",
			"FT-Anlaufverfolger #ANLVER#+1",
			"Einrichtungszustand global #BZUSGL#",
			"", "", "", "", "",
		},
	},

	{
		0x507c,
		"Plausibilitaetsfehler (Aenderung der Frequenz- genauigkeit von 'KEINE' auf 'VOLLE').",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Einrichtungszustand global #BZUSGL#",
			"FT-Anlaufverfolger #ANLVER#+1",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x507d,
		"Seit 2 Std 'BEDINGTE' Frequenzgenauigkeit im PHE.",
		5,
		{
			"Phys. Einrichtungsnummer",
			"FT-Anlaufverfolger #ANLVER#+1",
			"Genauigkeit der Phasen- und Frequenznachfuehrung #PGEN.FGEN#",
			"Erster Ausstiegsgrund aus der Phasenmessung #PAUSST#",
			"Letzter Ausstiegsgrund aus der Phasenmessung #PAUSST#+1",
			"", "", "", "", "",
		},
	},

	{
		0x507e,
		"Nach mindestens 2 Std 'BEDINGTE' wieder 'VOLLE' Frequenzgenauigkeit im PHE erreicht.",
		2,
		{
			"Phys. Einrichtungsnummer",
			"FT-Anlaufverfolger #ANLVER#+1",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x507f,
		"Bei wiederholtem Aussenden der Statusmeldung (YSTAE) keine Quittung von der FDS erhalten.",
		2,
		{
			"Phys. Einrichtungsnummer",
			"Einrichtungszustand global #BZUSGL#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x5080,
		"Betriebsmaessige Umschaltung trotz Verfuegbarkeit des zweiten PHE nicht moeglich.",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#+1",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x5082,
		"Datenverfaelschung (kein Fehlermeldefach adressiert).",
		1,
		{
			"Phys. Einrichtungsnummer",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x5083,
		"Datenverfaelschung (unzulaessiger Index fuer die Fehlerklasse).",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Systemmeldungs-Nummer  (High Byte)",
			"Systemmeldungs-Nummer  (Low Byte)",
			"Falscher #Fehlerklassen-Index#",
			"", "", "", "", "", "",
		},
	},

	{
		0x5084,
		"Datenverfaelschung (unzulaessiger Index fuer Fehlermassnahmen).",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Systemmeldungs-Nummer  (High Byte)",
			"Systemmeldungs-Nummer  (Low Byte)",
			"Falscher #Fehlermassnahmen-Index#",
			"", "", "", "", "", "",
		},
	},

	{
		0x5085,
		"Bei wiederholtem Aussenden einer Systemmeldung (YALAY) keine Quittung von der FDS erhalten (FBH 2).",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Systemmeldungs-Nummer  (High Byte)",
			"Systemmeldungs-Nummer  (Low Byte)",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x508b,
		"Datenverfaelschung (falsche Massnahmen fuer Fehlerbehandlung gefordert; Massnahmen 6, 9 und 10 werden in der FUPEF nicht gefuehrt, Massnahme 12 wird nicht in diesem Modul realisiert (UP YSTAT)).",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Systemmeldungs-Nummer  (High Byte)",
			"Systemmeldungs-Nummer  (Low Byte)",
			"Falscher #Fehlermassnahmen-Index#",
			"", "", "", "", "", "",
		},
	},

	{
		0x508d,
		"Gewolltes Reset im PHE (zB: Anlauf-Anforderung mit Kennzeichen 'KERNANLAUF' erhalten oder Ausfall der seriellen Schnittstelle zur FDS erkannt).",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Stunden der absoluten Uhrzeit #UZEIT#",
			"Minuten der absoluten Uhrzeit #UZEIT#+1",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x508e,
		"Bei wiederholtem Aussenden einer Systemmeldung (YALAY) keine Quittung von der FDS erhalten.",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Systemmeldungs-Nummer  (High Byte)",
			"Systemmeldungs-Nummer  (Low Byte)",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x508f,
		"Plausibilitaetsfehler (falsches Aufrufkennzeichen fuer Zeitbewertung eines HW-Fehlers wurde erkannt).",
		2,
		{
			"Phys. Einrichtungsnummer",
			"Falsches #Aufrufkennzeichen#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x5090,
		"Externe Fuehrungsgroesse defekt !",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Messwert vom Frequenzdiskriminator",
			"Messwert vom Frequenzdiskr. vor 1 Minute",
			"Generatoreinstellwert (High Byte) #GENEIW#+1",
			"Generatoreinstelwert (Low Byte) #GENEIW#",
			"", "", "", "", "",
		},
	},

	{
		0x5091,
		"Trotz 5-maligen Aussenden des Phasenfuehrungs-Auftrages (TPHAE) konnte keine Quittung von der FDS empfangen werden.",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Anlaufverfolger der Funktechnik #ANLVER#+1",
			"Zustand der Phasenfuehrung Teil 1 #FPHAE#",
			"Zustand der Phasenfuehrung Teil 2 #FPHAE#+1",
			"", "", "", "", "", "",
		},
	},

	{
		0x5092,
		"Ursache des Suchlaufes im History-File hinterlegen",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Aufrufort (Modul/Unterprog.) und Ursache des Suchlaufes 01=TEBFAK, nach verlorener Synchronisation konnte eine bestimmte Zeit lang kein PBF empfangen werden beim Anlauf konnte kein PBF empfangen werden indirekter Start erfolgte durch die FDS im passiven PHE direkter Start erfolgte durch die FDS im aktiven PHE",
			"Ausstiegsgrund der ersten Messung (#PAUSST#); wenn BYTE 1 = 01 MSC- Nummer der letzten empfangenen BS;         -- \" --   = 03",
			"Ausstiegsgrund der letzten Messung (#PAUSST#+1);-- \" --   = 01 BS- Nummer der letzten empfangenen BS;          -- \" --   = 03",
			"Wenn BYTE 1 = 03;  K  Kors wurde mindestens einmal erkannt +-+-+-+-+-+-+-+-+  D  mind. einmal wurde eine Nachricht dec !K!D!F!J!O!x!x!x!  F  mindestens einmal Feldstaerke gut +-+-+-+-+-+-+-+-+  J  mindestens einmal Jitter gut Bit- Maske      O  mind. einmal wurde Leerruf erkannt",
			"", "", "", "", "",
		},
	},

	{
		0x5093,
		"Versuch der Neusynchronisation auf das Netz mittels Suchlauf.",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Absolute Uhrzeit Stunden #UZEIT#",
			"Absolute Uhrzeit Minuten #UZEIT#+1",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x5094,
		"Suchlauf wurde im aktiven PHE positiv beendet, mit normalen Anlauf als passiver PHE beginnen.",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Absolute Uhrzeit Stunden #UZEIT#",
			"Absolute Uhrzeit Minuten #UZEIT#+1",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x5095,
		"Die Phasenmessung im passiven PHE kann wegen dauernd belegter HW nicht durchgefuehrt werden.",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Anlaufverfolger der Funktechnik #ANLVER#+1",
			"Funkblockzaehler #FBZAE#",
			"Nummer der zu messenden OGK-Frequenz #OGKNR#+2",
			"Nummer des zu messenden PBF #PBFNR#+2",
			"", "", "", "", "",
		},
	},

	{
		0x5096,
		"Die Phasenmessung im aktiven PHE kann wegen dauernd belegter HW nicht durchgefuehrt werden.",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Anlaufverfolger der Funktechnik #ANLVER#+1",
			"Funkblockzaehler #FBZAE#",
			"Nummer der zu messenden OGK-Frequenz #OGKNR#+2",
			"Nummer des zu messenden PBF #PBFNR#+2",
			"", "", "", "", "",
		},
	},

	{
		0x5097,
		"Datenverfaelschung in der Tabelle der OGK-Frequenzen erkannt.",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Funktechnik Anlaufverfolger #ANLVER#+1",
			"Ist-Checksum",
			"Soll-Checksum",
			"", "", "", "", "", "",
		},
	},

	{
		0x5098,
		"Datenverfaelschung in der Tabelle der Statistikschwell- werte erkannt.",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Funktechnik Anlaufverfolger #ANLVER#+1",
			"Ist-Checksum",
			"Soll-Checksum",
			"", "", "", "", "", "",
		},
	},

	{
		0x5099,
		"Wiederholt sporadischer Fehler bei der Eigenpruefung im PHE festgestellt (Statistikueberlauf FBH 8).",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Systemmeldungs-Nummer (High Byte)",
			"Systemmeldungs-Nummer (Low Byte)",
			"Eskalationsschwellwert",
			"", "", "", "", "", "",
		},
	},

	{
		0x509a,
		"Wiederholt sporadischer Fehler erkannt",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Systemmeldungs-Nummer (High Byte)",
			"Systemmeldungs-Nummer (Low Byte)",
			"Eskalationsschwellwert",
			"", "", "", "", "", "",
		},
	},

	{
		0x509b,
		"Datenverfaelschung im Feld ABRUST erkannt",
		5,
		{
			"Phys. Einrichtungsnummer",
			"falsche Fehlernummer ( High )",
			"falsche Fehlernummer ( Low )",
			"Ist-Checksum",
			"Soll-Checksum",
			"", "", "", "", "",
		},
	},

	{
		0x509c,
		"Verfaelschung der auszusendenden Systemmeldungs-Nummer erkannt.",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Unzulaessige Systemmeldungs-Nummer (High Byte)",
			"Unzulaessige Systemmeldungs-Nummer (Low Byte)",
			"Ist-Checksum",
			"Soll-Checksum",
			"", "", "", "", "",
		},
	},

	{
		0x509d,
		"Unterbrechung der Verbindung zur FDS ( Breakausfall < 3 sec. ) Es findet nur die Protokollierung des Ereignisses statt.",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Abbild HW-Stoerungsregister #HWSTRN#",
			"Abbild HW-Stoerungsregister + 1 #HWSTRN#+1",
			"Funkblockzaehlerstand #FBZAE#",
			"", "", "", "", "", "",
		},
	},

	{
		0x509e,
		"Wiederholt sporadischer Fehler bei der Eigenpruefung im PFG festgestellt ( Statistikueberlauf FBH 14 )",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Systemmeldungsnummer (High Byte)",
			"Systemmeldungsnummer (Low Byte)",
			"Eskalationsschwellwert",
			"", "", "", "", "", "",
		},
	},

	{
		0x6000,
		"Sende- Empfangsteilerketten laenger als 1 Rahmen asynchron (FTAK).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#+1",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#+1",
			"", "", "", "", "",
		},
	},

	{
		0x6001,
		"Watch-Dog hat angesprochen (WADOG).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#+1",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#+1",
			"", "", "", "", "",
		},
	},

	{
		0x6002,
		"Rahmensetz-Signal QSET ist laenger als 1 Rahmen ausgefallen (FQSET).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#+1",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#+1",
			"", "", "", "", "",
		},
	},

	{
		0x6003,
		"Sendeteilerkette laenger als 1 Rahmen ausgefallen (FSTK).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#+1",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#+1",
			"", "", "", "", "",
		},
	},

	{
		0x6004,
		"Nicht alle Baugruppen gesteckt oder ein Kontaktfehler (BGOK)",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#+1",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#+1",
			"", "", "", "", "",
		},
	},

	{
		0x6005,
		"Synthesizer-Lockkriterium hat angesprochen (SYLOK0).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#+1",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#+1",
			"", "", "", "", "",
		},
	},

	{
		0x6009,
		"Lockkriterium Modulator hat angesprochen (MODLOK)",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#+1",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#+1",
			"", "", "", "", "",
		},
	},

	{
		0x6018,
		"Waehrend des PFG-Anlaufes sind nicht alle HW-Stoerungen abgeklungen.",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#+1",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x6019,
		"Der Decoder ist defekt.",
		1,
		{
			"Phys. Einrichtungsnummer",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x601a,
		"Der Coder ist defekt.",
		1,
		{
			"Phys. Einrichtungsnummer",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x601c,
		"HW-Mehrfach-Fehler wurde erkannt.",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#+1",
			"Massnahmenverursachender Fehler #MVF#",
			"", "", "", "", "", "",
		},
	},

	{
		0x6028,
		"Betriebsmittelmangel (alle Fehlermeldefaecher sind belegt-FETAB).",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Systemmeldungs-Nummer (Low Byte)",
			"",
			"Anzahl der momentan aktiven Prozesse",
			"", "", "", "", "", "",
		},
	},

	{
		0x6029,
		"Betriebsmittelmangel (FIFO-Ueberlauf fuer Puffer zur FDS).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Fortsetzungsadresse des aufrufenden Prozesses (High Byte)",
			"Fortsetzungsadresse des aufrufenden Prozesses (Low Byte)",
			"Speicherbanknummer des aufrufenden Prozesses #BNR#",
			"Anzahl der momentan aktiven Prozesse",
			"", "", "", "", "",
		},
	},

	{
		0x602a,
		"Betriebsmittelmangel (kein VT-Puffer fuer Ausgabe zur FDS frei).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Fortsetzungsadresse des aufrufenden Prozesses (Low Byte)",
			"Fortsetzungsadresse des aufrufenden Prozesses (High Byte)",
			"Speicherbanknummer des aufrufenden Prozesses #BNR#",
			"Anzahl der momentan aktiven Prozesse",
			"", "", "", "", "",
		},
	},

	{
		0x602b,
		"Betriebsmittelmangel (FIFO-Ueberlauf fuer interne Puffer).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Fortsetzungsadresse des aufrufenden Prozesses (High Byte)",
			"Fortsetzungsadresse des aufrufenden Prozesses (Low Byte)",
			"Speicherbanknummer des aufrufenden Prozesses #BNR#",
			"Anzahl der momentan aktiven Prozesse",
			"", "", "", "", "",
		},
	},

	{
		0x602c,
		"Betriebsmittelmangel (FIFO fuer Warten auf FDS-Meldung uebergelaufen).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Fortsetzungsadresse des vertagten Prozesses (High Byte)",
			"Fortsetzungsadresse des vertagten Prozesses (Low Byte)",
			"Speicherbanknummer des vertagten Prozesses #BNR#",
			"Anzahl der momentan aktiven Prozesse",
			"", "", "", "", "",
		},
	},

	{
		0x602d,
		"Betriebsmittelmangel (Tabellenueberlauf fuer DE-Vertagung).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Fortsetzungsadresse des vertagten Prozesses (High Byte)",
			"Fortsetzungsadresse des vertagten Prozesses (Low Byte)",
			"Speicherbanknummer des vertagten Prozesses #BNR#",
			"Anzahl der momentan aktiven Prozesse",
			"", "", "", "", "",
		},
	},

	{
		0x602e,
		"Betriebsmittelmangel (Tabellenueberlauf fuer FR-Vertagung)",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Fortsetzungsadresse des vertagten Prozesses (High Byte)",
			"Fortsetzungsadresse des vertagten Prozesses (Low Byte)",
			"Speicherbanknummer des vertagten Prozesses #BNR#",
			"Anzahl der momentan aktiven Prozesse",
			"", "", "", "", "",
		},
	},

	{
		0x602f,
		"Betriebsmittelmangel (Tabellenueberlauf fuer IR-Vertagung)",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Fortsetzungsadresse des vertagten Prozesses (High Byte)",
			"Fortsetzungsadresse des vertagten Prozesses (Low Byte)",
			"Speicherbanknummer des vertagten Prozesses #BNR#",
			"Anzahl der momentan aktiven Prozesse",
			"", "", "", "", "",
		},
	},

	{
		0x6030,
		"Datenverfaelschung (unzulaessige Vertagung bei Prozessrueckkehr).",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Fortsetzungsadresse des vertagten Prozesses (High Byte)",
			"Fortsetzungsadresse des vertagten Prozesses (Low Byte)",
			"Speicherbanknummer des vertagten Prozesses #BNR#",
			"", "", "", "", "", "",
		},
	},

	{
		0x6031,
		"Sporadischer HW-Fehler (unzulaessiger Interrupt RST 7.5).",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Rahmenzaehlerstand (High Byte)",
			"Rahmenzaehlerstand (Low Byte)",
			"Funkblockzaehlerstand #FBZAE#",
			"", "", "", "", "", "",
		},
	},

	{
		0x6032,
		"Datenverfaelschung (Anstoss fuer Prozessstart mit einer unzulaessigen Startquelle).",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Unzulaessige Startquelle",
			"Uebergebener #Opcode#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x6033,
		"Sporadischer HW-Fehler (Dauer-Break auf der seriellen Schnittstelle).",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Rahmenzaehlerstand (High Byte)",
			"Rahmenzaehlerstand (Low Byte)",
			"Funkblockzaehlerstand #FBZAE#",
			"", "", "", "", "", "",
		},
	},

	{
		0x6034,
		"Datenverfaelschung (3 Zeitplaetze wurden aus der Zuteilungsmeldung ermittelt).",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Rahmenzaehlerstand (High Byte)",
			"Rahmenzaehlerstand (Low Byte)",
			"Funkblockzaehlerstand #FBZAE#",
			"", "", "", "", "", "",
		},
	},

	{
		0x6035,
		"Datenverfaelschung (Empfang einer FDS-Meldung mit einer falschen FKS-Nummer).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Falsche FKS-Nummer aus der FDS-Meldung",
			"Rahmenzaehlerstand (High Byte)",
			"Rahmenzaehlerstand (Low Byte)",
			"Funkblockzaehlerstand #FBZAE#",
			"", "", "", "", "",
		},
	},

	{
		0x6036,
		"Datenverfaelschung (interner Prozessstart mit einem unzulaessigen Opcode).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Unzulaessiger #Opcode#",
			"Rahmenzaehlerstand (High Byte)",
			"Rahmenzaehlerstand (Low Byte)",
			"Funkblockzaehlerstand #FBZAE#",
			"", "", "", "", "",
		},
	},

	{
		0x6037,
		"Datenverfaelschung (unzulaessiger Opcode fuer Prozessstart von der FDS).",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Unzulaessiger #Opcode#",
			"#Ident-Nummer#",
			"Funkblockzaehlerstand #FBZAE#",
			"", "", "", "", "", "",
		},
	},

	{
		0x6038,
		"Datenverfaelschung (unzulaessiger Opcode fuer Prozessstart vom Funk - gilt nur fuer OSK ).",
		1,
		{
			"Phys. Einrichtungsnummer",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x6039,
		"Betriebsmittelmangel (kein Prozesspeicher fuer den zu startenden Prozess frei).",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Startadresse des Prozesses (High Byte)",
			"Startadresse des Prozesses (Low Byte)",
			"Funkblockzahlerstand #FBZAE#",
			"", "", "", "", "", "",
		},
	},

	{
		0x603a,
		"FDS-Meldung nicht im erwarteten Zeitraum empfangen (Timeout bei Empfang auf der seriellen Schnittstelle).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Rahmenzaehlerstand (High Byte)",
			"Rahmenzaehlerstand (Low Byte)",
			"Funkblockzaehlerstand #FBZAE#",
			"Interruptzaehler des OS",
			"", "", "", "", "",
		},
	},

	{
		0x603b,
		"Datenverfaelschung (Checksum Fehler in einer Signalisierung von der FDS).",
		3,
		{
			"Phys. Einrichtungsnummer",
			"#Opcode#",
			"#Ident-Nummer#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x603c,
		"Sporadischer HW-Fehler (Parity oder Frame Fehler des USARTs bei Empfang einer FDS-Signalisierung).",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Rahmenzaehlerstand (High Byte)",
			"Rahmenzaehlerstand (Low Byte)",
			"Funkblockzaehlerstand #FBZAE#",
			"", "", "", "", "", "",
		},
	},

	{
		0x603d,
		"Betriebsmittelmangel (Pool-Ueberlauf bei den Zusatzspeichern).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Fortsetzungsadresse des aufrufenden Prozesses (High Byte)",
			"Fortsetzungsadresse des aufrufenden Prozesses (Low Byte)",
			"Speicherbanknummer des aufrufenden Prozesses #BNR#",
			"Anzahl der momentan aktiven Prozesse",
			"", "", "", "", "",
		},
	},

	{
		0x603e,
		"Sporadischer HW-Fehler oder Datenverfaelschung (HW- und SW-Funkblockzaehler sind asynchron).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Zaehlerstand des SW-Funkblockzaehlers #FBZAE#",
			"Zaehlerstand des HW-Funkblockzaehlers #FRBZAE#",
			"Rahmenzaehlerstand (High Byte)",
			"Rahmenzaehlerstand (Low Byte)",
			"", "", "", "", "",
		},
	},

	{
		0x6043,
		"Datenverfaelschung (unzulaessige Pruefeinstellung).",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Byte fuer fehlerhafte Pruefeinstellung #BETART#",
			"Flag fuer HW-Belegung #FLHWBL#+1",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x6044,
		"Wiederholt wurde ein falscher OP-Code in der Funksignalisierung eines Teilnehmers erkannt (gilt nur fuer OSK im OGK-Betrieb - Zusatzindizien siehe EW0029)",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Byte 1 der Funksignalisierung (TLN-Restnummer Low Byte)",
			"Byte 2 der Funksignalisierung (TLN-Restnummer High Byte)",
			"Byte 3 der Funksignalisierung (TLN-Nationalitaet und MSC-Nummer)",
			"Byte 4 der Funksignalisierung (FUZ-Restnummer)",
			"", "", "", "", "",
		},
	},

	{
		0x6045,
		"Zusatzindizien zu Systemmeldung EW0028",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Byte 5 der Funksignalisierung (FUZ-Nationalitaet und MSC-Nummer)",
			"Byte 9 der Funksignalisierung #Opcode#",
			"Funkblockzaehlerstand #FBZAE#",
			"", "", "", "", "", "",
		},
	},

	{
		0x6047,
		"Unterprogramm fuer Uebergabe des Data-Recording-Puffers hat eine Veraenderung der Checksum ueber den Pufferkopf erkannt.",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Byte 1 des Data-Recording-Puffer-Kopfes",
			"Byte 2 des Data-Recording-Puffer-Kopfes",
			"Byte 3 des Data-Recording-Puffer-Kopfes",
			"", "", "", "", "", "",
		},
	},

	{
		0x6054,
		"Bei der Kommunikationspruefung zwischen dem PFG und der FDS langt kein Auftrag der FDS ein (2x hintereinander).",
		1,
		{
			"Phys. Einrichtungsnummer",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x6055,
		"Anlauf-Anforderung (YAAV) von der FDS mit Kennzeichen 'KERNANLAUF' erhalten.",
		1,
		{
			"Phys. Einrichtungsnummer",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x6056,
		"Die Funktionsfaehigkeit des PFG ist innerhalb von 4 Minuten nicht erreicht worden (Anlauf mit der FDS nicht abgeschlossen).",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#+1",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x6057,
		"Resetursache des PFG (Power-on-Reset oder Baugruppen-Reset).",
		2,
		{
			"Phys. Einrichtungsnummer",
			"Kennzeichen fuer Art des Resets #VSTANL#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x6058,
		"Wiederholt sporadische HW-Fehler oder Datenverfaelschung auf der seriellen Schnittstelle zwischen FDS und PFG festgestellt.",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Systemmeldungs-Nummer  (High Byte)",
			"Systemmeldungs-Nummer  (Low Byte)",
			"Eskalationsschwellwert",
			"", "", "", "", "", "",
		},
	},

	{
		0x6059,
		"Ein unbekannter Anlaufgrund wurde im PFG festgestellt.",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Abbild der HW-Stoerungsregister bei Fehlererkennung #HWSTRN#",
			"Abbild der HW-Stoerungsregister bei Fehlererkennung #HWSTRN#+1",
			"Fortsetzungsadresse des laufenden Tasks (High Byte)",
			"Fortsetzungsadresse des laufenden Tasks (Low Byte)",
			"", "", "", "", "",
		},
	},

	{
		0x605d,
		"Datenverfaelschung (falsches Unterprogramm fuer die Betriebsparameteruebernahme wurde aufgerufen).",
		1,
		{
			"Phys. Einrichtungsnummer",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x605e,
		"Anlaufquittung (YAQV) nicht innerhalb von 5 Sekunden nach Absenden der Anlaufanforderung (YAAx) erhalten.",
		1,
		{
			"Phys. Einrichtungsnummer",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x605f,
		"Ausfall der seriellen Schnittstelle zur FDS erkannt (Break-Aufall)",
		1,
		{
			"Phys. Einrichtungsnummer",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x6060,
		"Sporadischer HW-Fehler bei der Eigenpruefung des PFG erkannt (Empfaengerlaufzeit in konzentrierter Signalisierung ausserhalb der Toleranz).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Empfangstaktphase #RRPHAS#",
			"Feldstaerke des Empfangssignals",
			"Jittermomentanwert",
			"Ergebnisse ueber KORS #Entwickler-Info#",
			"", "", "", "", "",
		},
	},

	{
		0x6061,
		"Sporadischer HW-Fehler bei der Eigenpruefung des PFG erkannt (falsche Daten bei der Empfaengerlaufzeit- decodierung festgestellt).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Decodersteuerbyte #FKSDC#",
			"Feldstaerke des Empfangssignals",
			"Jittermomentanwert",
			"Ergebnisse ueber KORS #Entwickler-Info#",
			"", "", "", "", "",
		},
	},

	{
		0x6062,
		"Sporadischer HW-Fehler bei der Eigenpruefung des PFG erkannt (Modulatorlaufzeit in konzentrierter Signalisierung ausserhalb der Toleranz).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Korrigierte Empfaengerlaufzeit #VLAUFE#",
			"Feldstaerke des Empfangssignals",
			"Jittermomentanwert",
			"Ergebnisse ueber KORS #Entwickler-Info#",
			"", "", "", "", "",
		},
	},

	{
		0x6063,
		"Sporadischer HW-Fehler bei der Eigenpruefung des PFG erkannt (falsche Daten bei der Modulatorlaufzeit- decodierung erkannt).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Decodersteuerbyte #FKSDC#",
			"Feldstaerke des Empfangssignals",
			"Jittermomentanwert",
			"Ergebnisse ueber KORS #Entwickler-Info#",
			"", "", "", "", "",
		},
	},

	{
		0x6064,
		"Sporadischer HW-Fehler bei der Eigenpruefung des PFG erkannt (bei der Decodierung einer verteilten Signal- isierung wurde ein Fehler erkannt).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Decodersteuerbyte #FKSDC#",
			"Feldstaerke des Empfangssignals",
			"Jittermomentanwert",
			"Ergebnisse ueber KORS #Entwickler-Info#",
			"", "", "", "", "",
		},
	},

	{
		0x6065,
		"Sporadischer HW-Fehler bei der Eigenpruefung des PFG erkannt (ungueltiges Ergebnis der SINAD-Messung vom Schwellwertentscheider).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Ergebnis am Schwellwertentscheider #Entwickler-Info#",
			"Feldstaerke des Empfangssignals",
			"Jittermomentanwert",
			"Ergebnisse ueber KORS #Entwickler-Info#",
			"", "", "", "", "",
		},
	},

	{
		0x6066,
		"Sporadischer HW-Fehler bei der Eigenpruefung des PFG erkannt (bei der SINAD-Messung wurden falsche Daten beim Decodieren festgestellt).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Decodersteuerbyte #FKSDC#",
			"Feldstaerke des Empfangssignals",
			"Jittermomentanwert",
			"Ergebnisse ueber KORS #Entwickler-Info#",
			"", "", "", "", "",
		},
	},

	{
		0x6067,
		"Sporadischer HW-Fehler bei der Eigenpruefung des PFG erkannt (ungueltiges Schwellwertentscheiderergebnis bei Amplitudengangmessung 0.3kHz).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Ergebnis am Schwellwertentscheider #Entwickler-Info#",
			"Feldstaerke des Empfangssignals",
			"Jittermomentanwert",
			"Ergebnisse ueber KORS #Entwickler-Info#",
			"", "", "", "", "",
		},
	},

	{
		0x6068,
		"Sporadischer HW-Fehler bei der Eigenpruefung des PFG erkannt (bei Amplitudengangmessung 0.3kHz wurden beim Decodieren falsche Daten festgestellt).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Decodersteuerbyte #FKSDC#",
			"Feldstaerke des Empfangssignals",
			"Jittermomentanwert",
			"Ergebnisse ueber KORS #Entwickler-Info#",
			"", "", "", "", "",
		},
	},

	{
		0x6069,
		"Sporadischer HW-Fehler bei der Eigenpruefung des PFG erkannt (ungueltiges Schwellwertentscheiderergebnis bei Amplitudengangmessung 1.0kHz).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Ergebnis am Schwellwertentscheider #Entwickler-Info#",
			"Feldstaerke des Empfangssignals",
			"Jittermomentanwert",
			"Ergebnisse ueber KORS #Entwickler-Info#",
			"", "", "", "", "",
		},
	},

	{
		0x606a,
		"Sporadischer HW-Fehler bei der Eigenpruefung erkannt (bei der Amplitudengangmessung 1.0kHz wurden beim Decodieren falsche Daten festgestellt).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Decodersteuerbyte #FKSDC#",
			"Feldstaerke des Empfangssignals",
			"Jittermomentanwert",
			"Ergebnisse ueber KORS #Entwickler-Info#",
			"", "", "", "", "",
		},
	},

	{
		0x606b,
		"Sporadischer HW-Fehler bei der Eigenpruefung des PFG erkannt (ungueltiges Schwellwertentscheiderergebnis bei Amplitudengangmessung 2.3kHz).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Ergebnis am Schwellwertentscheider #Entwickler-Info#",
			"Feldstaerke des Empfangssignals",
			"Jittermomentanwert",
			"Ergebnisse ueber KORS #Entwickler-Info#",
			"", "", "", "", "",
		},
	},

	{
		0x606c,
		"Sporadischer HW-Fehler bei der Eigenpruefung des PFG erkannt (bei der Amplitudengangmessung 2.3kHz wurden beim Decodieren falsche Daten festgestellt).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Decodersteuerbyte #FKSDC#",
			"Feldstaerke des Empfangssignals",
			"Jittermomentanwert",
			"Ergebnisse ueber KORS #Entwickler-Info#",
			"", "", "", "", "",
		},
	},

	{
		0x6071,
		"Sporadischer HW-Fehler bei der Eigenpruefung des PFG erkannt (ungueltiges Ergebnis aus Schwellwertent- scheider beim Betrieb Sender verschleiert und Empfaenger klar).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Ergebnis am Schwellwertentscheider #Entwickler-Info#",
			"Feldstaerke des Empfangssignals",
			"Jittermomentanwert",
			"Ergebnisse ueber KORS #Entwickler-Info#",
			"", "", "", "", "",
		},
	},

	{
		0x6072,
		"Sporadischer HW-Fehler bei der Eigenpruefung des PFG erkannt (beim Betrieb Sender verschleiert und Empfaenger klar wurden beim Decodieren falsche Daten festgestellt).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Decodersteuerbyte #FKSDC#",
			"Feldstaerke des Empfangssignals",
			"Jittermomentanwert",
			"Ergebnisse ueber KORS #Entwickler-Info#",
			"", "", "", "", "",
		},
	},

	{
		0x6075,
		"Fehler bei der Prozesskommunikation (Cancelauftrag (WMCANC) war bei einer OGK-Pruefung nicht erfolgreich).",
		2,
		{
			"Phys. Einrichtungsnummer",
			"Flag fuer HW-Belegung #FLHWBL#+1",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x6076,
		"Plausibilitaetsfehler (bei einer OGK-Pruefung ist die HW mit einer externen Pruefung belegt).",
		2,
		{
			"Phys. Einrichtungsnummer",
			"Flag fuer HW-Belegung #FLHWBL#+1",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x6077,
		"Fehler bei der Prozesskommunikation (Cancelauftrag (WMCANC) war bei einer SPK-Pruefung nicht erfolgreich).",
		2,
		{
			"Phys. Einrichtungsnummer",
			"Flag fuer HW-Belegung #FLHWBL#+1",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x6078,
		"Plausibilitaetsfehler (bei einer SPK-Pruefung ist die HW mit einer externen Pruefung belegt).",
		2,
		{
			"Phys. Einrichtungsnummer",
			"Flag fuer HW-Belegung #FLHWBL#+1",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x6079,
		"Fehler bei der Prozesskommunikation (Cancelauftrag (WMCANC) war bei einer FME-Pruefung nicht erfolgreich).",
		2,
		{
			"Phys. Einrichtungsnummer",
			"Flag fuer HW-Belegung #FLHWBL#+1",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x607a,
		"Plausibilitaetsfehler (bei einer FME-Pruefung ist die HW mit einer externen Pruefung belegt).",
		2,
		{
			"Phys. Einrichtungsnummer",
			"Flag fuer HW-Belegung #FLHWBL#+1",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x607b,
		"Ablauf des Timeout bei einer FME-Pruefung (Pruefschritt nicht beendet, Start des naechsten Pruefschritts).",
		1,
		{
			"Phys. Einrichtungsnummer",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x607c,
		"Datenverfaelschung (keinen Ausgabepuffer erhalten).",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Steuerbyte des Coderpuffers #FKSDC#",
			"Flag fuer \"HW-Belegung\" #FLHWBL#+1",
			"Funkblockzaehlerstand #FBZAE#",
			"", "", "", "", "", "",
		},
	},

	{
		0x607d,
		"Plausibilitaetsfehler (Voraussetzungen fuer eine FME- Pruefung sind nicht erfuellt).",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Flag fuer \"Laufzeiteichung erfolgt\" #FLEIGP#",
			"Flag fuer \"VT Bereitschaft\" #VTBERT#",
			"Flag fuer \"ST Anlauf beendet\" #ANLFDS#",
			"", "", "", "", "", "",
		},
	},

	{
		0x607e,
		"Ablauf des Timeout bei einer FME-Pruefung (Pruefschritt nicht beendet, Start des naechsten Pruefschritts).",
		1,
		{
			"Phys. Einrichtungsnummer",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x607f,
		"Plausibilitaetsfehler (HW bei einer FME-Pruefung nicht belegt).",
		1,
		{
			"Phys. Einrichtungsnummer",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x6080,
		"Datenverfaelschung (keinen Ausgabepuffer erhalten).",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Steuerbyte des Coderpuffers #FKSDC#",
			"Flag fuer \"HW-Belegung\" #FLHWBL#+1",
			"Funkblockzaehlerstand #FBZAE#",
			"", "", "", "", "", "",
		},
	},

	{
		0x6081,
		"Datenverfaelschung in der Einrichtungsliste erkannt.",
		1,
		{
			"Phys. Einrichtungsnummer",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x6082,
		"Veraenderung in der Tabelle fuer die Einstelldaten der Synthesizer erkannt.",
		1,
		{
			"Phys. Einrichtungsnummer",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x6083,
		"Veraenderung des Laufzeitwertes des eigenen Empfaengers erkannt.",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Laufzeitwert  #VLAUFE#",
			"Checksum des Laufzeitwertes #VLAUEC#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x6084,
		"Veraenderung des Laufzeitwertes des eigenen Senders wurde erkannt",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Laufzeitwert #VLAUFS#",
			"Checksum des Laufzeitwertes #VLAUSC#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x6087,
		"Datenverfaelschung (kein Fehlermeldefach adressiert).",
		1,
		{
			"Phys. Einrichtungsnummer",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x6088,
		"Datenverfaelschung (unzulaessiger Index fuer die Fehlerklasse).",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Systemmeldungs-Nummer  (High Byte)",
			"Systemmeldungs-Nummer  (Low Byte)",
			"Falscher #Fehlerklassen-Index#",
			"", "", "", "", "", "",
		},
	},

	{
		0x6089,
		"Datenverfaelschung (unzulaessiger Index fuer Fehlermassnahmen).",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Systemmeldungs-Nummer (High Byte)",
			"Systemmeldungs-Nummer (Low Byte)",
			"Falscher #Fehlermassnahmen-Index#",
			"", "", "", "", "", "",
		},
	},

	{
		0x608a,
		"Bei wiederholtem Aussenden einer Systemmeldung (YALAY) keine Quittung von der FDS erhalten.",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Systemmeldungs-Nummer  (High Byte)",
			"Systemmeldungs-Nummer  (Low Byte)",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x6090,
		"Datenverfaelschung (falsche Massnahmen fuer Fehlerbehandlung gefordert; Massnahmen 6, 9 und 10 werden in der FUPEF nicht gefuehrt, Massnahme 12 wird nicht in diesem Modul realisiert (UP YSTAT)).",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Systemmeldungs-Nummer  (High Byte)",
			"Systemmeldungs-Nummer  (Low Byte)",
			"Falscher #Fehlermassnahmen-Index#",
			"", "", "", "", "", "",
		},
	},

	{
		0x6092,
		"Gewolltes Reset im PFG (zB: Anlauf-Anforderung erhalten oder Ausfall der seriellen Schnittstelle zur FDS erkannt)",
		1,
		{
			"Phys. Einrichtungsnummer",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x6093,
		"Bei wiederholtem Aussenden einer Systemmeldung (YALAY) keine Quittung von der FDS erhalten.",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Systemmeldungs-Nummer  (High Byte)",
			"Systemmeldungs-Nummer  (Low Byte)",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x6094,
		"Plausibilitaetsfehler (falsches Aufrufkennzeichen fuer Zeitbewertung eines HW-Fehlers wurde erkannt).",
		2,
		{
			"Phys. Einrichtungsnummer",
			"Falsches #Aufrufkennzeichen#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x6095,
		"Wiederholt sporadischer Fehler bei der Eigenpruefung im PFG festgestellt (Statistikueberlauf FBH 8).",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Systemmeldungs-Nummer (High Byte)",
			"Systemmeldungs-Nummer (Low Byte)",
			"Eskalationsschwellwert",
			"", "", "", "", "", "",
		},
	},

	{
		0x6096,
		"Wiederholt sporadischer Fehler erkannt",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Systemmeldungs-Nummer (High Byte)",
			"Systemmeldungs-Nummer (Low Byte)",
			"Eskalationsschwellwert",
			"", "", "", "", "", "",
		},
	},

	{
		0x6097,
		"Eine Veraenderung in der Tabelle der Statistikschwell= werte wurde festgestellt.",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Ist-Checksum",
			"Soll-Checksum",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x6098,
		"Eine Verfaelschung der auszusendenden Systemmeldungs- Nummer wurde erkannt.",
		5,
		{
			"Phys. Einrichtungsnummer",
			"falsche Systemmeldungs-Nummer (High Byte)",
			"falsche Systemmeldungs-Nummer (Low Byte)",
			"Ist-Checksum",
			"Soll-Checksum",
			"", "", "", "", "",
		},
	},

	{
		0x6099,
		"Verfaelschung der auszusendenden Systemmeldungs-Nummer erkannt.",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Unzulaessige Systemmeldungs-Nummer (High Byte)",
			"Unzulaessige Systemmeldungs-Nummer (Low Byte)",
			"Ist-Checksum",
			"Soll-Checksum",
			"", "", "", "", "",
		},
	},

	{
		0x609a,
		"Unterbrechung der Verbindung zur FDS (Break-Ausfall < 3 sec. )",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Abbild HW-Stoerungsregister #HWSTRN#",
			"Abbild HW-Stoerungsregister #HWSTRN#+1",
			"Funkblockzaehler #FBZAE#",
			"", "", "", "", "", "",
		},
	},

	{
		0x609b,
		"Wiederholt sporadischer Fehler bei der Eigenpruefung im PFG festgestellt ( Statistikueberlauf FBH 14 )",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Systemmeldungsnummer (High Byte)",
			"Systemmeldungsnummer (Low Byte)",
			"Eskalationsschwellwert",
			"", "", "", "", "", "",
		},
	},

	{
		0x609c,
		"Sporadischer HW-Fehler bei der Eigenpruefung des PFG erkannt (Empfaengerlaufzeit in verteilter Signalisierung ausserhalb der Toleranz).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Mittelwert der Gesamtlaufzeit #VLZVGM#",
			"Feldstaerke des Empfangssignals",
			"Jittermomentanwert",
			"Ergebnisse ueber KORS #Entwickler-Info#",
			"", "", "", "", "",
		},
	},

	{
		0x609d,
		"Sporadischer HW-Fehler bei der Eigenpruefung des PFG erkannt (Modulatorlaufzeit in verteilter Signalisierung ausserhalb der Toleranz).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Korrigierte Empfaengerlaufzeit #VLAUVE#",
			"Feldstaerke des Empfangssignals",
			"Jittermomentanwert",
			"Ergebnisse ueber KORS #Entwickler-Info#",
			"", "", "", "", "",
		},
	},

	{
		0x7000,
		"Sende- Empfangsteilerketten laenger als 1 Rahmen asynchron (FTAK).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#+1",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#+1",
			"", "", "", "", "",
		},
	},

	{
		0x7001,
		"Watch-Dog hat angesprochen (WADOG).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#+1",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#+1",
			"", "", "", "", "",
		},
	},

	{
		0x7002,
		"Rahmensetz-Signal QSET ist laenger als 1 Rahmen ausgefallen (FQSET).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#+1",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#+1",
			"", "", "", "", "",
		},
	},

	{
		0x7003,
		"Sendeteilerkette laenger als 1 Rahmen ausgefallen (FSTK).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#+1",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#+1",
			"", "", "", "", "",
		},
	},

	{
		0x7004,
		"Nicht alle Baugruppen gesteckt oder ein Kontaktfehler (BGOK)",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#+1",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#+1",
			"", "", "", "", "",
		},
	},

	{
		0x7005,
		"Synthesizer-Lockkriterium hat angesprochen (SYLOK0).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#+1",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#+1",
			"", "", "", "", "",
		},
	},

	{
		0x7006,
		"Lockkriterium Synthesizer 1 hat angesprochen (SYLOK1)",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#+1",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#+1",
			"", "", "", "", "",
		},
	},

	{
		0x7007,
		"Lockkriterium Synthesizer 2 hat angesprochen (SYLOK2)",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#+1",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x7008,
		"Lockkriterium Synthesizer 3 hat angesprochen (SYLOK3)",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#+1",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#+1",
			"", "", "", "", "",
		},
	},

	{
		0x7018,
		"Waehrend des FME-Anlaufes sind nicht alle HW-Stoerungen abgeklungen.",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#+1",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x7019,
		"Der Decoder ist defekt.",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Rahmenzaehlerstand (High Byte)",
			"Rahmenzaehlerstand (Low Byte)",
			"Funkblockzaehlerstand #FBZAE#",
			"", "", "", "", "", "",
		},
	},

	{
		0x701a,
		"Der Coder ist defekt.",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Rahmenzaehlerstand (High Byte)",
			"Rahmenzaehlerstand (Low Byte)",
			"Funkblockzaehlerstand #FBZAE#",
			"", "", "", "", "", "",
		},
	},

	{
		0x701c,
		"HW-Mehrfach-Fehler wurde erkannt.",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#+1",
			"Massnahmenverursachender Fehler #MVF#",
			"", "", "", "", "", "",
		},
	},

	{
		0x701d,
		"Lockkriterium Modulator hat angesprochen (MODLOK) (wird nur 1 x gemeldet, da Modulator nicht verwendet).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#",
			"Abbild der HW-Stoerungen bei Fehlererkennung #HWSTRN#+1",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#",
			"Abbild der HW-Stoerungen vor Fehlererkennung #HWSTRA#+1",
			"", "", "", "", "",
		},
	},

	{
		0x7028,
		"Betriebsmittelmangel (alle Fehlermeldefaecher sind belegt-FETAB).",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Systemmeldungs-Nummer (Low Byte)",
			"",
			"Anzahl der momentan aktiven Prozesse",
			"", "", "", "", "", "",
		},
	},

	{
		0x7029,
		"Betriebsmittelmangel (FIFO-Ueberlauf fuer Puffer zur FDS).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Fortsetzungsadresse des aufrufenden Prozesses (High Byte)",
			"Fortsetzungsadresse des aufrufenden Prozesses (Low Byte)",
			"Speicherbanknummer des aufrufenden Prozesses #BNR#",
			"Anzahl der momentan aktiven Prozesse",
			"", "", "", "", "",
		},
	},

	{
		0x702a,
		"Betriebsmittelmangel (kein VT-Puffer fuer Ausgabe zur FDS frei).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Fortsetzungsadresse des aufrufenden Prozesses (Low Byte)",
			"Fortsetzungsadresse des aufrufenden Prozesses (High Byte)",
			"Speicherbanknummer des aufrufenden Prozesses #BNR#",
			"Anzahl der momentan aktiven Prozesse",
			"", "", "", "", "",
		},
	},

	{
		0x702b,
		"Betriebsmittelmangel (FIFO-Ueberlauf fuer interne Puffer).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Fortsetzungsadresse des aufrufenden Prozesses (High Byte)",
			"Fortsetzungsadresse des aufrufenden Prozesses (Low Byte)",
			"Speicherbanknummer des aufrufenden Prozesses #BNR#",
			"Anzahl der momentan aktiven Prozesse",
			"", "", "", "", "",
		},
	},

	{
		0x702c,
		"Betriebsmittelmangel (FIFO fuer Warten auf FDS-Meldung uebergelaufen).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Fortsetzungsadresse des vertagten Prozesses (High Byte)",
			"Fortsetzungsadresse des vertagten Prozesses (Low Byte)",
			"Speicherbanknummer des vertagten Prozesses #BNR#",
			"Anzahl der momentan aktiven Prozesse",
			"", "", "", "", "",
		},
	},

	{
		0x702d,
		"Betriebsmittelmangel (Tabellenueberlauf fuer DE-Vertagung).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Fortsetzungsadresse des vertagten Prozesses (High Byte)",
			"Fortsetzungsadresse des vertagten Prozesses (Low Byte)",
			"Speicherbanknummer des vertagten Prozesses #BNR#",
			"Anzahl der momentan aktiven Prozesse",
			"", "", "", "", "",
		},
	},

	{
		0x702e,
		"Betriebsmittelmangel (Tabellenueberlauf fuer FR-Vertagung)",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Fortsetzungsadresse des vertagten Prozesses (High Byte)",
			"Fortsetzungsadresse des vertagten Prozesses (Low Byte)",
			"Speicherbanknummer des vertagten Prozesses #BNR#",
			"Anzahl der momentan aktiven Prozesse",
			"", "", "", "", "",
		},
	},

	{
		0x702f,
		"Betriebsmittelmangel (Tabellenueberlauf fuer IR-Vertagung)",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Fortsetzungsadresse des vertagten Prozesses (High Byte)",
			"Fortsetzungsadresse des vertagten Prozesses (Low Byte)",
			"Speicherbanknummer des vertagten Prozesses #BNR#",
			"Anzahl der momentan aktiven Prozesse",
			"", "", "", "", "",
		},
	},

	{
		0x7030,
		"Datenverfaelschung (unzulaessige Vertagung bei Prozessrueckkehr).",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Fortsetzungsadresse des vertagten Prozesses (High Byte)",
			"Fortsetzungsadresse des vertagten Prozesses (Low Byte)",
			"Speicherbanknummer des vertagten Prozesses #BNR#",
			"", "", "", "", "", "",
		},
	},

	{
		0x7031,
		"Sporadischer HW-Fehler (unzulaessiger Interrupt RST 7.5).",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Rahmenzaehlerstand (High Byte)",
			"Rahmenzaehlerstand (Low Byte)",
			"Funkblockzaehlerstand #FBZAE#",
			"", "", "", "", "", "",
		},
	},

	{
		0x7032,
		"Datenverfaelschung (Anstoss fuer Prozessstart mit einer unzulaessigen Startquelle).",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Unzulaessige Startquelle",
			"Uebergebener #Opcode#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x7033,
		"Sporadischer HW-Fehler (Dauer-Break auf der seriellen Schnittstelle).",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Rahmenzaehlerstand (High Byte)",
			"Rahmenzaehlerstand (Low Byte)",
			"Funkblockzaehlerstand #FBZAE#",
			"", "", "", "", "", "",
		},
	},

	{
		0x7034,
		"Datenverfaelschung (3 Zeitplaetze wurden aus der Zuteilungsmeldung ermittelt).",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Rahmenzaehlerstand (High Byte)",
			"Rahmenzaehlerstand (Low Byte)",
			"Funkblockzaehlerstand #FBZAE#",
			"", "", "", "", "", "",
		},
	},

	{
		0x7035,
		"Datenverfaelschung (Empfang einer FDS-Meldung mit einer falschen FKS-Nummer).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Falsche FKS-Nummer aus der FDS-Meldung",
			"Rahmenzaehlerstand (High Byte)",
			"Rahmenzaehlerstand (Low Byte)",
			"Funkblockzaehlerstand #FBZAE#",
			"", "", "", "", "",
		},
	},

	{
		0x7036,
		"Datenverfaelschung (interner Prozessstart mit einem unzulaessigen Opcode).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Unzulaessiger #Opcode#",
			"Rahmenzaehlerstand (High Byte)",
			"Rahmenzaehlerstand (Low Byte)",
			"Funkblockzaehlerstand #FBZAE#",
			"", "", "", "", "",
		},
	},

	{
		0x7037,
		"Datenverfaelschung (unzulaessiger Opcode fuer Prozessstart von der FDS).",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Unzulaessiger #Opcode#",
			"#Ident-Nummer#",
			"Funkblockzaehlerstand #FBZAE#",
			"", "", "", "", "", "",
		},
	},

	{
		0x7038,
		"Datenverfaelschung (unzulaessiger Opcode fuer Prozessstart vom Funk - gilt nur fuer OSK ).",
		1,
		{
			"Phys. Einrichtungsnummer",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x7039,
		"Betriebsmittelmangel (kein Prozesspeicher fuer den zu startenden Prozess frei).",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Startadresse des Prozesses (High Byte)",
			"Startadresse des Prozesses (Low Byte)",
			"Funkblockzahlerstand #FBZAE#",
			"", "", "", "", "", "",
		},
	},

	{
		0x703a,
		"FDS-Meldung nicht im erwarteten Zeitraum empfangen (Timeout bei Empfang auf der seriellen Schnittstelle).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Rahmenzaehlerstand (High Byte)",
			"Rahmenzaehlerstand (Low Byte)",
			"Funkblockzaehlerstand #FBZAE#",
			"Interruptzaehler des OS",
			"", "", "", "", "",
		},
	},

	{
		0x703b,
		"Datenverfaelschung (Checksum Fehler in einer Signalisierung von der FDS).",
		3,
		{
			"Phys. Einrichtungsnummer",
			"#Opcode#",
			"#Ident-Nummer#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x703c,
		"Sporadischer HW-Fehler (Parity oder Frame Fehler des USARTs bei Empfang einer FDS-Signalisierung).",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Rahmenzaehlerstand (High Byte)",
			"Rahmenzaehlerstand (Low Byte)",
			"Funkblockzaehlerstand #FBZAE#",
			"", "", "", "", "", "",
		},
	},

	{
		0x703d,
		"Betriebsmittelmangel (Pool-Ueberlauf bei den Zusatzspeichern).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Fortsetzungsadresse des aufrufenden Prozesses (High Byte)",
			"Fortsetzungsadresse des aufrufenden Prozesses (Low Byte)",
			"Speicherbanknummer des aufrufenden Prozesses #BNR#",
			"Anzahl der momentan aktiven Prozesse",
			"", "", "", "", "",
		},
	},

	{
		0x703e,
		"Sporadischer HW-Fehler oder Datenverfaelschung (HW- und SW-Funkblockzaehler sind asynchron).",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Zaehlerstand des SW-Funkblockzaehlers #FBZAE#",
			"Zaehlerstand des HW-Funkblockzaehlers #FRBZAE#",
			"Rahmenzaehlerstand (High Byte)",
			"Rahmenzaehlerstand (Low Byte)",
			"", "", "", "", "",
		},
	},

	{
		0x703f,
		"Korrelationsprogramm zu spaet aufgerufen",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Fortsetzungsadresse des letzten Prozesses (High Byte)",
			"Fortsetzungsadresse des letzten Prozesses (Low Byte)",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x7040,
		"Programmieren eines nicht vorhandenen Synthesizers",
		2,
		{
			"Phys. Einrichtungsnummer",
			"Synthesizernummer",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x7041,
		"Index der Synthesizer Einstellwerte-Tabelle ausserhalb des Wertebereichs",
		2,
		{
			"Phys. Einrichtungsnummer",
			"Falscher Wert des Index",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x7042,
		"Geforderte Synthesizeranschaltung ungueltig.",
		2,
		{
			"Phys. Einrichtungsnummer",
			"Synthesizernummer",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x7044,
		"Wiederholt wurde ein falscher Opcode in der Funksignalisierung eines Teilnehmers erkannt (gilt nur fuer OSK im OGK-Betrieb - Zusatzindizien siehe EW0029)",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Byte 1 der Funksignalisierung (TLN-Restnummer Low Byte)",
			"Byte 2 der Funksignalisierung (TLN-Restnummer High Byte)",
			"Byte 3 der Funksignalisierung (TLN-Nationalitaet u. -UELE-Nummer)",
			"Byte 4 der Funksignalisierung (FUZ-Restnummer)",
			"", "", "", "", "",
		},
	},

	{
		0x7045,
		"Zusatzindizien zu Systemmeldung EW0028",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Byte 5 der Funksignalisierung (FUZ-Nationalitaet und Uele-Nummer)",
			"Byte 9 der Funksignalisierung #Opcode#",
			"Funkblockzaehlerstand #FBZAE#",
			"", "", "", "", "", "",
		},
	},

	{
		0x7047,
		"Unterprogramm fuer Uebergabe des Data-Recording-Puffers hat eine Veraenderung der Checksum ueber den Pufferkopf erkannt.",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Byte 1 des Data-Recording-Puffer-Kopfes",
			"Byte 2 des Data-Recording-Puffer-Kopfes",
			"Byte 3 des Data-Recording-Puffer-Kopfes",
			"", "", "", "", "", "",
		},
	},

	{
		0x7054,
		"Bei der Kommunikationspruefung zwischen dem FME und der FDS langt kein Auftrag der FDS ein (2x hintereinander).",
		1,
		{
			"Phys. Einrichtungsnummer",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x7055,
		"Anlauf-Anforderung (YAAV) von der FDS mit Kennzeichen 'KERNANLAUF' erhalten.",
		1,
		{
			"Phys. Einrichtungsnummer",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x7056,
		"Die Funktionsfaehigkeit des FME ist innerhalb von 4 Minuten nicht erreicht worden (Anlauf mit der FDS nicht abgeschlossen).",
		1,
		{
			"Phys. Einrichtungsnummer",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x7057,
		"Power-On oder Baugruppen-Reset",
		2,
		{
			"Phys. Einrichtungsnummer",
			"Kennzeichen fuer Art des Resets #VSTANL#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x7058,
		"Wiederholt sporadische HW-Fehler oder Datenverfaelschung auf der seriellen Schnittstelle zwischen FDS und FME festgestellt.",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Systemmeldungs-Nummer  (High Byte)",
			"Systemmeldungs-Nummer  (Low Byte)",
			"Eskalationsschwellwert",
			"", "", "", "", "", "",
		},
	},

	{
		0x7059,
		"Ein unbekannter Anlaufgrund wurde im FME festgestellt.",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Abbild der HW-Stoerungsregister bei Fehlererkennung #HWSTRN#",
			"Abbild der HW-Stoerungsregister bei Fehlererkennung #HWSTRN#+1",
			"Fortsetzungsadresse des laufenden Tasks (High Byte)",
			"Fortsetzungsadresse des laufenden Tasks (Low Byte)",
			"", "", "", "", "",
		},
	},

	{
		0x705a,
		"Datenverfaelschung (Falsche Aktion im Synthesizerdescriptor)",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Adresse des Synthesizerdescriptors (High Byte)",
			"Adresse des Synthesizerdescriptors (Low Byte)",
			"Verfaelschtes Datum   #VSYDS.KSYACT#",
			"", "", "", "", "", "",
		},
	},

	{
		0x705b,
		"Datenverfaelschung (Falscher Empfaengertyp)",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Adresse des Empfaengerdescriptors  (High Byte)",
			"Adresse des Empfaengerdescriptors  (Low Byte)",
			"Verfaelschtes Datum   #VRXDS.KRXTYP#",
			"", "", "", "", "", "",
		},
	},

	{
		0x705c,
		"Datenverfaelschung (Falsche Aktion im Synthesizerdescriptor)",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Adresse des Synthesizerdescriptors (High Byte)",
			"Adresse des Synthesizerdescriptors (Low Byte)",
			"Verfaelschtes Datum   #VSYDS.KSYACT#",
			"", "", "", "", "", "",
		},
	},

	{
		0x705d,
		"Datenverfaelschung (Nicht belegter Synthesizer wurde dem Einstellprozess uebergeben)",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Adresse des Synthesizerdescriptors  (High Byte)",
			"Adresse des Synthesizerdescriptors  (Low Byte)",
			"Verfaelschtes Datum   #VSYDS.KSYZST#",
			"", "", "", "", "", "",
		},
	},

	{
		0x705e,
		"Datenverfaelschung (Falscher Synthesizertyp)",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Adresse des Synthesizerdescriptors (High Byte)",
			"Adresse des Synthesizerdescriptors (Low Byte)",
			"Verfaelschtes Datum   #VSYDS.KSYTYP#",
			"", "", "", "", "", "",
		},
	},

	{
		0x705f,
		"Ablauffehler (Synthesizereinstellwerte wurden wiederholt vom OS nicht uebernommen)",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Adresse des Synthesizerdescriptors (High Byte)",
			"Adresse des Synthesizerdescriptors (Low Byte)",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x7060,
		"Datenverfaelschung (Falsche Synthesizernummer)",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Adresse des Synthesizerdescriptors  (High Byte)",
			"Adresse des Synthesizerdescriptors  (Low Byte)",
			"Verfaelschtes Datum   #VSYDS.KSYNR#",
			"", "", "", "", "", "",
		},
	},

	{
		0x7061,
		"Datenverfaelschung (Synthesizer ist nicht belegt (Scannen))",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Adresse des Synthesizerdescriptors  (High Byte)",
			"Adresse des Synthesizerdescriptors  (Low Byte)",
			"Verfaelschtes Datum   #VSYDS.KSYZST#",
			"", "", "", "", "", "",
		},
	},

	{
		0x7062,
		"Datenverfaelschung (Empfaenger ist nicht belegt (Scannen))",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Adresse des Empfaengerdescriptors  (High Byte)",
			"Adresse des Empfaengerdescriptors  (Low Byte)",
			"Verfaelschtes Datum   #VRXDS.KRXZST#",
			"", "", "", "", "", "",
		},
	},

	{
		0x7063,
		"Datenverfaelschung (BS-Typ (Kennung) falsch)",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Adresse des Kanaldescriptors (High Byte)",
			"Adresse des Kanaldescriptors (Low Byte)",
			"Falsches Datum   #SBKTYP#",
			"", "", "", "", "", "",
		},
	},

	{
		0x7064,
		"Datenverfaelschung (BS-Typ (Kennung) eines NBF falsch)",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Adresse des Kanaldescriptors (High Byte)",
			"Adresse des Kanaldescriptors (Low Byte)",
			"Falsches Datum   #FKTAB.I.KKFTYP#",
			"", "", "", "", "", "",
		},
	},

	{
		0x7065,
		"Plausibilitaetsfehler (Korrigierte Entfernung wird bei Subtraktion der Empfaengerlaufzeit, oder der internen Geraetelaufzeit, oder des Korrekturfakors negativ; keine Entfernungsbewertung moeglich); Zusatzindizien: ET0012",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Pegelmomentanwert",
			"Kanalindex (1 - 40T)",
			"FME - Entfernungsmesswert  #FKTAB.I.KKENMO#",
			"SPK - Entfernungsmesswert  #FKTAB.I.KKENTT#",
			"", "", "", "", "",
		},
	},

	{
		0x7066,
		"Zusatzindizien zu ET0011",
		5,
		{
			"Phys. Einrichtungsnummer",
			"MS-Nummer  (Byte 1)",
			"MS-Nummer  (Byte 2)",
			"MS-Nummer  (Byte 3)",
			"Anzahl der hintereinander erfolgten Identifikationen dieser MS, bei der neg. Entfernung errechnet wurde",
			"", "", "", "", "",
		},
	},

	{
		0x7068,
		"Datenverfaelschung (Frequenzgenauigkeit ungueltig).",
		2,
		{
			"Phys. Einrichtungsnummer",
			"Wert der Frequenzgenauigkeit",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x7069,
		"Ablauffehler (Messauftrag, obwohl noch keine VT - Freigabe erfolgt ist)",
		1,
		{
			"Phys. Einrichtungsnummer",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x706a,
		"Messauftrag fuer Frequenz, die von diesem FME nicht ueberwacht wird",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Frequenznummer (Low Byte)",
			"Frequenznummer (High Byte)",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x706b,
		"Datenverfaelschung des Wertes der Empfaengerlaufzeit (Checksum-Fehler)",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Empfaengerlaufzeit               #ICLINT#",
			"Checksum der Empfaengerlaufzeit  #ICLINC#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x706e,
		"Ablauffehler (Rechnerspez. Anlauf falsch aufgerufen)",
		1,
		{
			"Phys. Einrichtungsnummer",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x706f,
		"Ablauffehler im Anlauf ((YAQV) nicht innerhalb 5 Sec.)",
		1,
		{
			"Phys. Einrichtungsnummer",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x7070,
		"Schnittstelle zu uebergeordneter Einrichtung (DKV) ausgefallen",
		1,
		{
			"Phys. Einrichtungsnummer",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x7071,
		"Falscher Betriebsparameter (Typ (Kennung) der eigenen BS aus Anlagenliste empfangen)",
		2,
		{
			"Phys. Einrichtungsnummer",
			"Falscher Wert des Betriebspar. #SA1TYP#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x7072,
		"Falscher Betriebsparameter (Relative Entfernungsangabe der eigenen BS aus Anlagenliste empfangen)",
		2,
		{
			"Phys. Einrichtungsnummer",
			"Falscher Wert d. Betrpar.    #IA1RAD#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x7073,
		"Falscher Betriebsparameter (Umschalttoleranz der eigenen BS aus Anlagenliste empfangen)",
		2,
		{
			"Phys. Einrichtungsnummer",
			"Falscher Wert des Betriebspar.   #IA1TOL#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x7074,
		"Falscher Betriebsparameter (Einschalten Pegelbewertung der eigenen BS aus Anlagenliste empfangen)",
		2,
		{
			"Phys. Einrichtungsnummer",
			"Falscher Wert des Betriebspar.   #BA1PON#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x7075,
		"Falscher Betriebsparameter (Anzahl Messungen fuer Mittelung der Feldstaerke aus Anlagenliste empfangen)",
		2,
		{
			"Phys. Einrichtungsnummer",
			"Falscher Wert des Betriebspar.   #IA1MIF#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x7076,
		"Falscher Betriebsparameter (BS-Typ (Kennung) eines NBF aus Anlagenliste empfangen)",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Falscher Wert des Betriebspar.  #FA2NBF.I.KA2TYP#",
			"NBF-Elementnummer",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x7077,
		"Falscher Betriebsparameter (Relative Entfernungsangabe eines NBF aus Anlagenliste empfangen)",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Falscher Wert des Betriebspar.  #FA2NBF.I.KA2RAD#",
			"NBF-Elementnummer",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x7078,
		"Falscher Betriebsparameter (Umschalttoleranz eines NBF aus Anlagenliste empfangen)",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Falscher Wert des Betriebspar.  #FA2NBF.I.KA2TOL#",
			"NBF-Elementnummer",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x7079,
		"Falscher Betriebsparameter (Einschalten Pegelbewertung eines NBF aus Anlagenliste empfangen)",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Falscher Wert des Betriebspar.  #FA2NBF.I.KA2PON#",
			"NBF-Elementnummer",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x707a,
		"Falscher Betriebsparameter (Ausstattung FME aus Anlagenliste empfangen (Synthesizeranzahl >1 bei alter HW))",
		2,
		{
			"Phys. Einrichtungsnummer",
			"Falscher Wert des Betriebspar.   #SA3HW#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x707c,
		"Falscher Betriebsparameter (Ausstattung FME aus Anlagenliste empfangen (Synthesizeranzahl <> 4 bei neuer HW))",
		2,
		{
			"Phys. Einrichtungsnummer",
			"Falscher Wert des Betriebspar.   #SA3HW#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x707d,
		"Falscher Betriebsparameter (Zu ueberwachende Sprechfrequenz eines NBF aus Anlagenliste empfangen)",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Falscher Betriebspar.  #FA4CHA.I.KA4CHL#",
			"Falscher Betriebspar.  #FA4CHA.I.KA4CHL#+1",
			"Kanal-Elementnummer",
			"", "", "", "", "", "",
		},
	},

	{
		0x707e,
		"Falscher Betriebsparameter ((NBF-Index) aus Anlagenliste erhalten)",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Falscher Wert des Betriebspar.  #FA4CHA.I.KA4IFU#",
			"Kanal-Elementnummer",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x707f,
		"Ablauffehler in der ST-FEP erkannt (Zeitbedingung nicht erfuellt).",
		1,
		{
			"Phys. Einrichtungsnummer",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x7080,
		"Ablauffehler in der FT erkannt (Zeitbedingung beim Beenden der FT nicht erfuellt).",
		1,
		{
			"Phys. Einrichtungsnummer",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x7081,
		"Ablauffehler (FEP im falschen Funkblock aufgerufen)",
		1,
		{
			"Phys. Einrichtungsnummer",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x7086,
		"Ablauffehler in der FEP erkannt (Zeitbedingung der FEP nicht erfuellt).",
		1,
		{
			"Phys. Einrichtungsnummer",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x7087,
		"Datenverfaelschung in der Einrichtungsliste",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Checksum - Istwert  #EICHEK#",
			"Checksum - Sollwert",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x708a,
		"Datenverfaelschung (kein Fehlermeldefach adressiert).",
		1,
		{
			"Phys. Einrichtungsnummer",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x708b,
		"Datenverfaelschung (unzulaessiger Index fuer die Fehlerklasse).",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Systemmeldungs-Nummer  (High Byte)",
			"Systemmeldungs-Nummer  (Low Byte)",
			"Falscher #Fehlerklassen-Index#",
			"", "", "", "", "", "",
		},
	},

	{
		0x708c,
		"Datenverfaelschung (unzulaessiger Index fuer Fehlermassnahmen).",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Systemmeldungs-Nummer  (High Byte)",
			"Systemmeldungs-Nummer  (Low Byte)",
			"Falscher #Fehlermassnahmen-Index#",
			"", "", "", "", "", "",
		},
	},

	{
		0x708d,
		"Bei wiederholtem Aussenden einer Systemmeldung (YALAY) keine Quittung von der FDS erhalten (FBH 2).",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Systemmeldungs-Nummer  (High Byte)",
			"Systemmeldungs-Nummer  (Low Byte)",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x7093,
		"Datenverfaelschung (falsche Massnahmen fuer Fehlerbehandlung gefordert; Massnahmen 6, 9 und 10 werden in der FUPEF nicht gefuehrt, Massnahme 12 wird nicht in diesem Modul realisiert (UP YSTAT)).",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Systemmeldungs-Nummer  (High Byte)",
			"Systemmeldungs-Nummer  (Low Byte)",
			"Falscher #Fehlermassnahmen-Index#",
			"", "", "", "", "", "",
		},
	},

	{
		0x7095,
		"Gewolltes Reset im FME (zB: Anlauf-Anforderung erhalten oder Ausfall der seriellen Schnittstelle zur FDS erkannt)",
		1,
		{
			"Phys. Einrichtungsnummer",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x7096,
		"Bei wiederholtem Aussenden einer Systemmeldung (YALAY) keine Quittung von der FDS erhalten.",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Systemmeldungs-Nummer  (High Byte)",
			"Systemmeldungs-Nummer  (Low Byte)",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x7097,
		"Plausibilitaetsfehler (falsches Aufrufkennzeichen fuer Zeitbewertung eines HW-Fehlers wurde erkannt).",
		2,
		{
			"Phys. Einrichtungsnummer",
			"Falsches #Aufrufkennzeichen#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x7098,
		"Sprechfrequenz, fuer die eine Identifikation erfolgt, wurde in keinem Kanaldescriptor der Kanaltabelle gefunden.",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Sprechfrequenz (LOW)",
			"Sprechfrequenz (HIGH)   #FKTAB.I.KKCLOG#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x7099,
		"Sprechfrequenz, fuer die ein Scannprozess laeuft, wurde in keinem Kanaldescriptor der Kanaltabelle gefunden.",
		3,
		{
			"Phys. Einrichtungsnummer",
			"Sprechfrequenz  (LOW)",
			"Sprechfrequenz  (HIGH)   #FKTAB.I.KKCLOG#",
			"", "", "", "", "", "", "",
		},
	},

	{
		0x709a,
		"Falscher Betriebsparameter (Klein-/Grossleistung der BS aus Anlagenliste empfangen)",
		2,
		{
			"Phys. Einrichtungsnummer",
			"Falscher Wert des Betriebspar. #IAKLGR#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x709b,
		"Falscher Betriebsparameter (Testanlagen-Parameter aus Anlagenliste empfangen)",
		2,
		{
			"Phys. Einrichtungsnummer",
			"Falscher Wert des Betriebspar. #IATEAN#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x709c,
		"Falscher Betriebsparameter (Laenderkennzeichen aus Anlagenliste empfangen)",
		2,
		{
			"Phys. Einrichtungsnummer",
			"Falscher Wert des Betriebspar. #IALAND#",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x709d,
		"Wiederholt sporadischer Fehler bei der Eigenpruefung im FME festgestellt (Statistikueberlauf FBH 8).",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Systemmeldungs-Nummer (High Byte)",
			"Systemmeldungs-Nummer (Low Byte)",
			"Eskalationsschwellwert",
			"", "", "", "", "", "",
		},
	},

	{
		0x709e,
		"Wiederholt sporadischer Fehler erkannt",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Systemmeldungs-Nummer (High Byte)",
			"Systemmeldungs-Nummer (Low Byte)",
			"Eskalationsschwellwert",
			"", "", "", "", "", "",
		},
	},

	{
		0x709f,
		"Datenverfaelschung im Feld ABRUST erkannt",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Systemmeldungsnummer (High Byte)",
			"Systemmeldungsnummer (Low Byte)",
			"Ist-Checksum",
			"Soll-Checksum",
			"", "", "", "", "",
		},
	},

	{
		0x70a0,
		"Verfaelschung der auszusendenden Systemmeldungs-Nummer erkannt.",
		5,
		{
			"Phys. Einrichtungsnummer",
			"Unzulaessige Systemmeldungs-Nummer (High Byte)",
			"Unzulaessige Systemmeldungs-Nummer (Low Byte)",
			"Ist-Checksum",
			"Soll-Checksum",
			"", "", "", "", "",
		},
	},

	{
		0x70a1,
		"Unterbrechung der Verbindung zur FDS ( Break-Ausfall < 3 sec. )",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Abbild HW-Stoerungsregister #HWSTRN#",
			"Abbild HW-Stoerungsregister #HWSTRN#+1",
			"Funkblockzaehler #FBZAE#",
			"", "", "", "", "", "",
		},
	},

	{
		0x70a2,
		"Ueberlauf der Fehlerstatistik",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Systemmeldungsnummer (High Byte)",
			"Systemmeldungsnummer (Low Byte)",
			"Eskalationsschwellwert",
			"", "", "", "", "", "",
		},
	},

	{
		0x70a3,
		"Datenverfaelschung (falsche Synthesizernummer)",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Adresse des Synthesizerdescriptors (High Byte)",
			"Adresse des Synthesizerdescriptors (Low Byte)",
			"Verfaelschtes Datum   #VSYDS.KSYNR#",
			"", "", "", "", "", "",
		},
	},

	{
		0x70a4,
		"Datenverfaelschung (Synthesizer ist nicht belegt (EICHEN))",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Adresse des Synthesizerdescriptors (High Byte)",
			"Adresse des Synthesizerdescriptors (Low Byte)",
			"Verfaelschtes Datum   #VSYDS.KSYZST#",
			"", "", "", "", "", "",
		},
	},

	{
		0x70a5,
		"Datenverfaelschung (Empfaenger ist nicht belegt (Eichen))",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Adresse des Empfaengerdescriptors (High Byte)",
			"Adresse des Empfaengerdescriptors (Low Byte)",
			"Verfaelschtes Datum   #VRXDS.KRXZST#",
			"", "", "", "", "", "",
		},
	},

	{
		0x70a7,
		"Datenverfaelschung (Synthesizer ist nicht belegt (Eichen))",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Adresse des Synthesizerdescriptors (High Byte)",
			"Adresse des Synthesizerdescriptors (Low Byte)",
			"Verfaelschtes Datum   #VSYDS.KSYZST#",
			"", "", "", "", "", "",
		},
	},

	{
		0x70a8,
		"Datenverfaelschung (Empfaenger ist nicht belegt (Eichen))",
		4,
		{
			"Phys. Einrichtungsnummer",
			"Adresse des Empfaengerdescriptors (High Byte)",
			"Adresse des Empfaengerdescriptors (Low Byte)",
			"Verfaelschtes Datum   #VRXDS.KRXZST#",
			"", "", "", "", "", "",
		},
	},

	{
		0x70a9,
		"Gemessene Feldstaerke des Eichtongenerators befand sich zu lange ausserhalb des erlaubten Pegelfensters.",
		2,
		{
			"Phys. Einrichtungsnummer",
			"Feldstaerke",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x70aa,
		"Standardabweichung (der Eichwerte) groesser als erlaubt.",
		2,
		{
			"Phys. Einrichtungsnummer",
			"Standardabweichung",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x70ab,
		"Gemessene Feldstaerke des Eichtongenerators ausserhalb des erlaubten Pegelfensters.",
		2,
		{
			"Phys. Einrichtungsnummer",
			"Feldstaerke",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x70ad,
		"Falscher Betriebsparameter (bei Bahn-BS stimmt HW-Typ oder Synthesizer-Anzahl nicht)",
		4,
		{
			"Phys. Einrichtungsnummer",
			"BS-Typ #SBKTYP#",
			"HW-Ausstattung #SA3HW#",
			"HW-Typ #HWTYP#",
			"", "", "", "", "", "",
		},
	},

	{
		0x8000,
		"Watchdog RESET Auf Grund des RESETS hat der PBR einen Anlauf durchgefuehrt. Der Systemmeldungsspeicher bleibt erhalten, ebenso die zugehoerigen Verwaltungsgroessen.",
		1,
		{
			"Physikalische Einrichtungs-Nummer des PBR",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x8001,
		"Watchdog Reset Auf Grund des RESETS hat der PBR einen Anlauf durchgefuehrt. Der Systemmeldungsspeicher bleibt erhalten, ebenso die zugehoerigen Verwaltungsgroessen. Als Grund fuer den watch dog reset wurde \"PROM-Fehler\" festgestellt.",
		1,
		{
			"Physikalische Einrichtungs-Nummer des PBR",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x8002,
		"Watchdog Reset Auf Grund des RESETS hat der PBR einen Anlauf durchgefuehrt. Der Systemmeldungsspeicher bleibt erhalten, ebenso die zugehoerigen Verwaltungsgroessen. Als Grund fuer den watch dog reset wurde \"RAM-Fehler\" festgestellt.",
		1,
		{
			"Physikalische Einrichtungs-Nummer des PBR",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x8004,
		"Watchdog RESET Auf Grund des RESETS hat der PBR einen Anlauf durchgefuehrt. Der Systemmeldungsspeicher bleibt erhalten, ebenso die zugehoerigen Verwaltungsgroessen. Als Grund fuer den watch dog reset wurde ein Ueberlauf der Prozess-Kommunikationsqueue festgestellt.",
		1,
		{
			"Physikalische Einrichtungs-Nummer des PBR",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x8005,
		"Watchdog RESET Auf Grund des RESETS hat der PBR einen Anlauf durchgefuehrt. Der Systemmeldungsspeicher bleibt erhalten, ebenso die zugehoerigen Verwaltungsgroessen. Als Grund fuer den watch dog reset wurde eine Ueberlauf des Betriebsmittels \"Kachelspeicher\" festgestellt.",
		1,
		{
			"Physikalische Einrichtungs-Nummer des PBR",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x8008,
		"Tasten - RESET Der Bediener hat die Reset - Taste betaetigt. Es wurde ein Anlauf ausgefuehrt. Alle Speicher wurden geloescht und neu initialisiert.",
		1,
		{
			"Physikalische Einrichtungs-Nummer des PBR",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x8010,
		"Spannungsreset Es wurde ein Anlauf ausgefuehrt. Alle Speicher sind geloescht und wurden neu initialisiert.",
		1,
		{
			"Physikalische Einrichtungs-Nummer des PBR",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x8011,
		"                  Bedienerinterface (BIF) BS-Alarm Der PBR hat einen BS-Alarm festgestellt. Am PBT werden alle Indizien angezeigt. An die FDS werden nur die ersten 4 Bytes (ohne physikalische Einrichtungs-Nummer des PBR) abgesetzt, wenn die Systemmeldung zum Entstehungszeitpunkt an die FDS absetzbar war (ueber Signalisierung YALAY). Konnte die Systemmeldung nicht abgesetzt werden und fordert die FDS sie mit YLMEV an, so werden alle Indizien (inklusive physikalische Einrichtungs-Nummer des PBR) mit Hilfe der Signalierung YLUSB an die FDS uebertragen.",
		4,
		{
			"Anzahl gepufferte Systemmeldungen (high Byte)",
			"Anzahl gepufferte Systemmeldungen (low Byte)",
			"Anzahl ueberschriebene Systemmeldungen (high Byte)",
			"Anzahl ueberschriebene Systemmeldungen (low Byte) (Anzahl der Systemmeldungen ist jeweils hexadezimal.)",
			"", "", "", "", "", "",
		},
	},

	{
		0x8012,
		"KOP- Ausfall Der PBR hat den Ausfall der KOP festgestellt. Am PBT werden alle Indizien angezeigt. An die FDS werden nur die ersten 4 Bytes (ohne physikalische Einrichtungs-Nummer des PBR) abgesetzt, wenn die Systemmeldung zum Entstehungszeitpunkt an die FDS absetzbar war (ueber Signalisierung YALAY). Konnte die Systemmeldung nicht abgesetzt werden und fordert die FDS sie mit YLMEV an, so werden alle Indizien (inklusive physikalische Einrichtungs-Nummer des PBR) mit Hilfe der Signalierung YLUSB an die FDS uebertragen.",
		4,
		{
			"Anzahl gepufferte Systemmeldungen (high Byte)",
			"Anzahl gepufferte Systemmeldungen (low Byte)",
			"Anzahl ueberschriebene Systemmeldungen (high Byte)",
			"Anzahl ueberschriebene Systemmeldungen (low Byte) (Anzahl der Systemmeldungen ist jeweils hexadezimal.)",
			"", "", "", "", "", "",
		},
	},

	{
		0x8013,
		"Versorgungstakt- Pruefung Der PBR konnte waehrend seines Anlaufes keinen Versorgungstakt feststellen. Die Schnittstelle zur FDS kann nicht bedient werden.",
		1,
		{
			"Physikalische Einrichtungs-Nummer des PBR",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x8014,
		"Versorgungstakt-Ueberwachung Im laufenden Betrieb hat der PBR den Ausfall des Versorgungstaktes festgestellt. Die Schnittstelle zur FDS kann nicht mehr bedient werden.",
		1,
		{
			"Physikalische Einrichtungs-Nummer des PBR",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x8016,
		"SETZ- Taktueberwachung Im laufenden Betrieb hat der PBR den Ausfall des SETZ- Taktes festgestellt. Die Schnittstelle zur FDS kann nicht mehr bedient werden.",
		1,
		{
			"Physikalische Einrichtungs-Nummer des PBR",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x8017,
		"                  Bedienerinterface (BIF)                   Anlaufsignalisierung (AS) Falsche Signalisierungsfolgenummer Der PBR hat beim Empfang einer Signalisierungsfolge einen Fehler in der Reihenfolge der Signalisierungsfolgenummern erkannt. Die Indizien enthalten die Signalisierung mit der als fehlerhaft erkannten Signalisierungsnummer. Am PBT werden alle Indizien angezeigt. An die FDS werden nur die ersten 4 Bytes (ohne physikalische Einrichtungs-Nummer des PBR) abgesetzt, wenn die Systemmeldung zum Entstehungszeitpunkt an die FDS absetzbar war (ueber Signalisierung YALAY). Konnte die Systemmeldung nicht abgesetzt werden und fordert die FDS sie mit YLMEV an, so werden alle Indizien (inklusive physikalische Einrichtungs-Nummer des PBR) mit Hilfe der Signalierung YLUSB an die FDS uebertragen.",
		10,
		{
			"Physikalische Einrichtungs-Nummer des PBR",
			"Signalisierungsbyte 0 (#Opcode#) der Signalisierung",
			"Signalisierungsbyte 1 (#Ident-Nummer#) der Signalisierung",
			"Signalisierungsbyte 2 der Signalisierung",
			"Signalisierungsbyte 3 der Signalisierung",
			"Signalisierungsbyte 4 der Signalisierung",
			"Signalisierungsbyte 5 der Signalisierung",
			"Signalisierungsbyte 6 der Signalisierung",
			"Signalisierungsbyte 7 der Signalisierung",
			"Signalisierungsbyte 8 der Signalisierung",
		},
	},

	{
		0x8018,
		"                  Meldungsverwaltung (MEV)                   Meldungsverwaltung (MV) Falsche DKV- Nummer Der PBR hat beim Empfang einer Signalisierungsfolge einen Fehler in der signalisierten DKV- Nummer erkannt. Die Indizien enthalten die Signalisierung mit der als fehlerhaft erkannten DKV- Nummer. Die DKV- Nummer ist in den Indizien nicht sichtbar. Am PBT werden alle Indizien angezeigt. An die FDS werden nur die ersten 4 Bytes (ohne physikalische Einrichtungs-Nummer des PBR) abgesetzt, wenn die Systemmeldung zum Entstehungszeitpunkt an die FDS absetzbar war (ueber Signalisierung YALAY). Konnte die Systemmeldung nicht abgesetzt werden und fordert die FDS sie mit YLMEV an, so werden alle Indizien (inklusive physikalische Einrichtungs-Nummer des PBR) mit Hilfe der Signalierung YLUSB an die FDS uebertragen.",
		10,
		{
			"Physikalische Einrichtungs-Nummer des PBR",
			"Signalisierungsbyte 0 (#Opcode#) der Signalisierung",
			"Signalisierungsbyte 1 (#Ident-Nummer#) der Signalisierung",
			"Signalisierungsbyte 2 der Signalisierung",
			"Signalisierungsbyte 3 der Signalisierung",
			"Signalisierungsbyte 4 der Signalisierung",
			"Signalisierungsbyte 5 der Signalisierung",
			"Signalisierungsbyte 6 der Signalisierung",
			"Signalisierungsbyte 7 der Signalisierung",
			"Signalisierungsbyte 8 der Signalisierung",
		},
	},

	{
		0x8019,
		"Falsche Laenge der Signalisierungsfolge Die dem PBR bekannte maximale Laenge einer Signalisierungsfolge wurde ueberschritten. Die Indizien enthalten die Signalisierung mit der als fehlerhaft erkannten Laenge. Am PBT werden alle Indizien angezeigt. An die FDS werden nur die ersten 4 Bytes (ohne physikalische Einrichtungs-Nummer des PBR) abgesetzt, wenn die Systemmeldung zum Entstehungszeitpunkt an die FDS absetzbar war (ueber Signalisierung YALAY). Konnte die Systemmeldung nicht abgesetzt werden und fordert die FDS sie mit YLMEV an, so werden alle Indizien (inklusive physikalische Einrichtungs-Nummer des PBR) mit Hilfe der Signalierung YLUSB an die FDS uebertragen.",
		10,
		{
			"Physikalische Einrichtungs-Nummer des PBR",
			"Signalisierungsbyte 0 (#Opcode#) der Signalisierung",
			"Signalisierungsbyte 1 (#Ident-Nummer#) der Signalisierung",
			"Signalisierungsbyte 2 der Signalisierung",
			"Signalisierungsbyte 3 der Signalisierung",
			"Signalisierungsbyte 4 der Signalisierung",
			"Signalisierungsbyte 5 der Signalisierung",
			"Signalisierungsbyte 6 der Signalisierung",
			"Signalisierungsbyte 7 der Signalisierung",
			"Signalisierungsbyte 8 der Signalisierung",
		},
	},

	{
		0x801a,
		"                  Bedienerinterface  (BIF)                   Bedienerinterface FDS Schnittstelle (BF) Falscher Operationscode Der PBR hat eine Signalisierung mit einem nicht definierten Operationscode empfangen. Die Indizien enthalten die Signalisierung mit dem als fehlerhaft erkannten Operationscode. Am PBT werden alle Indizien angezeigt. An die FDS werden nur die ersten 4 Bytes (ohne physikalische Einrichtungs-Nummer des PBR) abgesetzt, wenn die Systemmeldung zum Entstehungszeitpunkt an die FDS absetzbar war (ueber Signalisierung YALAY). Konnte die Systemmeldung nicht abgesetzt werden und fordert die FDS sie mit YLMEV an, so werden alle Indizien (inklusive physikalische Einrichtungs-Nummer des PBR) mit Hilfe der Signalierung YLUSB an die FDS uebertragen.",
		10,
		{
			"Physikalische Einrichtungs-Nummer des PBR",
			"Signalisierungsbyte 0 (#Opcode#) der Signalisierung",
			"Signalisierungsbyte 1 (#Ident-Nummer#) der Signalisierung",
			"Signalisierungsbyte 2 der Signalisierung",
			"Signalisierungsbyte 3 der Signalisierung",
			"Signalisierungsbyte 4 der Signalisierung",
			"Signalisierungsbyte 5 der Signalisierung",
			"Signalisierungsbyte 6 der Signalisierung",
			"Signalisierungsbyte 7 der Signalisierung",
			"Signalisierungsbyte 8 der Signalisierung",
		},
	},

	{
		0x801b,
		"Falsche Pruefsumme in der Signalisierung. Der PBR hat eine Signalisierung mit einem falschen Pruefbyte empfangen. Die Indizien enthalten die Signalisierung mit dem als fehlerhaft erkannten Pruefbyte. Das Pruefbyte ist in den Indizien nicht sichtbar. Am PBT werden alle Indizien angezeigt. An die FDS werden nur die ersten 4 Bytes (ohne physikalische Einrichtungs-Nummer des PBR) abgesetzt, wenn die Systemmeldung zum Entstehungszeitpunkt an die FDS absetzbar war (ueber Signalisierung YALAY). Konnte die Systemmeldung nicht abgesetzt werden und fordert die FDS sie mit YLMEV an, so werden alle Indizien (inklusive physikalische Einrichtungs-Nummer des PBR) mit Hilfe der Signalierung YLUSB an die FDS uebertragen.",
		10,
		{
			"Physikalische Einrichtungs-Nummer des PBR",
			"Signalisierungsbyte 0 (#Opcode#) der Signalisierung",
			"Signalisierungsbyte 1 (#Ident-Nummer#) der Signalisierung",
			"Signalisierungsbyte 2 der Signalisierung",
			"Signalisierungsbyte 3 der Signalisierung",
			"Signalisierungsbyte 4 der Signalisierung",
			"Signalisierungsbyte 5 der Signalisierung",
			"Signalisierungsbyte 6 der Signalisierung",
			"Signalisierungsbyte 7 der Signalisierung",
			"Signalisierungsbyte 8 der Signalisierung",
		},
	},

	{
		0x801d,
		"Falsche PBR- Einrichtungsnummer Der PBR hat festgestellt, dass eine Signalisierung empfangen wurde, die nicht mit der gueltigen PBR- Einrichtungsnummer versehen ist. Die Indizien enthalten die Signalisierung mit der als fehlerhaft erkannten Einrichtungsnummer. Die Einrichtungsnummer ist in den Indizien nicht sichtbar. Am PBT werden alle Indizien angezeigt. An die FDS werden nur die ersten 4 Bytes (ohne physikalische Einrichtungs-Nummer des PBR) abgesetzt, wenn die Systemmeldung zum Entstehungszeitpunkt an die FDS absetzbar war (ueber Signalisierung YALAY). Konnte die Systemmeldung nicht abgesetzt werden und fordert die FDS sie mit YLMEV an, so werden alle Indizien (inklusive physikalische Einrichtungs-Nummer des PBR) mit Hilfe der Signalierung YLUSB an die FDS uebertragen.",
		10,
		{
			"Physikalische Einrichtungs-Nummer des PBR",
			"Signalisierungsbyte 0 (#Opcode#) der Signalisierung",
			"Signalisierungsbyte 1 (#Ident-Nummer#) der Signalisierung",
			"Signalisierungsbyte 2 der Signalisierung",
			"Signalisierungsbyte 3 der Signalisierung",
			"Signalisierungsbyte 4 der Signalisierung",
			"Signalisierungsbyte 5 der Signalisierung",
			"Signalisierungsbyte 6 der Signalisierung",
			"Signalisierungsbyte 7 der Signalisierung",
			"Signalisierungsbyte 8 der Signalisierung",
		},
	},

	{
		0x801e,
		"Ueberlauf der MV-Queue Der PBR hat festgestellt, dass zu viele Signalisierungen auf die Uebertragung zur FDS hin warten. Die letzte im PBR erzeugte Signalisierung wurde verworfen.",
		1,
		{
			"Physikalische Einrichtungs-Nummer des PBR",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x801f,
		"                  Bedienerinterface  (BIF)                   Anlauf und Alarmsteuerung (AAS)                   Bedienerinterface FDS Schnitstelle (BF)                   Anlaufsignalisierungsueberwachung  (AS) Ueberwachungszeit abgelaufen Der PBR hat innerhalb einer festgelegten Zeit eine Signalisierungsfolge nicht vollstaendig empfangen oder auf eine Auftragssignalisierung keine Antwort erhalten. Die Indizien enthalten im ersten Fall die Signalisierung, die als letzte empfangen wurde. Die Indizien enthalten im zweiten Fall die Signalisierung, die als Auftragssignalisierung an die FDS gesendet wurde. Am PBT werden alle Indizien angezeigt. An die FDS werden nur die ersten 4 Bytes (ohne physikalische Einrichtungs-Nummer des PBR) abgesetzt, wenn die Systemmeldung zum Entstehungszeitpunkt an die FDS absetzbar war (ueber Signalisierung YALAY). Konnte die Systemmeldung nicht abgesetzt werden und fordert die FDS sie mit YLMEV an, so werden alle Indizien (inklusive physikalische Einrichtungs-Nummer des PBR) mit Hilfe der Signalierung YLUSB an die FDS uebertragen.",
		10,
		{
			"Physikalische Einrichtungs-Nummer des PBR",
			"Signalisierungsbyte 0 (#Opcode#) der Signalisierung",
			"Signalisierungsbyte 1  der Signalisierung",
			"Signalisierungsbyte 2 der Signalisierung",
			"Signalisierungsbyte 3 der Signalisierung",
			"Signalisierungsbyte 4 der Signalisierung",
			"Signalisierungsbyte 5 der Signalisierung",
			"Signalisierungsbyte 6 der Signalisierung",
			"Signalisierungsbyte 7 der Signalisierung",
			"Signalisierungsbyte 8 der Signalisierung",
		},
	},

	{
		0x8020,
		"Falscher Quittungsparameter Der PBR hat in einer Quittungsignalisierung einen Parameterwert gefunden, der ausserhalb des fuer ihn definierten Wertebereiches liegt. Die Indizien beinhalten die als fehlerhaft erkannte Signalisierung.",
		10,
		{
			"Physikalische Einrichtungs-Nummer des PBR",
			"Signalisierungsbyte 0 (#Opcode#) der Signalisierung",
			"Signalisierungsbyte 1 der Signalisierung",
			"Signalisierungsbyte 2 der Signalisierung",
			"Signalisierungsbyte 3 der Signalisierung",
			"Signalisierungsbyte 4 der Signalisierung",
			"Signalisierungsbyte 5 der Signalisierung",
			"Signalisierungsbyte 6 der Signalisierung",
			"Signalisierungsbyte 7 der Signalisierung",
			"Signalisierungsbyte 8 der Signalisierung",
		},
	},

	{
		0x8021,
		"PBR Login Der PBR hat einen Login ausgefuehrt, der von der FDS nicht quittiert wurde. Am PBT werden alle Indizien angezeigt. An die FDS werden nur die ersten 4 Bytes (ohne physikalische Einrichtungs-Nummer des PBR) abgesetzt, wenn die Systemmeldung zum Entstehungszeitpunkt an die FDS absetzbar war (ueber Signalisierung YALAY). Konnte die Systemmeldung nicht abgesetzt werden und fordert die FDS sie mit YLMEV an, so werden alle Indizien (inklusive physikalische Einrichtungs-Nummer des PBR) mit Hilfe der Signalierung YLUSB an die FDS uebertragen.",
		6,
		{
			"Physikalische Einrichtungs-Nummer des PBR",
			"Passworttyp (0 - 4)",
			"Kommandoabsender des PBR:   (Wert 0)",
			"Alarmanzeigen an PBR:      0  Nein,  1  Ja",
			"Systemmeldungen an MSC:    0  Nein,  1  Ja",
			"Synchronisation der Timer (Wert = FFH , Beginn der LOGIN-Session)",
			"", "", "", "",
		},
	},

	{
		0x8023,
		"BREAK1 Ausfall Der PBR hat den Ausfall des BREAK- Signales 1 festgestellt. Am PBT werden alle Indizien angezeigt. An die FDS werden nur die ersten 4 Bytes (ohne physikalische Einrichtungs-Nummer des PBR) abgesetzt, wenn die Systemmeldung zum Entstehungszeitpunkt an die FDS absetzbar war (ueber Signalisierung YALAY). Konnte die Systemmeldung nicht abgesetzt werden und fordert die FDS sie mit YLMEV an, so werden alle Indizien (inklusive physikalische Einrichtungs-Nummer des PBR) mit Hilfe der Signalierung YLUSB an die FDS uebertragen.",
		4,
		{
			"Anzahl gepufferte Systemmeldungen (high Byte)",
			"Anzahl gepufferte Systemmeldungen (low Byte)",
			"Anzahl ueberschriebene Systemmeldungen (high Byte)",
			"Anzahl ueberschriebene Systemmeldungen (low Byte) (Anzahl der Systemmeldungen ist jeweils hexadezimal.)",
			"", "", "", "", "", "",
		},
	},

	{
		0x8024,
		"Ungleichheit der Betriebsparameter-Pruefsummenermittlung Die in den Betriebsparametern gelieferte Pruefsumme stimmt nicht ueberein mit der im PBR erzeugten Pruefsumme. Die Indizien beinhalten die als fehlerhaft erkannte Signalisierung mit der Pruefsumme, die von der FDS geliefert wurde. Am PBT werden alle Indizien angezeigt. An die FDS werden nur die ersten 4 Bytes (ohne physikalische Einrichtungs-Nummer des PBR) abgesetzt, wenn die Systemmeldung zum Entstehungszeitpunkt an die FDS absetzbar war (ueber Signalisierung YALAY). Konnte die Systemmeldung nicht abgesetzt werden und fordert die FDS sie mit YLMEV an, so werden alle Indizien (inklusive physikalische Einrichtungs-Nummer des PBR) mit Hilfe der Signalierung YLUSB an die FDS uebertragen.",
		10,
		{
			"Physikalische Einrichtungs-Nummer des PBR",
			"Signalisierungsbyte 0 (#Opcode#) der Signalisierung",
			"Signalisierungsbyte 1, Signalisierungsnummer",
			"Signalisierungsbyte 2, Byte 1 Pruefsumme",
			"Signalisierungsbyte 3, Byte 2 Pruefsumme",
			"Signalisierungsbyte 4 der Signalisierung",
			"Signalisierungsbyte 5 der Signalisierung",
			"Signalisierungsbyte 6 der Signalisierung",
			"Signalisierungsbyte 7 der Signalisierung",
			"Signalisierungsbyte 8 der Signalisierung",
		},
	},

	{
		0x8025,
		"Von der FDS geforderter Anlauf Im laufenden Betrieb wurde von der FDS eine Anlaufanforderung empfangen.",
		1,
		{
			"Physikalische Einrichtungs-Nummer des PBR",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x8026,
		"FDS- bedingter Daueranlauf Die FDS hat in einer Stunde mehr als 10 Anlaeufe ausgefuehrt. Am PBT werden alle Indizien angezeigt. An die FDS werden nur die ersten 4 Bytes (ohne physikalische Einrichtungs-Nummer des PBR) abgesetzt, wenn die Systemmeldung zum Entstehungszeitpunkt an die FDS absetzbar war (ueber Signalisierung YALAY). Konnte die Systemmeldung nicht abgesetzt werden und fordert die FDS sie mit YLMEV an, so werden alle Indizien (inklusive physikalische Einrichtungs-Nummer des PBR) mit Hilfe der Signalierung YLUSB an die FDS uebertragen.",
		4,
		{
			"Anzahl gepufferte Systemmeldungen (high Byte)",
			"Anzahl gepufferte Systemmeldungen (low Byte)",
			"Anzahl ueberschriebene Systemmeldungen (high Byte)",
			"Anzahl ueberschriebene Systemmeldungen (low Byte) (Anzahl der Systemmeldungen ist jeweils hexadezimal.)",
			"", "", "", "", "", "",
		},
	},

	{
		0x8027,
		"BS-Ausfall, n mal keine Betriebsparameter empfangen. Der PBR hat eine definierte Anzahl von Versuchen unternommen, die Beriebsparameter anzufordern. Die definierte Zahl der Wiederholungen wurde ueberschritten. Am PBT werden alle Indizien angezeigt. An die FDS werden nur die ersten 4 Bytes (ohne physikalische Einrichtungs-Nummer des PBR) abgesetzt, wenn die Systemmeldung zum Entstehungszeitpunkt an die FDS absetzbar war (ueber Signalisierung YALAY). Konnte die Systemmeldung nicht abgesetzt werden und fordert die FDS sie mit YLMEV an, so werden alle Indizien (inklusive physikalische Einrichtungs-Nummer des PBR) mit Hilfe der Signalierung YLUSB an die FDS uebertragen.",
		4,
		{
			"Anzahl gepufferte Systemmeldungen (high Byte)",
			"Anzahl gepufferte Systemmeldungen (low Byte)",
			"Anzahl ueberschriebene Systemmeldungen (high Byte)",
			"Anzahl ueberschriebene Systemmeldungen (low Byte) (Anzahl der Systemmeldungen ist jeweils hexadezimal.)",
			"", "", "", "", "", "",
		},
	},

	{
		0x8028,
		"PBR Logoff Der PBR hat einen Fremd Logoff ausgefuehrt. Am PBT werden alle Indizien angezeigt.",
		2,
		{
			"Physikalische Einrichtungs-Nummer des PBR",
			"Logoff-Grund der FDS  ( 2 = Timeout, 3 = hoher priorer Auftrag, 4 = Fehlergruende)",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x8029,
		"BREAK2 Ausfall Der PBR hat den Ausfall des BREAK- Signals 2 festgestellt. Am PBT werden alle Indizien angezeigt. An die FDS werden nur die ersten 4 Bytes (ohne physikalische Einrichtungs-Nummer des PBR) abgesetzt, wenn die Systemmeldung zum Entstehungszeitpunkt an die FDS absetzbar war (ueber Signalisierung YALAY). Konnte die Systemmeldung nicht abgesetzt werden und fordert die FDS sie mit YLMEV an, so werden alle Indizien (inklusive physikalische Einrichtungs-Nummer des PBR) mit Hilfe der Signalierung YLUSB an die FDS uebertragen.",
		4,
		{
			"Anzahl gepufferte Systemmeldungen (high Byte)",
			"Anzahl gepufferte Systemmeldungen (low Byte)",
			"Anzahl ueberschriebene Systemmeldungen (high Byte)",
			"Anzahl ueberschriebene Systemmeldungen (low Byte) (Anzahl der Systemmeldungen ist jeweils hexadezimal.)",
			"", "", "", "", "", "",
		},
	},

	{
		0x802a,
		"Der Erreichbarkeitstest mit einem der Betriebsrechner verlief negativ.",
		2,
		{
			"Physikalische Einrichtungs-Nummer des PBR",
			"Betriebsrechner",
			"", "", "", "", "", "", "", "",
		},
	},

	{
		0x802b,
		"Das Modem im PBR-COM arbeitet fehlerhaft.",
		1,
		{
			"Physikalische Einrichtungs-Nummer des PBR",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x802c,
		"Die Batteriespannung im PBR-BED ist unter den fuer die Datenerhaltung notwendigen Pegel gesunken.",
		1,
		{
			"Physikalische Einrichtungs-Nummer des PBR",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x802d,
		"Die Checksum ueber den Inhalt des Festwertspeichers (PROM) des Subsystems PBR-BED ist fehlerhaft.",
		1,
		{
			"Physikalische Einrichtungs-Nummer des PBR",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x802e,
		"Der Test des Datenspeichers (RAM) im PBR-BED ist negativ verlaufen.",
		1,
		{
			"Physikalische Einrichtungs-Nummer des PBR",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x802f,
		"Das Dual Ported RAM (Schnittstelle PBR-BED und PBR-COM) arbeitet fehlerhaft.",
		1,
		{
			"Physikalische Einrichtungs-Nummer des PBR",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x8030,
		"Nichtuebertragene FDS-Alarme Wenn FDS-Alarme nicht an den Betriebsrechner uebertragen werden koennen, werden sie im Historyfile geparkt.",
		4,
		{
			"Physikalische Einrichtungs-Nummer des PBR",
			"Alarm 07  bitcodiert",
			"Alarm 815  bitcodiert",
			"Alarm 1619  bitcodiert",
			"", "", "", "", "", "",
		},
	},

	{
		0x8031,
		"Timerablauf bei der Quittierung der Auftragsfreigabe von der FDS.",
		1,
		{
			"Physikalische Einrichtungs-Nummer des PBR",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x8032,
		"Software-Fehler im PBR-COM-Teil.",
		1,
		{
			"Physikalische Einrichtungs-Nummer des PBR",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x8033,
		"Hardware-Fehler im PBR-COM-Teil.",
		1,
		{
			"Physikalische Einrichtungs-Nummer des PBR",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x8034,
		"PBR-COM-Teil hat einen Reset durchgefuehrt.",
		4,
		{
			"Physikalische Einrichtungs-Nummer des PBR",
			"Panic-Number (Integer im MOTOROLA-Format) (high Byte)",
			"Panic-Number (Integer im MOTOROLA-Format) (low Byte)",
			"Resetgrund (Codierung ?)",
			"", "", "", "", "", "",
		},
	},

	{
		0x8035,
		"Vom Betriebsrechner kommt unerwartet ein \"Disconnect-Indication\" auf Transportebene.",
		1,
		{
			"Physikalische Einrichtungs-Nummer des PBR",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x8036,
		"Der Ausweich-Betriebsrechner ist nach 10 Wahlwiederholungs- versuchen nicht erreichbar.",
		1,
		{
			"Physikalische Einrichtungs-Nummer des PBR",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x8037,
		"Der Regional-Betriebsrechner ist nach 10 Wahlwiederholungs- versuchen nicht erreichbar.",
		1,
		{
			"Physikalische Einrichtungs-Nummer des PBR",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x8038,
		"Der Zentral-Betriebsrechner ist nach 10 Wahlwiederholungs- versuchen nicht erreichbar.",
		1,
		{
			"Physikalische Einrichtungs-Nummer des PBR",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x8039,
		"Ablauf des Timers, der die Zeit vom Start des bedienerverursachten Verbindungsabbaus bis zum erwarteten \"Disconnect-Indication\" ueberwacht.",
		1,
		{
			"Physikalische Einrichtungs-Nummer des PBR",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x803a,
		"Timerablauf bei Ueberwachung des Datenaustauschs auf Applikationsebene.",
		1,
		{
			"Physikalische Einrichtungs-Nummer des PBR",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x803b,
		"Timerablauf auf Applikationsebene. Die Zeit zwischen Daten-Request und Daten-Confirmation wurde ueberschritten.",
		1,
		{
			"Physikalische Einrichtungs-Nummer des PBR",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x803c,
		"Timerablauf bei der Ueberwachung zwischen Ende einer vom Betriebsrechner gestarteten Applikation und dem erwarteten Verbindungsabbau.",
		1,
		{
			"Physikalische Einrichtungs-Nummer des PBR",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x803d,
		"Timerablauf bei der Ueberwachung eines vom PBR gesendeten Applikations-Requests und dessen Bestaetigung durch den Betriebsrechner.",
		1,
		{
			"Physikalische Einrichtungs-Nummer des PBR",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x803e,
		"Timerablauf beim Warten auf die Quittierung eines gesendeten Datenblocks auf Applikationsebene.",
		1,
		{
			"Physikalische Einrichtungs-Nummer des PBR",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x803f,
		"Timerablauf beim Verbindungsaufbau. Zeit zwischen T_CONNECT_RQ und T_CONNECT_CONFIRM war zu lang.",
		1,
		{
			"Physikalische Einrichtungs-Nummer des PBR",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x8040,
		"Die Batteriespannung im PBR-COM ist unter den fuer die Datenhaltung notwendigen Pegel gesunken.",
		1,
		{
			"Physikalische Einrichtungs-Nummer des PBR",
			"", "", "", "", "", "", "", "", "",
		},
	},

	{
		0x8041,
		"Fremdteilnehmer versucht, sich im PBR einzuloggen. Die DATEX-P-Nummer des Teilnehmers (max. 15 Ziffern) wird BCD-codiert (max. 7,5 Bytes) eingetragen und die restlichen freien Stellen mit FF(Hex) aufgefuellt.",
		10,
		{
			"Physikalische Einrichtungs-Nummer des PBR",
			"Fremdnummer BCD-codiert MSB",
			"Fremdnummer BCD-codiert",
			"Fremdnummer BCD-codiert",
			"Fremdnummer BCD-codiert",
			"Fremdnummer BCD-codiert",
			"Fremdnummer BCD-codiert",
			"Fremdnummer BCD-codiert",
			"Fremdnummer BCD-codiert LSB  (4 Bit)",
			"FF(Hex)",
		},
	},
};

void print_systemmeldung(uint16_t code, int bytes, uint8_t *ind)
{
	int i, ii, j;

	if (bytes > 10)
		bytes = 10;

	ii = sizeof(systemmeldungen) / sizeof(struct systemmeldungen);

	for (i = 0; i < ii; i++) {
		if (systemmeldungen[i].code == code)
			break;
	}
	if (i == ii)
		return;

	LOGP(DMUP, LOGL_INFO, " -> %s\n", systemmeldungen[i].desc);
	for (j = 0; j < systemmeldungen[i].bytes; j++)
		LOGP(DMUP, LOGL_INFO, "    Byte %d = %02Xh: %s\n", j, ind[j], systemmeldungen[i].ind[j]);
}

