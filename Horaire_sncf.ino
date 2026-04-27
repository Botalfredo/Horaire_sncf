#include <SPI.h>
#include <GxEPD2_3C.h>
#include <Fonts/FreeSans9pt7b.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include "secret.h"
#include "display.h"
#include "time.h"

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600;      // UTC+1
const int daylightOffset_sec = 3600;  // heure d'été

time_t prochainRafraichissement = 0;  // Stocke l'heure UNIX exacte du prochain refresh
unsigned long chronometreMinute = 0;

struct tm timeinfo;

#define MAX_TRAINS 10

// --- STRUCTURES  ---
struct MergedTrain {
  String numTrain;
  String prevu;
  String reel;
  String retardStr;
  String alerteMsg;
};

struct MergedData {
  MergedTrain trains[5];  // On limite strictement à 5 trains
  int count = 0;
};

MergedData affichageFinal;

struct JourneyTrain {
  String prevu;
  String reel;
  String alerte;
};

struct JourneyData {
  JourneyTrain trains[MAX_TRAINS];
  int count = 0;
};

// --- STRUCTURES POUR DEPARTURES ---
struct DepartureTrain {
  String numTrain;
  String prevu;
  String reel;
  String retardStr;
  String alerteMsg;
};

struct DepartureData {
  DepartureTrain trains[MAX_TRAINS];
  int count = 0;
};

JourneyData mesJourneys;
DepartureData mesDepartures;


void planifierProchainRafraichissement(const MergedData& data) {
  time_t maintenant;
  time(&maintenant);
  struct tm timeinfo;
  localtime_r(&maintenant, &timeinfo);

  // Conversion de l'heure actuelle en minutes depuis minuit
  int minutesAujourdhui = timeinfo.tm_hour * 60 + timeinfo.tm_min;

  int intervalleMinutes = 30;  // Par défaut : 30 minutes

  // Entre 15h30 (930 min) et 19h00 (1140 min)
  if (minutesAujourdhui >= (15 * 60 + 30) && minutesAujourdhui < (19 * 60)) {
    intervalleMinutes = 10;
  }

  time_t ciblePeriodique = maintenant + (intervalleMinutes * 60);
  time_t cibleTrain = 0;

  for (int i = 0; i < data.count; i++) {
    // On utilise l'heure prévue (HH:MM)
    int hTrain = data.trains[i].prevu.substring(0, 2).toInt();
    int mTrain = data.trains[i].prevu.substring(3, 5).toInt();

    // On crée un objet temps pour ce train
    struct tm trainTm = timeinfo;
    trainTm.tm_hour = hTrain;
    trainTm.tm_min = mTrain;
    trainTm.tm_sec = 0;  // On rafraîchit pile à la seconde 0 de la minute de départ

    time_t tTrain = mktime(&trainTm);

    // On cherche le PREMIER train qui n'est pas encore parti
    if (tTrain > maintenant) {
      cibleTrain = tTrain;
      break;  // Les trains étant triés, le premier trouvé est le bon
    }
  }

  if (cibleTrain != 0 && cibleTrain < ciblePeriodique) {
    prochainRafraichissement = cibleTrain;
    Serial.println("\n🎯 Prochain rafraichissement cale sur le depart d'un train.");
  } else {
    prochainRafraichissement = ciblePeriodique;
    Serial.printf("\n⏱️ Prochain rafraichissement periodique base sur rythme de %d min.\n", intervalleMinutes);
  }

  struct tm prochaineFois;
  localtime_r(&prochainRafraichissement, &prochaineFois);
  char buffer[6];
  strftime(buffer, sizeof(buffer), "%H:%M", &prochaineFois);
  Serial.printf("✅ Prochaine actualisation prevue a %s\n", buffer);
}

void setup() {
  Serial.begin(115200);
  pinMode(10, OUTPUT);
  pinMode(6, OUTPUT);
  pinMode(7, OUTPUT);

  delay(1000);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  WiFi.setHostname("INFO-455350");

  WiFi.begin(ssid, password);
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
  Serial.print("Connexion au WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nConnecté !");
  Serial.print("IP : ");
  Serial.println(WiFi.localIP());

  delay(100);
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org");
  Serial.println("Synchronisation NTP...");
  // Attente sync
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Erreur de récupération de l'heure");
  }
  Serial.println("Heure synchronisée !");
  if (getLocalTime(&timeinfo)) {
    printLocalTime();
  }

  SPI.begin(EPD_SCL, EPD_MISO, EPD_SDA, EPD_CS);
  display.init(115200, true, 2, false);
  display.setRotation(1);  // Paysage
  display.setFont(&FreeSans9pt7b);
  display.setFullWindow();
}

void loop() {
  time_t maintenant;
  time(&maintenant);

  // Si c'est le tout premier démarrage OU qu'il est l'heure de rafraîchir
  if (prochainRafraichissement == 0 || maintenant >= prochainRafraichissement) {

    Serial.println("=== Cycle API ecran ===");

    // 1. Collecte et fusion des données
    JourneyData mesJourneys;
    DepartureData mesDepartures;
    MergedData affichageFinal;

    fetchJourneysData(mesJourneys);
    fetchDepartures(mesDepartures);
    fusionnerDonnees(mesJourneys, mesDepartures, affichageFinal);
    printMergedData(affichageFinal);

    // 2. Affichage sur l'écran E-paper
    afficherHorairesTrains(affichageFinal);

    // 3. Calcul de la prochaine échéance
    planifierProchainRafraichissement(affichageFinal);

    // 4. On réinitialise le chronomètre pour le compte à rebours
    chronometreMinute = millis();
  }

  // -- COMPTE A REBOURS TOUTES LES MINUTES --
  // On vérifie si 60 000 millisecondes (1 minute) se sont écoulées
  if (millis() - chronometreMinute >= 60000) {
    chronometreMinute = millis();  // On remet le chrono à zéro pour la minute suivante
    time(&maintenant);             // On reprend l'heure exacte

    int secondesRestantes = prochainRafraichissement - maintenant;

    if (secondesRestantes > 0) {
      int minRestantes = secondesRestantes / 60;
      int secRestantes = secondesRestantes % 60;

      Serial.printf("⏳ Attente... Prochaine actualisation dans %d min et %d sec.\n", minRestantes, secRestantes);
    }
  }

  // Petite pause pour éviter que la boucle loop ne tourne à 100% du CPU pour rien
  delay(100);
}