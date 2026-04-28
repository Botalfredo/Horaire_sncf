#include <GxEPD2_3C.h>
#include <Fonts/FreeSans9pt7b.h>
#include <WiFi.h>

#include "secret.h"
#include "display.h"
#include "time.h"
#include "API.h"

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600;      // UTC+1
const int daylightOffset_sec = 3600;  // heure d'été
time_t prochainRafraichissement = 0;  // Stocke l'heure UNIX exacte du prochain refresh
unsigned long chronometreMinute = 0;

struct tm timeinfo;

void setup() {
  Serial.begin(115200);
  Serial.println("Démarrage Horaire de train");

  init_IO();

  init_screen();

  init_WiFi();

  init_Time();
}

void loop() {
  time_t maintenant;
  time(&maintenant);

  // Si c'est le tout premier démarrage OU qu'il est l'heure de rafraîchir
  if (prochainRafraichissement == 0 || maintenant >= prochainRafraichissement) {

    Serial.println("=== Cycle API ecran ===");

    JourneyData mesJourneys;
    DepartureData mesDepartures;
    MergedData affichageFinal;

    fetchJourneysData(mesJourneys);
    printJourneyData(mesJourneys);

    fetchDepartures(mesDepartures);
    printDepartureData(mesDepartures);

    fusionnerDonnees(mesJourneys, mesDepartures, affichageFinal);
    printMergedData(affichageFinal);

    afficherHorairesTrains(affichageFinal);

    planifierProchainRafraichissement(affichageFinal);

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