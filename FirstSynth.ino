/*
  WalkerRemoteBoard / ESP8266
  TECHNO / DISCO SYNTH
  + STRONG CLUB KICK
  + POWER-UP ARCADE BUTTON ON D0 / GPIO16
  + BASS
  + LEAD
  + HI-HAT
  + SNARE
  + ELECTRIC PIANO
  + CHROMATIC SCALE
  + SIREN / ROBOT / LASER FX

  Effetto bottone:
  - Premi D0/GPIO16: effetto power-up arcade tipo fungo sopra il ritmo
  - Lasci D0/GPIO16: effetto si spegne
  - Il ritmo continua sempre

  Output audio: D5 / GPIO14

  Collegamento audio consigliato:
  D5 GPIO14 -> 1k -> condensatore 47uF -> TIP jack amplificatore
  GND ESP8266 -> SLEEVE jack amplificatore

  Condensatore elettrolitico:
  + lato ESP8266
  - lato amplificatore

  Bottone:
  D0 / GPIO16 -> bottone -> 3.3V

  Con INPUT_PULLDOWN_16:
  rilasciato = LOW
  premuto   = HIGH
*/

#define AUDIO_PIN D5
#define FX_BUTTON_PIN 16

// -----------------------------
// AUDIO SETTINGS
// -----------------------------
const uint16_t SAMPLE_RATE = 12000;
const uint32_t SAMPLE_PERIOD_US = 1000000UL / SAMPLE_RATE;

unsigned long lastSampleMicros = 0;

const int PWM_CENTER = 512;

// -----------------------------
// BUTTON SETTINGS
// -----------------------------
bool buttonWasPressed = false;

// -----------------------------
// NOTE FREQUENCIES
// -----------------------------
#define NOTE_C3   131
#define NOTE_CS3  139
#define NOTE_D3   147
#define NOTE_DS3  156
#define NOTE_E3   165
#define NOTE_F3   175
#define NOTE_FS3  185
#define NOTE_G3   196
#define NOTE_GS3  208
#define NOTE_A3   220
#define NOTE_AS3  233
#define NOTE_B3   247

#define NOTE_C4   262
#define NOTE_CS4  277
#define NOTE_D4   294
#define NOTE_DS4  311
#define NOTE_E4   330
#define NOTE_F4   349
#define NOTE_FS4  370
#define NOTE_G4   392
#define NOTE_GS4  415
#define NOTE_A4   440
#define NOTE_AS4  466
#define NOTE_B4   494

#define NOTE_C5   523
#define NOTE_CS5  554
#define NOTE_D5N  587
#define NOTE_DS5  622
#define NOTE_E5   659
#define NOTE_F5   698
#define NOTE_FS5  740
#define NOTE_G5   784
#define NOTE_GS5  831
#define NOTE_A5   880
#define NOTE_AS5  932
#define NOTE_B5   988

#define NOTE_C6   1047
#define NOTE_CS6  1109
#define NOTE_D6   1175
#define NOTE_DS6  1245
#define NOTE_E6   1319
#define NOTE_F6   1397
#define NOTE_FS6  1480
#define NOTE_G6   1568
#define NOTE_GS6  1661
#define NOTE_A6   1760
#define NOTE_AS6  1865
#define NOTE_B6   1976

#define NOTE_C7   2093
#define NOTE_D7   2349
#define NOTE_E7   2637
#define NOTE_G7   3136

// Scala cromatica alta ottava 6
const uint16_t chromaticScaleHigh[] = {
  NOTE_C6, NOTE_CS6, NOTE_D6, NOTE_DS6,
  NOTE_E6, NOTE_F6, NOTE_FS6, NOTE_G6,
  NOTE_GS6, NOTE_A6, NOTE_AS6, NOTE_B6
};

const int CHROMATIC_LEN = 12;

// Effetto power-up arcade originale
const uint16_t powerUpNotes[] = {
  NOTE_C5, NOTE_E5, NOTE_G5, NOTE_C6,
  NOTE_E6, NOTE_G6, NOTE_C7, NOTE_E7,
  NOTE_G7, NOTE_E7, NOTE_C7, NOTE_G6
};

const int POWERUP_LEN = 12;

// -----------------------------
// SEQUENCER
// -----------------------------
const int BPM = 150;
const int STEP_MS = 60000 / BPM / 4;

unsigned long lastStepMillis = 0;
int stepIndex = 0;
int barCounter = 0;

// -----------------------------
// OSCILLATORS 32-bit PHASE
// -----------------------------
uint32_t bassPhase = 0;
uint32_t bassInc = 0;

