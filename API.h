
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