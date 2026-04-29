String ID_LABEGE = "stop_area:SNCF:87612002";
String ID_TOULOUSE = "stop_area:SNCF:87611004";

class BufferedStream : public Stream {
  Stream& _stream;
  uint8_t _buffer[512];
  size_t _head, _tail;
public:
  BufferedStream(Stream& s) : _stream(s), _head(0), _tail(0) {}
  int available() override {
    if (_head < _tail) return _tail - _head;
    return _stream.available();
  }
  int read() override {
    if (_head >= _tail) {
      _head = 0;
      _tail = _stream.readBytes(_buffer, sizeof(_buffer));
      if (_tail == 0) return -1;
    }
    return _buffer[_head++];
  }
  int peek() override {
    if (_head >= _tail) {
      _head = 0;
      _tail = _stream.readBytes(_buffer, sizeof(_buffer));
      if (_tail == 0) return -1;
    }
    return _buffer[_head];
  }
  void flush() override { _stream.flush(); }
  size_t write(uint8_t c) override { return _stream.write(c); }
};

void fetchJourneysData(JourneyData &data) {
  Serial.println("\nfetchJourneysData()");
  data.count = 0;

  if (WiFi.status() == WL_CONNECTED) {
    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(20000);

    HTTPClient http;
    http.setTimeout(20000);

    String url = "https://api.navitia.io/v1/coverage/sncf/journeys?from=stop_area:SNCF:87612002&to=stop_area:SNCF:87611004&min_nb_journeys=4";

    http.begin(client, url);
    http.setAuthorization(api_key, "");
    http.useHTTP10(true);

    int httpCode = http.GET();

    Serial.print("Journeys httpcode : ");
    Serial.println(httpCode);

    if (httpCode == 200) {
      // On enveloppe le flux Wi-Fi dans notre lecteur rapide (Buffer)
      BufferedStream buffStream(*http.getStreamPtr());

      // On filtre uniquement ce dont on a besoin pour soulager la RAM
      DynamicJsonDocument filter(512);
      filter["journeys"][0]["departure_date_time"] = true;

      // On alloue la mémoire pour le JSON filtré
      DynamicJsonDocument doc(4096);

      // On désérialise depuis notre flux rapide
      DeserializationError error = deserializeJson(doc, buffStream, DeserializationOption::Filter(filter));

      if (error) {
        Serial.print("Erreur JSON (Journeys): ");
        Serial.println(error.c_str());
      } else {
        JsonArray journeys = doc["journeys"];
        
        for (JsonObject journey : journeys) {
          if (data.count >= MAX_TRAINS) break;

          // Récupération de la chaîne brute "YYYYMMDDTHHMMSS"
          String rawPrevu = journey["departure_date_time"].as<String>();
          
          if (rawPrevu != "null" && rawPrevu.length() >= 15) {
            // Extraction de HH:MM pour correspondre à la logique de ton afficheur
            data.trains[data.count].prevu = rawPrevu.substring(9, 11) + ":" + rawPrevu.substring(11, 13);
            
            // Initialisation des données secondaires
            data.trains[data.count].reel = data.trains[data.count].prevu; 
            data.trains[data.count].alerte = "";
            
            data.count++;
          }
        }
        
        Serial.print(" Receive JSON parsing reussi : ");
        Serial.print(data.count);
        Serial.println(" train(s) extrait(s)");
      }

    } else {
      Serial.print("Erreur HTTP Journeys: ");
      Serial.println(httpCode);
    }
    
    http.end();
  } else {
    Serial.println("Erreur : WiFi non connecté.");
  }
}

