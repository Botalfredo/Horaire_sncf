#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <SPI.h>

// --- Définition de vos broches ESP32-C3 ---
#define EPD_BUSY 4   // Broche autorisée
#define EPD_RST  16  // Broche autorisée
#define EPD_DC   17  // Broche autorisée
#define EPD_CS   5   // CS (Chip Select) standard du VSPI
#define EPD_SCL  18  // SCK (Horloge) standard du VSPI
#define EPD_SDA  23  // MOSI (Données) standard du VSPI
#define EPD_MISO -1  // Non utilisé par l'écran
// Initialisation de l'écran (Contrôleur SSD1680)
GxEPD2_3C<GxEPD2_290_C90c, GxEPD2_290_C90c::HEIGHT> display(GxEPD2_290_C90c(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));