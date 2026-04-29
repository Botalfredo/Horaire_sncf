void init_WiFi() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);

  WiFi.setAutoReconnect(true);

  WiFi.setHostname("INFO-455350");
  WiFi.begin(ssid, password);
  WiFi.setTxPower(WIFI_POWER_15dBm );
  Serial.print("Connexion au WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nConnecté !");
  Serial.print("IP : ");
  Serial.println(WiFi.localIP());
}


void ensureWiFiConnected() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi perdu, tentative de reconnexion forcée...");
    WiFi.begin(ssid, password); 
    
    // On attend maximum 10 secondes pour ne pas bloquer le reste du code (capteurs, affichage)
    int timeout = 0;
    while (WiFi.status() != WL_CONNECTED && timeout < 20) {
      delay(500);
      Serial.print(".");
      timeout++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println(" Reconnecté !");
    } else {
      Serial.println(" Échec. Le serveur sera interrogé au prochain tour.");
    }
  }
}