void fetchDepartures(DepartureData &data) {
  Serial.println("\nfetchDepartures");
  data.count = 0;  // Réinitialise le compteur

  if (WiFi.status() == WL_CONNECTED) {
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.setTimeout(15000);

    String url = "https://api.sncf.com/v1/coverage/sncf/stop_areas/" + String(ID_LABEGE) + "/departures?count=8";

    http.begin(client, url);
    http.setAuthorization(api_key, "");

    int httpCode = http.GET();

    Serial.print("coverage httpcode");
    Serial.println(httpCode);

    if (httpCode == 200) {
      String payload = http.getString();

      DynamicJsonDocument filter(512);
      filter["departures"][0]["display_informations"]["direction"] = true;
      filter["departures"][0]["display_informations"]["headsign"] = true;
      filter["departures"][0]["stop_date_time"]["base_departure_date_time"] = true;
      filter["departures"][0]["stop_date_time"]["departure_date_time"] = true;
      filter["departures"][0]["display_informations"]["links"][0]["type"] = true;
      filter["departures"][0]["display_informations"]["links"][0]["id"] = true;
      filter["disruptions"][0]["id"] = true;
      filter["disruptions"][0]["messages"][0]["text"] = true;

      DynamicJsonDocument doc(16384);
      DeserializationError error = deserializeJson(doc, payload, DeserializationOption::Filter(filter));

      if (error) {
        Serial.println("Erreur JSON (Departures)");
        http.end();
        return;
      }

      JsonArray departures = doc["departures"];

      for (JsonObject dep : departures) {
        if (data.count >= 6) break;  // Limite fixée à 6 comme dans ton code

        String direction = dep["display_informations"]["direction"].as<String>();

        if (direction.indexOf("Toulouse") != -1) {
          String numTrain = dep["display_informations"]["headsign"].as<String>();
          data.trains[data.count].numTrain = (numTrain == "null") ? "N/A" : numTrain;

          String rawPrevu = dep["stop_date_time"]["base_departure_date_time"].as<String>();
          String rawReel = dep["stop_date_time"]["departure_date_time"].as<String>();

          data.trains[data.count].prevu = rawPrevu.substring(9, 11) + ":" + rawPrevu.substring(11, 13);
          data.trains[data.count].reel = rawReel.substring(9, 11) + ":" + rawReel.substring(11, 13);

          int retardMin = calculerRetard(rawPrevu, rawReel);
          data.trains[data.count].retardStr = (retardMin > 0) ? "⚠️ +" + String(retardMin) + "m" : "✅ A l'heure";

          String alerteMsg = "-";
          JsonArray links = dep["display_informations"]["links"];

          for (JsonObject link : links) {
            if (link["type"] == "disruption") {
              String dId = link["id"].as<String>();
              for (JsonObject disruption : doc["disruptions"].as<JsonArray>()) {
                if (disruption["id"].as<String>() == dId) {
                  alerteMsg = disruption["messages"][0]["text"].as<String>();
                  break;
                }
              }
            }
          }

          if (alerteMsg.length() > 40) {
            alerteMsg = alerteMsg.substring(0, 37) + "...";
          }
          data.trains[data.count].alerteMsg = alerteMsg;

          data.count++;
        }
      }
    } else {
      Serial.println("Erreur HTTP Departures: " + String(httpCode));
    }
    http.end();
  }
}

void printJourneyData(const JourneyData &data) {
  Serial.println("------------------------------------------------------------");
  Serial.printf(" printJourneyData\n", data.count);
  Serial.println("------------------------------------------------------------");

  for (int i = 0; i < data.count; i++) {
    // Calcul simple du statut pour l'affichage (Prevu vs Reel)
    String statut = (data.trains[i].prevu == data.trains[i].reel) ? "✅ A l'heure" : ("⚠️ " + data.trains[i].reel);
    String alerteTxt = (data.trains[i].alerte != "") ? (" Alerte : " + data.trains[i].alerte) : "";

    Serial.printf("[%d] Depart a %s : %s%s\n",
                  i + 1,
                  data.trains[i].prevu.c_str(),
                  statut.c_str(),
                  alerteTxt.c_str());
  }
  Serial.println("------------------------------------------------------------");
}

void printDepartureData(const DepartureData &data) {
  Serial.println("=========================================================================================");
  Serial.println("printDepartureData");
  Serial.println("=========================================================================================");
  Serial.printf("%-10s | %-5s | %-5s | %-12s | %s\n", "N Train", "Prevu", "Reel", "Retard", "Avertissements SNCF");
  Serial.println("-----------------------------------------------------------------------------------------");

  for (int i = 0; i < data.count; i++) {
    Serial.printf("%-10s | %-5s | %-5s | %-12s | %s\n",
                  data.trains[i].numTrain.c_str(),
                  data.trains[i].prevu.c_str(),
                  data.trains[i].reel.c_str(),
                  data.trains[i].retardStr.c_str(),
                  data.trains[i].alerteMsg.c_str());
  }

  if (data.count == 0) {
    Serial.println("Aucun train prevu vers Toulouse dans l'immediat.");
  }

  Serial.println("=========================================================================================\n");
}