uint32_t leadPhase = 0;
uint32_t leadInc = 0;

uint32_t fxPhase = 0;
uint32_t fxInc = 0;

uint32_t sirenPhase = 0;
uint32_t sirenInc = 0;

uint32_t robotPhase = 0;
uint32_t robotInc = 0;

uint32_t scalePhase = 0;
uint32_t scaleInc = 0;

uint32_t pianoPhase = 0;
uint32_t pianoInc = 0;

uint32_t pianoEchoPhase = 0;
uint32_t pianoEchoInc = 0;

// Power-up arcade FX
bool powerUpActive = false;
uint32_t powerUpPhase = 0;
uint32_t powerUpInc = 0;
int powerUpEnv = 0;
int powerUpIndex = 0;
int powerUpTimer = 0;

// -----------------------------
// ENVELOPES
// -----------------------------
int bassEnv = 0;
int leadEnv = 0;
int fxEnv = 0;

int sirenEnv = 0;
int sirenMode = 0;

int robotEnv = 0;
int robotCounter = 0;

int crashEnv = 0;

int kickEnv = 0;
int kickPitch = 0;

int snareEnv = 0;
int hatEnv = 0;

int scaleEnv = 0;
int scaleMode = 0;
int scaleIndex = 0;
int scaleStepTimer = 0;

int pianoEnv = 0;
int pianoEchoEnv = 0;

// noise generator LFSR
uint32_t noiseState = 0xABCDE123;

// -----------------------------
// UTILS
// -----------------------------
uint32_t freqToInc(uint16_t freq) {
  return ((uint64_t)freq * 4294967296ULL) / SAMPLE_RATE;
}

int fastNoise() {
  uint32_t bit = ((noiseState >> 0) ^ (noiseState >> 2) ^ (noiseState >> 3) ^ (noiseState >> 5)) & 1;
  noiseState = (noiseState >> 1) | (bit << 31);

  return (noiseState & 0xFF) - 128;
}

int squareOsc(uint32_t phase) {
  return (phase & 0x80000000UL) ? 127 : -127;
}

int sawOsc(uint32_t phase) {
  return ((phase >> 24) & 0xFF) - 128;
}

int triangleOsc(uint32_t phase) {
  uint8_t v = (phase >> 24) & 0xFF;

  if (v < 128) {
    return (v * 2) - 128;
  }

  return 127 - ((v - 128) * 2);
}

// -----------------------------
// TRIGGERS
// -----------------------------
void setBass(uint16_t freq) {
  bassInc = freqToInc(freq);
  bassEnv = 255;
}

void setLead(uint16_t freq) {
  leadInc = freqToInc(freq);
  leadEnv = 230;
}

void setFx(uint16_t freq) {
  fxInc = freqToInc(freq);
  fxEnv = 255;
}

void setPiano(uint16_t freq) {
  pianoInc = freqToInc(freq);
  pianoEnv = 255;

  pianoEchoInc = freqToInc(freq);
  pianoEchoEnv = 130;
}

// Grancassa automatica disco
void triggerKick() {
  kickEnv = 380;
  kickPitch = 300;
}

void triggerSnare() {
  snareEnv = 230;
}

void triggerHat() {
  hatEnv = 190;
}

void triggerCrash() {
  crashEnv = 255;
}

void triggerLaser() {
  fxInc = freqToInc(NOTE_C7);
  fxEnv = 255;
}

void triggerRiser() {
  fxInc = freqToInc(180);
  fxEnv = 255;
}

void triggerSiren() {
  sirenEnv = 255;
  sirenInc = freqToInc(300);
  sirenMode = 1;
}

void triggerRobot() {
  robotEnv = 255;
  robotCounter = 0;
  robotInc = freqToInc(120);
}

void triggerScaleUp() {
  scaleMode = 1;
  scaleIndex = 0;
  scaleStepTimer = 0;
  scaleInc = freqToInc(chromaticScaleHigh[scaleIndex]);
  scaleEnv = 255;
}

void triggerScaleDown() {
  scaleMode = 2;
  scaleIndex = CHROMATIC_LEN - 1;
  scaleStepTimer = 0;
  scaleInc = freqToInc(chromaticScaleHigh[scaleIndex]);
  scaleEnv = 255;
}

void triggerScaleRandom() {
  scaleMode = 3;
  scaleIndex = random(0, CHROMATIC_LEN);
  scaleStepTimer = 0;
  scaleInc = freqToInc(chromaticScaleHigh[scaleIndex]);
  scaleEnv = 255;
}

void triggerHighSirenScale() {
  triggerScaleUp();
  triggerSiren();
  triggerCrash();
}

