
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
  Serial.printf("Prochaine actualisation prevue a %s\n", buffer);
}