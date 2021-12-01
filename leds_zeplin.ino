#include <Adafruit_NeoPixel.h>

#define LEDS_PIN   6 // On Trinket or Gemma, suggest changing this to 1
#define NUMPIXELS  100 // Popular NeoPixel ring size

#define ANALOG_UP   1000
#define ANALOG_DOWN 10

#define MICROPHONE_SAMPLING_PERIOD  1
#define AMBIANT_NOISE_TIME_CONSTANT 5000

#define CLAP_THRESHOLD        80
#define CLAP_TOLERANCE        5

bool CLAP_SEQUENCE[7] = {0, 1, 0, 1, 0, 1, 0};
int current_index_in_clap_sequence = 0;
int current_tolerance = CLAP_TOLERANCE;

float ambient_noise = 0;
unsigned long last_ambient_noise_update_time = 0;

float noise_local_max = 0;
unsigned long last_noise_local_max_update_time = 0;

#define NOISE_RECORD_LIST_SIZE   10
int noise_record_list[NOISE_RECORD_LIST_SIZE] = {0};
int noise_record_list_index = 0;

Adafruit_NeoPixel pixels(NUMPIXELS, LEDS_PIN, NEO_GRB + NEO_KHZ800);

const uint8_t MICROPHONE_PIN = A5;
bool leds_on = true;

bool leds_strombo = true;  // flip-flap variable


void setup() {
  Serial.begin(115200);

  pinMode(A0, INPUT);  // color potentiometer
  pinMode(A1, INPUT);  // frequence potentiometer
  pinMode(MICROPHONE_PIN, INPUT);  // microphone
  pinMode(LEDS_PIN, OUTPUT);

  pixels.begin(); // INITIALIZE NeoPixel strip object (REQUIRED)
}

void loop() {
  uint32_t color = read_color();
  int strombo_period = read_strombo_period();

  test_noise_level(MICROPHONE_PIN, 20);

  if(strombo_period == 0){ // strombo and detect clap cannot cohabitate
    //if(detect_clap(MICROPHONE_PIN)){
    //  leds_on = not(leds_on);
    //}
    leds_on = true;

    if(leds_on){
      update_leds(color, 100);
    } else {
      update_leds(pixels.Color(0, 0, 0), 100);
    }
  } else if(strombo_period > 0){
    color = update_strombo(strombo_period, color);
    update_leds(color, 100);
  } else if(strombo_period == -1) {
    float noise_level = read_noise_level(MICROPHONE_PIN);
    int percentage = int(noise_level);
    update_leds(color, percentage);
  }
}


// INTERFACE
uint32_t read_color() {
  int potar_1 = analogRead(A0); // valeur entre 0 et 1023  
  uint32_t pixelHue = potar_1 * 65536L / 1023;

  uint32_t color = pixels.Color(0, 0, 0);
  if(potar_1 <= ANALOG_DOWN){
    color = pixels.Color(0, 0, 0); // éteint
  } else if(potar_1 >= ANALOG_UP) {
    color = pixels.Color(255, 255, 255); // blanc
  } else {
    color = pixels.gamma32(pixels.ColorHSV(pixelHue, 255, 255)); // couleur variable
  }

  return(color);
}

int read_strombo_period() {
  int potar_2 = analogRead(A1); // valeur entre 0 et 1023  
  int strombo_period = potar_2/2;
  
  if(potar_2 <= ANALOG_DOWN){
    // period = 0 <=> eclairage fixe
    strombo_period = 0;
  }

  if(potar_2 >= ANALOG_UP){
    // period = -1 <=> dependance au son
    strombo_period = -1;
  }
  
  return(strombo_period);
}

// LEDS
void update_leds(uint32_t color, int percentage) {
  pixels.clear();

  if(percentage == 100){
    for(int i=0; i<pixels.numPixels(); i++) {
      pixels.setPixelColor(i, color);
    }
  } else {
    for(int i=0; i<pixels.numPixels(); i++) {
      if(random(100) < percentage) {
        pixels.setPixelColor(i, color);
      } else {
        pixels.setPixelColor(i, 0, 0, 0);
      }
    }
  }

  pixels.show();
}

void level_on_leds(int level) {
  pixels.clear();

  for(int i=0; i<level; i++) {
    pixels.setPixelColor(i, 255, 0, 0);
  }

  pixels.show();
}

