// Variables globales pour mémoriser l'état précédent
MergedTrain anciensTrains[5];
bool premierAffichage = true;


void init_screen() {
  SPI.begin(EPD_SCL, EPD_MISO, EPD_SDA, EPD_CS);
  display.init(115200, true, 2, false);
  display.setRotation(1);  // Paysage
  display.setFont(&FreeSans9pt7b);
  display.setFullWindow();
}

void afficherEcranDemarrage() {
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
    
    // Titre principal
    display.setCursor(40, 50);
    display.print("SNCF - LABEGE");
    
    // Sous-titre ou destination
    display.setCursor(45, 80);
    display.print("Vers Toulouse");
    
    // Message de chargement
    display.setCursor(10, 120);
    display.print("Connexion et mise a jour...");
    
  } while (display.nextPage());
}

// =========================================================================
// FONCTION UTILITAIRE : Dessine UNE SEULE ligne de train
// =========================================================================
void dessinerLigneTrain(const MergedTrain &train, int index, int startY, int espacementY) {
  int x = 5;
  int y = startY + (index * espacementY);

  // Variables booléennes pour simplifier la lecture
  bool enRetard = (train.prevu != train.reel);
  bool aUneAlerte = (train.alerteMsg != "" && train.alerteMsg != "-");

  // -- 1. LOGIQUE D'AFFICHAGE DE L'HEURE --
  if (!enRetard) {
    // Train à l'heure
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(x, y);
    display.print(train.prevu);
  } else {
    // Train en retard : On écrit l'heure prévue en ROUGE et on la barre
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(x, y);
    display.print(train.prevu);

    // Calcul de la zone du texte pour dessiner la ligne par-dessus
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(train.prevu, x, y, &x1, &y1, &w, &h);

    int hauteurLigne = y - (h / 2) + 2;
    display.drawLine(x, hauteurLigne, x + w, hauteurLigne, GxEPD_RED);
    display.drawLine(x, hauteurLigne - 1, x + w, hauteurLigne - 1, GxEPD_RED);

    // On décale le curseur pour écrire l'heure réelle
    x += w + 8;
    display.setTextColor(GxEPD_RED);
    display.setCursor(x, y);
    display.print(train.reel);
  }

  // -- 2. AFFICHAGE DU STATUT / ALERTE --
  x = 110;
  display.setCursor(x, y);

  if (!enRetard && !aUneAlerte) {
    display.setTextColor(GxEPD_BLACK);
    display.print("A l'heure");
  } else {
    display.setTextColor(GxEPD_RED);
    if (aUneAlerte) {
      display.print(enleverAccents(train.alerteMsg.substring(0, 22)));
    } else {
      display.print(enleverAccents(train.retardStr));
    }
  }
}

// =========================================================================
// FONCTION PRINCIPALE : Affichage intelligent des horaires
// =========================================================================
void afficherHorairesTrains(const MergedData &affichageFinal) {
  int startY = 25;
  int espacementY = 30;

  if (premierAffichage) {
    // ---------------------------------------------------------
    // 1er PASSAGE : Rafraîchissement complet de l'écran
    // ---------------------------------------------------------
    display.setFullWindow();
    display.firstPage();
    do {
      display.fillScreen(GxEPD_WHITE);
      for (int i = 0; i < affichageFinal.count; i++) {
        dessinerLigneTrain(affichageFinal.trains[i], i, startY, espacementY);
        anciensTrains[i] = affichageFinal.trains[i];
      }
      // On s'assure de vider les lignes restantes en mémoire
      for (int i = affichageFinal.count; i < 5; i++) {
        anciensTrains[i] = MergedTrain();
      }
    } while (display.nextPage());

    premierAffichage = false;

  } else {
    // ---------------------------------------------------------
    // PASSAGES SUIVANTS : Mise à jour partielle optimisée
    // ---------------------------------------------------------
    int firstChanged = -1;
    int lastChanged = -1;

    // 1. On cherche quelle est la zone affectée par les changements
    for (int i = 0; i < 5; i++) {  // On vérifie toujours les 5 emplacements possibles
      bool existsNow = (i < affichageFinal.count);
      bool existedBefore = (anciensTrains[i].prevu != "");

      bool changed = false;

      if (existsNow) {
        if (affichageFinal.trains[i].prevu != anciensTrains[i].prevu || affichageFinal.trains[i].reel != anciensTrains[i].reel || affichageFinal.trains[i].alerteMsg != anciensTrains[i].alerteMsg) {
          changed = true;
        }
      } else if (existedBefore) {
        // Il n'y a plus de train à cet index (ex: on passe de 5 à 4 trains)
        changed = true;
      }

      if (changed) {
        if (firstChanged == -1) firstChanged = i;
        lastChanged = i;
      }
    }

    // 2. S'il y a eu un changement, on met à jour en UNE SEULE FOIS
    if (firstChanged != -1) {

      int windowY = (startY + (firstChanged * espacementY)) - 20;
      int nbLignes = (lastChanged - firstChanged) + 1;
      int windowHeight = nbLignes * espacementY;

      // On limite le rafraichissement au grand rectangle qui contient les modifs
      display.setPartialWindow(0, windowY, display.width(), windowHeight);

      display.firstPage();
      do {
        // On efface la zone en blanc
        display.fillRect(0, windowY, display.width(), windowHeight, GxEPD_WHITE);

        // On redessine uniquement les trains présents dans cette zone
        for (int i = firstChanged; i <= lastChanged; i++) {
          if (i < affichageFinal.count) {
            dessinerLigneTrain(affichageFinal.trains[i], i, startY, espacementY);
          }
        }
      } while (display.nextPage());

      // 3. On met à jour la mémoire globale
      for (int i = firstChanged; i <= lastChanged; i++) {
        if (i < affichageFinal.count) {
          anciensTrains[i] = affichageFinal.trains[i];
        } else {
          anciensTrains[i] = MergedTrain();  // Structure vide pour effacer la mémoire
        }
      }
    }
  }
}

String enleverAccents(String texte) {
  // 1. Remplacement des accents français courants (UTF-8)
  texte.replace("é", "e");
  texte.replace("è", "e");
  texte.replace("ê", "e");
  texte.replace("ë", "e");
  texte.replace("à", "a");
  texte.replace("â", "a");
  texte.replace("ç", "c");
  texte.replace("î", "i");
  texte.replace("ï", "i");
  texte.replace("ô", "o");
  texte.replace("ù", "u");
  texte.replace("û", "u");

  // Majuscules
  texte.replace("É", "E");
  texte.replace("È", "E");
  texte.replace("Ê", "E");
  texte.replace("À", "A");
  texte.replace("Ç", "C");

  // 2. Suppression des caractères non-ASCII restants (supprimera les Emojis !)
  String textePropre = "";
  for (int i = 0; i < texte.length(); i++) {
    char c = texte[i];
    // On garde uniquement les caractères de la table ASCII standard
    if (c >= 32 && c <= 126) {
      textePropre += c;
    }
  }

  return textePropre;
}