// -----------------------------
// POWER-UP FX
// -----------------------------
void startPowerUpFx() {
  powerUpActive = true;
  powerUpIndex = 0;
  powerUpTimer = 0;
  powerUpEnv = 255;
  powerUpInc = freqToInc(powerUpNotes[powerUpIndex]);

  Serial.println("POWER-UP FX ON");
}

void stopPowerUpFx() {
  powerUpActive = false;
  powerUpEnv = 0;
  powerUpTimer = 0;

  Serial.println("POWER-UP FX OFF");
}

void powerUpEngine(int &mix) {
  if (!powerUpActive) return;

  powerUpTimer++;

  // cambia nota ogni circa 55 ms a 12 kHz
  if (powerUpTimer > 660) {
    powerUpTimer = 0;

    powerUpIndex++;
    if (powerUpIndex >= POWERUP_LEN) {
      powerUpIndex = 0;
    }

    powerUpInc = freqToInc(powerUpNotes[powerUpIndex]);
    powerUpEnv = 255;
  }

  powerUpPhase += powerUpInc;

  // Suono arcade brillante: triangle + square + un pizzico saw
  int p1 = triangleOsc(powerUpPhase);
  int p2 = squareOsc(powerUpPhase + 0x10000000UL) / 2;
  int p3 = sawOsc(powerUpPhase) / 4;

  int sound = p1 + p2 + p3;
  sound = (sound * powerUpEnv) / 255;

  mix += sound / 2;

  // decadimento veloce della singola nota
  powerUpEnv -= 3;
  if (powerUpEnv < 60) {
    powerUpEnv = 60;
  }
}

// -----------------------------
// BUTTON READ
// -----------------------------
void checkFxButton() {
  bool pressed = digitalRead(FX_BUTTON_PIN) == HIGH;

  if (pressed && !buttonWasPressed) {
    buttonWasPressed = true;
    startPowerUpFx();
  }

  if (!pressed && buttonWasPressed) {
    buttonWasPressed = false;
    stopPowerUpFx();
  }
}