void fusionnerDonnees(const JourneyData &journeys, const DepartureData &departures, MergedData &merged) {
  merged.count = 0;

  // SCÉNARIO 1 : L'API Departures a fonctionné (C'est la base idéale)
  if (departures.count > 0) {
    for (int i = 0; i < departures.count; i++) {
      if (merged.count >= 5) break;

      MergedTrain t;
      t.numTrain = departures.trains[i].numTrain;
      t.prevu = departures.trains[i].prevu;
      t.reel = departures.trains[i].reel;
      t.retardStr = departures.trains[i].retardStr;
      t.alerteMsg = departures.trains[i].alerteMsg;

      // Enrichissement avec les alertes de l'API Journeys
      if (journeys.count > 0) {
        for (int j = 0; j < journeys.count; j++) {
          if (journeys.trains[j].prevu == t.prevu) {
            if (journeys.trains[j].alerte != "" && journeys.trains[j].alerte != "-") {
              if (t.alerteMsg == "-" || t.alerteMsg == "") {
                t.alerteMsg = journeys.trains[j].alerte;
              } else if (t.alerteMsg.indexOf(journeys.trains[j].alerte) == -1) {
                t.alerteMsg = t.alerteMsg + " | " + journeys.trains[j].alerte;
              }
            }
            break;
          }
        }
      }

      if (t.alerteMsg.length() > 40) {
        t.alerteMsg = t.alerteMsg.substring(0, 37) + "...";
      }

      merged.trains[merged.count] = t;
      merged.count++;
    }
  }
  // SCÉNARIO 2 : Mode DÉGRADÉ (L'API Departures a planté, mais Journeys a fonctionné)
  else if (journeys.count > 0) {
    Serial.println("⚠️ ATTENTION: API Departures indisponible. Bascule sur l'API Journeys uniquement.");

    for (int i = 0; i < journeys.count; i++) {
      if (merged.count >= 5) break;

      MergedTrain t;
      t.numTrain = "N/A";  // Numéro de train indisponible sans alourdir le JSON de journeys
      t.prevu = journeys.trains[i].prevu;
      t.reel = journeys.trains[i].reel;

      // Approximation du retard pour le mode dégradé (basé sur la différence textuelle)
      if (t.prevu == t.reel) {
        t.retardStr = "✅ A l'heure";
      } else {
        t.retardStr = "⚠️ " + t.reel;
      }

      t.alerteMsg = (journeys.trains[i].alerte != "") ? journeys.trains[i].alerte : "-";

      if (t.alerteMsg.length() > 40) {
        t.alerteMsg = t.alerteMsg.substring(0, 37) + "...";
      }

      merged.trains[merged.count] = t;
      merged.count++;
    }
  }

  // 3. Tri à bulles par heure de départ (format "HH:MM")
  for (int i = 0; i < merged.count - 1; i++) {
    for (int j = 0; j < merged.count - i - 1; j++) {
      if (merged.trains[j].prevu > merged.trains[j + 1].prevu) {
        MergedTrain temp = merged.trains[j];
        merged.trains[j] = merged.trains[j + 1];
        merged.trains[j + 1] = temp;
      }
    }
  }
}

void printMergedData(const MergedData &data) {
  Serial.println("\n=========================================================================================");
  Serial.println("🚆 SYNTHESE FINALE DEPARTS : LABEGE -> TOULOUSE");
  Serial.println("=========================================================================================");
  Serial.printf("%-10s | %-5s | %-5s | %-12s | %s\n", "N Train", "Prevu", "Reel", "Retard", "Avertissements");
  Serial.println("-----------------------------------------------------------------------------------------");

  for (int i = 0; i < data.count; i++) {
    Serial.printf("%-10s | %-5s | %-5s | %-12s | %s\n",
                  data.trains[i].numTrain.c_str(),
                  data.trains[i].prevu.c_str(),
                  data.trains[i].reel.c_str(),
                  data.trains[i].retardStr.c_str(),
                  data.trains[i].alerteMsg.c_str());
  }

  if (data.count == 0) {
    Serial.println("Aucun train prevu vers Toulouse dans l'immediat.");
  }

  Serial.println("=========================================================================================\n");
}

// Fonction utilitaire pour calculer le retard en minutes
int calculerRetard(String baseTime, String realTime) {
  if (baseTime.length() < 15 || realTime.length() < 15) return 0;

  struct tm tmBase = { 0 };
  struct tm tmReal = { 0 };

  // Format YYYYMMDDTHHMMSS
  tmBase.tm_year = baseTime.substring(0, 4).toInt() - 1900;
  tmBase.tm_mon = baseTime.substring(4, 6).toInt() - 1;
  tmBase.tm_mday = baseTime.substring(6, 8).toInt();
  tmBase.tm_hour = baseTime.substring(9, 11).toInt();
  tmBase.tm_min = baseTime.substring(11, 13).toInt();
  tmBase.tm_sec = baseTime.substring(13, 15).toInt();

  tmReal.tm_year = realTime.substring(0, 4).toInt() - 1900;
  tmReal.tm_mon = realTime.substring(4, 6).toInt() - 1;
  tmReal.tm_mday = realTime.substring(6, 8).toInt();
  tmReal.tm_hour = realTime.substring(9, 11).toInt();
  tmReal.tm_min = realTime.substring(11, 13).toInt();
  tmReal.tm_sec = realTime.substring(13, 15).toInt();

  time_t tBase = mktime(&tmBase);
  time_t tReal = mktime(&tmReal);

  return (int)difftime(tReal, tBase) / 60;
}