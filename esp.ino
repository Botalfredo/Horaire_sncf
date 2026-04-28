void init_IO() {
  pinMode(10, OUTPUT);
  pinMode(6, OUTPUT);
  pinMode(7, OUTPUT);
}

void init_Time() {
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
}