// -----------------------------
// AUDIO ENGINE
// -----------------------------
void audioEngine() {
  unsigned long now = micros();

  if (now - lastSampleMicros < SAMPLE_PERIOD_US) {
    return;
  }

  lastSampleMicros += SAMPLE_PERIOD_US;

  int mix = 0;

  // Bass oscillator
  if (bassEnv > 0) {
    bassPhase += bassInc;

    int b = squareOsc(bassPhase);
    b = (b * bassEnv) / 255;

    mix += b / 2;

    bassEnv -= 2;
    if (bassEnv < 0) bassEnv = 0;
  }

  // Lead oscillator
  if (leadEnv > 0) {
    leadPhase += leadInc;

    int l1 = triangleOsc(leadPhase);
    int l2 = squareOsc(leadPhase + 0x20000000UL);

    int lead = ((l1 * 2) + l2) / 3;
    lead = (lead * leadEnv) / 255;

    mix += lead / 2;

    leadEnv -= 3;
    if (leadEnv < 0) leadEnv = 0;
  }

  // Electric piano
  if (pianoEnv > 0) {
    pianoPhase += pianoInc;

    int p1 = triangleOsc(pianoPhase);
    int p2 = triangleOsc(pianoPhase * 2);
    int p3 = squareOsc(pianoPhase) / 4;

    int piano = (p1 * 2 + p2 / 2 + p3) / 3;
    piano = (piano * pianoEnv) / 255;

    mix += piano / 2;

    pianoEnv -= 2;
    if (pianoEnv < 0) pianoEnv = 0;
  }

  // Eco piano
  if (pianoEchoEnv > 0) {
    pianoEchoPhase += pianoEchoInc;

    int ep = triangleOsc(pianoEchoPhase);
    ep = (ep * pianoEchoEnv) / 255;

    mix += ep / 4;

    pianoEchoEnv -= 1;
    if (pianoEchoEnv < 0) pianoEchoEnv = 0;
  }

  // FX oscillator
  if (fxEnv > 0) {
    fxPhase += fxInc;

    if (fxInc > freqToInc(900)) {
      fxInc -= 9000;
    } else {
      fxInc += 3500;
    }

    int fx = sawOsc(fxPhase);
    fx = (fx * fxEnv) / 255;

    mix += fx / 2;

    fxEnv -= 4;
    if (fxEnv < 0) fxEnv = 0;
  }

  // Siren oscillator
  if (sirenEnv > 0) {
    sirenPhase += sirenInc;

    if (sirenMode == 1) {
      sirenInc += 1800;

      if (sirenInc > freqToInc(1700)) {
        sirenMode = 2;
      }
    } else {
      if (sirenInc > 2200) {
        sirenInc -= 2200;
      } else {
        sirenInc = freqToInc(300);
      }
    }

    int s = triangleOsc(sirenPhase);
    s = (s * sirenEnv) / 255;

    mix += s / 2;

    sirenEnv -= 2;
    if (sirenEnv < 0) sirenEnv = 0;
  }

  // Robot oscillator
  if (robotEnv > 0) {
    robotCounter++;

    if (robotCounter % 180 == 0) {
      robotInc = freqToInc(random(80, 650));
    }

    robotPhase += robotInc;

    int r = squareOsc(robotPhase);

    int n = fastNoise();

    if (n > 40) {
      r = -r;
    }

    r = (r * robotEnv) / 255;

    mix += r / 2;

    robotEnv -= 2;
    if (robotEnv < 0) robotEnv = 0;
  }

  // Scala cromatica alta
  if (scaleEnv > 0) {
    scalePhase += scaleInc;

    int sc1 = triangleOsc(scalePhase);
    int sc2 = squareOsc(scalePhase + 0x10000000UL);

    int sc = ((sc1 * 2) + sc2) / 3;
    sc = (sc * scaleEnv) / 255;

    mix += sc / 2;

    scaleEnv -= 3;
    if (scaleEnv < 0) scaleEnv = 0;
  }

  // Kick drum / grancassa disco più profonda
  if (kickEnv > 0) {
    uint32_t kInc = freqToInc(kickPitch);
    static uint32_t kickPhase = 0;

    kickPhase += kInc;

    int k1 = triangleOsc(kickPhase);
    int k2 = squareOsc(kickPhase) / 2;

    int punch = 0;
    if (kickEnv > 300) {
      punch = fastNoise() / 2;
    }

    int k = k1 + k2 + punch;

    k = (k * kickEnv) / 255;

    mix += k;

    kickEnv -= 5;

    if (kickEnv < 0) {
      kickEnv = 0;
    }

    if (kickPitch > 90) {
      kickPitch -= 10;
    } else if (kickPitch > 42) {
      kickPitch -= 2;
    }

    if (kickPitch < 42) {
      kickPitch = 42;
    }
  }

  // Snare noise
  if (snareEnv > 0) {
    int n = fastNoise();
    n = (n * snareEnv) / 255;

    mix += n;

    snareEnv -= 5;

    if (snareEnv < 0) {
      snareEnv = 0;
    }
  }

  // Hi-hat
  if (hatEnv > 0) {
    int h = fastNoise();

    if (h > 0) {
      h = 120;
    } else {
      h = -120;
    }

    h = (h * hatEnv) / 255;

    mix += h / 2;

    hatEnv -= 9;

    if (hatEnv < 0) {
      hatEnv = 0;
    }
  }

  // Crash
  if (crashEnv > 0) {
    int c = fastNoise();
    c = (c * crashEnv) / 255;

    mix += c;

    crashEnv -= 2;

    if (crashEnv < 0) {
      crashEnv = 0;
    }
  }

  // Gestione scala cromatica
  if (scaleMode > 0) {
    scaleStepTimer++;

    if (scaleStepTimer > 540) {
      scaleStepTimer = 0;

      if (scaleMode == 1) {
        scaleIndex++;

        if (scaleIndex >= CHROMATIC_LEN) {
          scaleMode = 0;
          scaleEnv = 0;
        } else {
          scaleInc = freqToInc(chromaticScaleHigh[scaleIndex]);
          scaleEnv = 255;
        }
      }

      else if (scaleMode == 2) {
        scaleIndex--;

        if (scaleIndex < 0) {
          scaleMode = 0;
          scaleEnv = 0;
        } else {
          scaleInc = freqToInc(chromaticScaleHigh[scaleIndex]);
          scaleEnv = 255;
        }
      }

      else if (scaleMode == 3) {
        static int randomCount = 0;

        scaleIndex = random(0, CHROMATIC_LEN);
        scaleInc = freqToInc(chromaticScaleHigh[scaleIndex]);
        scaleEnv = 230;

        randomCount++;

        if (randomCount > 10) {
          randomCount = 0;
          scaleMode = 0;
          scaleEnv = 0;
        }
      }
    }
  }

  // Power-up sopra al ritmo
  if (powerUpActive) {
    powerUpEngine(mix);
  }

  // Limiter
  mix = constrain(mix, -127, 127);

  int pwmValue = map(mix, -127, 127, 0, 1023);
  analogWrite(AUDIO_PIN, pwmValue);
}

