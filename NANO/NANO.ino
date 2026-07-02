#include <PDM.h>
#include <ambulance_inferencing.h>
#include <SPI.h>
#include <MFRC522.h>

// ====================== RFID ======================
#define SS_PIN  10
#define RST_PIN 9
MFRC522 rfid(SS_PIN, RST_PIN);

String registeredCards[] = {
  "CA E0 31 E9",
  "7A 35 58 E9",
  "EA 5D 48 E9",
  "89 64 02 07"
};
const int numRegistered = sizeof(registeredCards) / sizeof(registeredCards[0]);

// ====================== SIREN SETTINGS ======================
int sirenConfirmCount = 0;
const int REQUIRED_CONFIRMS = 4;      // Increased to 4
const float SIREN_THRESHOLD = 0.94;   // Increased to 0.94

// ====================== AUDIO ======================
typedef struct {
    int16_t *buffer;
    uint8_t buf_ready;
    uint32_t buf_count;
    uint32_t n_samples;
} inference_t;

static inference_t inference;
static signed short sampleBuffer[2048];
static bool debug_nn = false; 

void setup() {
    Serial.begin(115200);
    while (!Serial);

    SPI.begin();
    rfid.PCD_Init();

    if (microphone_inference_start(EI_CLASSIFIER_RAW_SAMPLE_COUNT) == false) {
        Serial.println("ERR: Could not allocate audio buffer");
        while(1);
    }

    Serial.println("=====================================");
    Serial.println("   SMART TRAFFIC EMERGENCY SYSTEM");
    Serial.println("=====================================");
    Serial.print("✅ ");
    Serial.print(numRegistered);
    Serial.println(" Ambulance cards registered");
    Serial.print("Siren will be confirmed only after ");
    Serial.print(REQUIRED_CONFIRMS);
    Serial.println(" consecutive detections (Threshold 0.94)\n");
}

void loop() {
    // RFID Part (unchanged)
    if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
        String uid = "";
        for (byte i = 0; i < rfid.uid.size; i++) {
            if (rfid.uid.uidByte[i] < 0x10) uid += " 0";
            else uid += " ";
            uid += String(rfid.uid.uidByte[i], HEX);
        }
        uid.trim();

        Serial.print("Scanned UID: ");
        Serial.println(uid);

        bool isAmbulance = false;
        for (int i = 0; i < numRegistered; i++) {
            if (uid.equalsIgnoreCase(registeredCards[i])) {
                isAmbulance = true;
                break;
            }
        }

        if (isAmbulance) {
            Serial.println("🚨 REGISTERED AMBULANCE CARD DETECTED!");
            Serial.println("AMBULANCE_CARD");
        } else {
            Serial.println("→ Unknown card ignored");
        }

        rfid.PICC_HaltA();
        delay(1500);
    }

    // === SIREN DETECTION with 4x Confirmation ===
    bool m = microphone_inference_record();
    if (m) {
        signal_t signal;
        signal.total_length = EI_CLASSIFIER_RAW_SAMPLE_COUNT;
        signal.get_data = &microphone_audio_signal_get_data;

        ei_impulse_result_t result = { 0 };
        EI_IMPULSE_ERROR r = run_classifier(&signal, &result, debug_nn);

        if (r == EI_IMPULSE_OK) {
            bool detectedNow = false;

            for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
                if (strcmp(result.classification[ix].label, "ambulance") == 0 && 
                    result.classification[ix].value > SIREN_THRESHOLD) {
                    detectedNow = true;
                    break;
                }
            }

            if (detectedNow) {
                sirenConfirmCount++;
                Serial.print("Siren confidence high (");
                Serial.print(sirenConfirmCount);
                Serial.print("/");
                Serial.print(REQUIRED_CONFIRMS);
                Serial.println(")");

                if (sirenConfirmCount >= REQUIRED_CONFIRMS) {
                    Serial.println("🚨 SIREN_DETECTED (Confirmed)");
                    Serial.println("SIREN_DETECTED");
                    sirenConfirmCount = 0;
                    delay(5000);        // 5 second cooldown
                }
            } else {
                if (sirenConfirmCount > 0) {
                    Serial.println("Siren confidence dropped → Resetting counter");
                }
                sirenConfirmCount = 0;
            }
        }
    }
}

// ====================== HELPER FUNCTIONS ======================
static void pdm_data_ready_inference_callback(void) {
    int bytesAvailable = PDM.available();
    int bytesRead = PDM.read((char *)&sampleBuffer[0], bytesAvailable);
    if (inference.buf_ready == 0) {
        for(int i = 0; i < bytesRead>>1; i++) {
            inference.buffer[inference.buf_count++] = sampleBuffer[i];
            if(inference.buf_count >= inference.n_samples) {
                inference.buf_count = 0;
                inference.buf_ready = 1;
                break;
            }
        }
    }
}

static bool microphone_inference_start(uint32_t n_samples) {
    inference.buffer = (int16_t *)malloc(n_samples * sizeof(int16_t));
    if(inference.buffer == NULL) return false;
    inference.buf_count = 0;
    inference.n_samples = n_samples;
    inference.buf_ready = 0;
    PDM.onReceive(&pdm_data_ready_inference_callback);
    PDM.setBufferSize(4096);
    if (!PDM.begin(1, EI_CLASSIFIER_FREQUENCY)) return false;
    PDM.setGain(127);
    return true;
}

static bool microphone_inference_record(void) {
    inference.buf_ready = 0;
    inference.buf_count = 0;
    while(inference.buf_ready == 0) { delay(10); }
    return true;
}

static int microphone_audio_signal_get_data(size_t offset, size_t length, float *out_ptr) {
    numpy::int16_to_float(&inference.buffer[offset], out_ptr, length);
    return 0;
}