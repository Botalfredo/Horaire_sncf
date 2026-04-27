
// --- Définition de vos broches ESP32-C3 ---
#define EPD_BUSY 21
#define EPD_RST 6
#define EPD_DC 7
#define EPD_CS 10
#define EPD_SCL 20
#define EPD_SDA 5
#define EPD_MISO -1

// Initialisation de l'écran (Contrôleur SSD1680)
GxEPD2_3C<GxEPD2_290_C90c, GxEPD2_290_C90c::HEIGHT> display(GxEPD2_290_C90c(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));