// -----------------------------
// SEQUENCER
// -----------------------------
void sequencer() {
  unsigned long now = millis();

  if (now - lastStepMillis < STEP_MS) {
    return;
  }

  lastStepMillis += STEP_MS;

  // Grancassa automatica 4/4
  if (stepIndex == 0 || stepIndex == 4 || stepIndex == 8 || stepIndex == 12) {
    triggerKick();
  }

  // Snare / clap
  if (stepIndex == 4 || stepIndex == 12) {
    triggerSnare();
  }

  // Hi-hat
  if (stepIndex == 2 || stepIndex == 6 || stepIndex == 10 || stepIndex == 14) {
    triggerHat();
  }

  if ((barCounter % 2 == 1) && (stepIndex == 3 || stepIndex == 11)) {
    triggerHat();
  }

  // Bass pattern
  if (stepIndex == 0)  setBass(NOTE_C3);
  if (stepIndex == 3)  setBass(NOTE_C3);
  if (stepIndex == 7)  setBass(NOTE_G3);
  if (stepIndex == 8)  setBass(NOTE_AS3);
  if (stepIndex == 11) setBass(NOTE_G3);
  if (stepIndex == 14) setBass(NOTE_DS3);

  // Lead melody
  if (stepIndex == 1)  setLead(NOTE_C5);
  if (stepIndex == 2)  setLead(NOTE_E5);
  if (stepIndex == 3)  setLead(NOTE_G5);
  if (stepIndex == 5)  setLead(NOTE_E5);
  if (stepIndex == 6)  setLead(NOTE_D5N);
  if (stepIndex == 7)  setLead(NOTE_C5);

  if (stepIndex == 9)  setLead(NOTE_G5);
  if (stepIndex == 10) setLead(NOTE_A5);
  if (stepIndex == 11) setLead(NOTE_G5);
  if (stepIndex == 13) setLead(NOTE_E5);
  if (stepIndex == 15) setLead(NOTE_C5);

  // Piano elettronico
  if (barCounter % 4 == 0 || barCounter % 4 == 1) {
    if (stepIndex == 0)  setPiano(NOTE_C4);
    if (stepIndex == 4)  setPiano(NOTE_E4);
    if (stepIndex == 8)  setPiano(NOTE_G4);
    if (stepIndex == 12) setPiano(NOTE_C5);
  }

  if (barCounter % 4 == 2 || barCounter % 4 == 3) {
    if (stepIndex == 0)  setPiano(NOTE_A3);
    if (stepIndex == 4)  setPiano(NOTE_C4);
    if (stepIndex == 8)  setPiano(NOTE_E4);
    if (stepIndex == 12) setPiano(NOTE_A4);
  }

  // FX automatici
  if (barCounter % 4 == 3 && stepIndex == 15) {
    triggerLaser();
  }

  if (barCounter % 8 == 7 && stepIndex == 15) {
    triggerSiren();
    triggerCrash();
  }

  if (barCounter % 6 == 5 && stepIndex == 7) {
    triggerRobot();
  }

  if (barCounter % 12 == 11 && stepIndex == 12) {
    triggerRiser();
  }

  if (barCounter % 16 == 15 && stepIndex == 0) {
    triggerCrash();
  }

  // Scale cromatiche
  if (barCounter % 2 == 1 && stepIndex == 0) {
    triggerScaleUp();
  }

  if (barCounter % 4 == 3 && stepIndex == 8) {
    triggerScaleDown();
  }

  if (barCounter % 6 == 5 && stepIndex == 4) {
    triggerScaleRandom();
  }

  if (barCounter % 8 == 7 && stepIndex == 12) {
    triggerHighSirenScale();
  }

  stepIndex++;

  if (stepIndex >= 16) {
    stepIndex = 0;
    barCounter++;
  }
}

// -----------------------------
// SETUP
// -----------------------------
void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(AUDIO_PIN, OUTPUT);

  // GPIO16 / D0: bottone verso 3.3V
  pinMode(FX_BUTTON_PIN, INPUT_PULLDOWN_16);

  analogWriteRange(1023);
  analogWriteFreq(40000);

  randomSeed(analogRead(A0));

  analogWrite(AUDIO_PIN, PWM_CENTER);

  delay(500);

  lastSampleMicros = micros();
  lastStepMillis = millis();

  Serial.println("WalkerRemoteBoard Synth started");
  Serial.println("POWER-UP arcade button on D0 / GPIO16");
}

// -----------------------------
// LOOP
// -----------------------------
void loop() {
  checkFxButton();
  audioEngine();
  sequencer();
}