uint32_t update_strombo(int strombo_period, uint32_t color) {
  int potar_1 = analogRead(A0); // valeur entre 0 et 1023  
  uint32_t pixelHue = potar_1 * 65536L / 1023;

  uint32_t new_color;

  if (leds_strombo){
    delay(strombo_period);
    new_color = color;
  } else {
    delay(10);
    new_color = pixels.Color(0, 0, 0);
  }
  leds_strombo = not(leds_strombo);

  return(new_color);
}

// MICROPHONE
float read_noise_level(const uint8_t microphone_pin){
  return(read_noise_level(microphone_pin, MICROPHONE_SAMPLING_PERIOD));
}

float read_noise_level(const uint8_t microphone_pin, const int sampling_period) {
  int signal_micro = analogRead(microphone_pin);
  return(signal_micro);
}

float test_noise_level(const uint8_t microphone_pin, const int sampling_period) {
# define samples_number 500
  int sample_list[samples_number] = {0};
  float ambient_list[samples_number] = {0};
  int local_max_list[samples_number] = {0};

  Serial.println("- 3 -");
  delay(1000);
  Serial.println("- 2 -");
  delay(1000);
  Serial.println("- 1 -");
  delay(1000);
  Serial.println("-TOP-");

  for (int i=0; i<samples_number; i++) {
    int signal_micro = analogRead(microphone_pin);

    unsigned long current_time = millis();
    float ambient = update_ambient_noise(signal_micro, current_time);
    int local_max = update_noise_local_max(signal_micro, current_time);

    sample_list[i] = signal_micro;
    ambient_list[i] = ambient;
    local_max_list[i] = local_max;

    //Serial.println(noise_record_list_index);
    delay(sampling_period);
  }

  Serial.println(samples_number);

  for (int i=0; i<samples_number; i++) {
    Serial.print(String(sample_list[i]));
    Serial.print(", ");
    Serial.print(String(ambient_list[i]));
    Serial.print(", ");
    Serial.println(String(local_max_list[i]));
  }
  Serial.println("#######################################");
  delay(200000);

  return(0);
}

/*
bool detect_clap(const uint8_t microphone_pin) {
  float noise_level = read_noise_level(microphone_pin);
  update_ambient_noise(noise_level);

  bool clap_detected = (noise_level - ambient_noise > CLAP_THRESHOLD);
  
  if(CLAP_SEQUENCE[current_index_in_clap_sequence + 1] == clap_detected) {
    current_index_in_clap_sequence += 1;
  } else {
    if(current_tolerance > 0) {
      // extra life is used
      current_tolerance -= 1;
    } else {
      current_index_in_clap_sequence = 0;
      current_tolerance = CLAP_TOLERANCE;
    }
  }

  //Serial.println("ambient : " + String(ambient_noise) + " | current : " + String(noise_level) + " | index : " + String(current_index_in_clap_sequence));

  if(current_index_in_clap_sequence == (sizeof(CLAP_SEQUENCE)/sizeof(CLAP_SEQUENCE[0])) - 1){
    // all the sequence has been done
    current_index_in_clap_sequence = 0;
    current_tolerance = CLAP_TOLERANCE;
    return(true);
  }
  return(false);
}
*/

float update_ambient_noise(const float noise_level, unsigned long current_time) {
  int dt = current_time - last_ambient_noise_update_time;

  ambient_noise = ambient_noise + (noise_level - ambient_noise) * dt/AMBIANT_NOISE_TIME_CONSTANT;

  last_ambient_noise_update_time = current_time;
  return ambient_noise;
}

int update_noise_local_max(const float noise_level, unsigned long current_time) {
  int dt = current_time - last_noise_local_max_update_time;

  noise_local_max = noise_record_list[0];
  for (int i = 0; i < (sizeof(noise_record_list) / sizeof(noise_record_list[0])); i++) {
    noise_local_max = max(noise_record_list[i], noise_local_max);
  }

  noise_record_list[noise_record_list_index] = noise_level;
  noise_record_list_index = (noise_record_list_index + 1) % NOISE_RECORD_LIST_SIZE;

  last_noise_local_max_update_time = current_time;
  return noise_local_max;
}
