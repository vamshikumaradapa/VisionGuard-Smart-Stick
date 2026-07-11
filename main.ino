#if defined(__has_include)
  #if __has_include(<SoftwareSerial.h>)
    #include <SoftwareSerial.h>
  #else
    #include <Arduino.h>
    // Minimal compatibility wrapper that forwards to Serial when SoftwareSerial.h is not available.
    // This allows IntelliSense/compilation to continue on platforms without SoftwareSerial.
    class SoftwareSerial : public Stream {
    public:
      SoftwareSerial(int rx, int tx) { (void)rx; (void)tx; }
      void begin(unsigned long baud) { Serial.begin(baud); }
      int available() { return Serial.available(); }
      int read() { return Serial.read(); }
      int peek() { return Serial.peek(); }
      void flush() { Serial.flush(); }
      size_t write(uint8_t b) { return Serial.write(b); }
      using Print::write;
    };
  #endif
#else
  #include <SoftwareSerial.h>
#endif

#include <TinyGPS++.h>
#include <DFRobotDFPlayerMini.h>

// GPS & GSM
SoftwareSerial gpsSerial(2, 3);
SoftwareSerial gsmSerial(10, 11);
TinyGPSPlus gps;

// DFPlayer (voice)
SoftwareSerial dfSerial(7, 8);
DFRobotDFPlayerMini player;

// Ultrasonic Sensors
const int trig1 = A1, echo1 = A0; // front
const int trig2 = A2, echo2 = A3; // side
const int trig3 = 4, echo3 = 9;   // bottom (hole detection)

// Inputs
const int buzzer = 5;
const int buttonPin = 6;
const int touchPin = 12; // rubber stick switch

float lat, lon;

// -------- SETUP --------
void setup() {
  Serial.begin(9600);
  gpsSerial.begin(9600);
  gsmSerial.begin(9600);
  dfSerial.begin(9600);

  pinMode(trig1, OUTPUT); pinMode(echo1, INPUT);
  pinMode(trig2, OUTPUT); pinMode(echo2, INPUT);
  pinMode(trig3, OUTPUT); pinMode(echo3, INPUT);

  pinMode(buzzer, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(touchPin, INPUT_PULLUP);

  if (!player.begin(dfSerial)) {
    Serial.println("DFPlayer Error");
  } else {
    player.volume(25);
  }

  Serial.println("Smart Stick Ready...");
}

// -------- DISTANCE --------
int getDistance(int trig, int echo) {
  digitalWrite(trig, LOW); delayMicroseconds(2);
  digitalWrite(trig, HIGH); delayMicroseconds(10);
  digitalWrite(trig, LOW);
  long duration = pulseIn(echo, HIGH);
  return duration * 0.034 / 2;
}

// -------- GPS --------
bool getGPS() {
  unsigned long start = millis();
  while (millis() - start < 5000) {
    while (gpsSerial.available()) {
      gps.encode(gpsSerial.read());
    }
    if (gps.location.isUpdated()) {
      lat = gps.location.lat();
      lon = gps.location.lng();
      return true;
    }
  }
  return false;
}

// -------- VOICE --------
void speak(int track) {
  player.play(track); // MP3 files
}

// -------- SOS --------
void sendSOS() {
  gsmSerial.println("AT+CMGF=1");
  delay(1000);

  gsmSerial.println("AT+CMGS=\"+91XXXXXXXXXX\"");
  delay(1000);

  gsmSerial.println("Emergency! Need help.");

  if (getGPS()) {
    gsmSerial.print("Location: ");
    gsmSerial.print(lat, 6);
    gsmSerial.print(",");
    gsmSerial.println(lon, 6);
  }

  gsmSerial.write(26);
  delay(2000);

  speak(3); // "Emergency activated"
}

// -------- LOOP --------
void loop() {

  int front = getDistance(trig1, echo1);
  int side  = getDistance(trig2, echo2);
  int down  = getDistance(trig3, echo3); // hole

  // OBSTACLE
  if (front <= 20 || side <= 20) {
    digitalWrite(buzzer, HIGH);
    speak(1); // "Obstacle ahead"
    delay(800);
    digitalWrite(buzzer, LOW);
  }

  // HOLE DETECTION
  if (down > 40) {  // large distance = hole
    digitalWrite(buzzer, HIGH);
    speak(2); // "Hole detected"
    delay(1000);
    digitalWrite(buzzer, LOW);
  }

  // TOUCH ALERT (for deaf-blind)
  if (digitalRead(touchPin) == LOW) {
    digitalWrite(buzzer, HIGH);
    delay(1000);
    digitalWrite(buzzer, LOW);
  }

  // SOS BUTTON
  if (digitalRead(buttonPin) == LOW) {
    sendSOS();
    delay(3000);
  }

  delay(200);
}