
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// what library versions?
// i should write all that down...


#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire);


#include <Adafruit_TLV320DAC3100.h>

Adafruit_TLV320DAC3100 codec; // Create codec object
#define TLV_RESET 13

#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>
#include <string.h>


// // GUItool: begin automatically generated code
// AudioSynthWaveformSine   sine1;          //xy=144.3333282470703,153.33334350585938
// AudioOutputI2S           i2s1;           //xy=389.3333282470703,185.3333282470703
// AudioConnection          patchCord1(sine1, 0, i2s1, 0);
// AudioConnection          patchCord2(sine1, 0, i2s1, 1);
// // GUItool: end automatically generated code



AudioPlaySdWav           playWav1;
AudioOutputI2S           audioOutput;
AudioConnection          patchCord1(playWav1, 0, audioOutput, 0);
AudioConnection          patchCord2(playWav1, 1, audioOutput, 1);






#define MAX_NUM_FILES 20
#define MAX_FILENAME_LEN 0xff // this should be exposed by FS library

// no dynamic allocation
char filenames[MAX_NUM_FILES][MAX_FILENAME_LEN];
int numFiles = 0; 

// which file is in selection/playing
#define NO_FILE_SELECTED -1
int fileSelect = NO_FILE_SELECTED;

// a file index [0-(numFiles-1)]
// how far is the screen scrolling
int fileOffset = 0;




#define VOLUME_SELECT 0
#define FILE_SELECT 1


uint8_t device_state = VOLUME_SELECT;
uint8_t volume;


// so volume step is 8, and there are 16
#define MAX_VOLUME 128 / 8


#define BUTTON_UP 12
#define BUTTON_DOWN 11
#define BUTTON_SELECT 10
#define BUTTON_SWITCH_STATE 9





void drawFilenames() {
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.setTextColor(SSD1306_WHITE);

  // cursor is upper left corner with adafruit gfx basic font
  // ?
  int i = fileOffset;

  while (display.getCursorY() < 64) {
    if (i == numFiles) break;
    if (i == fileSelect) display.print(">");
    display.println(filenames[i]);    
    i++;
  }

  if (display.getCursorY() < 64) {
    display.println("*");
  }

  display.display();
}

void selectDown() {
  if (fileOffset < numFiles) fileOffset++;
}

void selectUp() {
  if (fileOffset > 0) fileOffset--;
}


void drawVolume() {
  display.clearDisplay();

  // volume * 8 = number of pixels wide
  display.drawRect(0, 0, volume*8, 16, SSD1306_WHITE);
  display.display();
}


void halt(const char *message) {
  Serial.println(message);
  while (1)
    yield(); // Function to halt on critical errors
}


void playFile(const char *filename) {
  Serial.print("Playing file: ");
  Serial.println(filename);

  // Start playing the file.  This sketch continues to
  // run while the file plays.
  // playMp31.play(filename);
  if (!playWav1.play(filename)) {
    Serial.println("error at play");
  }

  delay(25);

  // wait?
}


void setup() {
  Serial.begin(9600);


  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  if (!SD.begin(BUILTIN_SDCARD)) {
    Serial.println("SD.begin() failed");
    return;
  }

  // iterate files
  File root = SD.open("/");
  File nextFile;
  numFiles = 0;

  while(1) {
    nextFile = root.openNextFile();
    if (!nextFile) break;
    Serial.printf("there exists: %s\n", nextFile.name());
    numFiles ++;
  }

  
  root.rewindDirectory();
  for (int i = 0; i < numFiles; i++) {
    nextFile = root.openNextFile();
    if (!nextFile) break;
    
    // filename string
    // is owned by the file object it seems
    // we need to copy

    strcpy(filenames[i], nextFile.name());
  }

  // update visuals after underlying data is changed
  device_state = FILE_SELECT;
  drawFilenames();





  // test display

  // display.clearDisplay();

  // display.setTextSize(1);              // Normal 1:1 pixel scale
  // display.setTextColor(SSD1306_WHITE); // Draw white text
  // display.setCursor(0,0);              // Start at top-left corner
  // display.println(F("Hello, world!"));

  // display.setTextColor(SSD1306_BLACK, SSD1306_WHITE); // Draw 'inverse' text
  // display.println(3.141592);

  // display.setTextSize(2);             // Draw 2X-scale text
  // display.setTextColor(SSD1306_WHITE);
  // display.print(F("0x")); display.println(0xDEADBEEF, HEX);

  // display.display();
  // delay(2000);




  // our pins
  // they pull hign by default
  // because of the module?
  pinMode(9, INPUT);
  pinMode(10, INPUT);
  pinMode(11, INPUT);
  pinMode(12, INPUT);

  
  // set led ON for fun
  digitalWrite(13, HIGH);


  // audio chip setup stuff

  pinMode(TLV_RESET, OUTPUT);
  digitalWrite(TLV_RESET, LOW);
  delay(100);
  digitalWrite(TLV_RESET, HIGH);


  Serial.println("Init TLV DAC");
  if (!codec.begin()) {
    halt("Failed to initialize codec!");
  }
  delay(10);

  // Interface Control
  if (!codec.setCodecInterface(TLV320DAC3100_FORMAT_I2S,     // Format: I2S
                               TLV320DAC3100_DATA_LEN_16)) { // Length: 16 bits
    halt("Failed to configure codec interface!");
  }

  // Clock MUX and PLL settings
  if (!codec.setCodecClockInput(TLV320DAC3100_CODEC_CLKIN_PLL) ||
      !codec.setPLLClockInput(TLV320DAC3100_PLL_CLKIN_BCLK)) {
    halt("Failed to configure codec clocks!");
  }

  if (!codec.setPLLValues(1, 2, 32, 0)) { // P=2, R=2, J=32, D=0
    halt("Failed to configure PLL values!");
  }

  // DAC/ADC Config
  if (!codec.setNDAC(true, 8) || // Enable NDAC with value 8
      !codec.setMDAC(true, 2)) { // Enable MDAC with value 2
    halt("Failed to configure DAC dividers!");
  }

  if (!codec.powerPLL(true)) { // Power up the PLL
    halt("Failed to power up PLL!");
  }

  // DAC Setup
  if (!codec.setDACDataPath(true, true,                    // Power up both DACs
                            TLV320_DAC_PATH_NORMAL,        // Normal left path
                            TLV320_DAC_PATH_NORMAL,        // Normal right path
                            TLV320_VOLUME_STEP_1SAMPLE)) { // Step: 1 per sample
    halt("Failed to configure DAC data path!");
  }

  if (!codec.configureAnalogInputs(TLV320_DAC_ROUTE_MIXER, // Left DAC to mixer
                                   TLV320_DAC_ROUTE_MIXER, // Right DAC to mixer
                                   false, false, false,    // No AIN routing
                                   false)) {               // No HPL->HPR
    halt("Failed to configure DAC routing!");
  }

  // DAC Volume Control
  if (!codec.setDACVolumeControl(
          false, false, TLV320_VOL_INDEPENDENT) || // Unmute both channels
      !codec.setChannelVolume(false, 5) ||        // Left DAC +0dB
      !codec.setChannelVolume(true, 5)) {         // Right DAC +0dB
    halt("Failed to configure DAC volumes!");
  }


  // Headphone and Speaker Setup
  if (!codec.configureHeadphoneDriver(
          true, true,                     // Power up both drivers
          TLV320_HP_COMMON_1_35V,         // Default common mode
          false) ||                       // Don't power down on SCD
      !codec.configureHPL_PGA(0, true) || // Set HPL gain, unmute
      !codec.configureHPR_PGA(0, true) || // Set HPR gain, unmute
      !codec.setHPLVolume(true, 3) ||     // Enable and set HPL volume
      !codec.setHPRVolume(true, 3)) {     // Enable and set HPR volume
    halt("Failed to configure headphone outputs!");
  }

  // if (!codec.enableSpeaker(true) ||                // Dis/Enable speaker amp
  //     !codec.configureSPK_PGA(TLV320_SPK_GAIN_6DB, // Set gain to 6dB
  //                             true) ||             // Unmute
  //     !codec.setSPKVolume(true, 0)) { // Enable and set volume to 0dB
  //   halt("Failed to configure speaker output!");
  // }


  if (!codec.configMicBias(false, true, TLV320_MICBIAS_AVDD) ||
      !codec.setHeadsetDetect(true) ||
      !codec.setInt1Source(true, true, false, false, false,
                           false) || // GPIO1 is detect headset or button press
      !codec.setGPIO1Mode(TLV320_GPIO1_INT1)) {
    halt("Failed to configure headset detect");
  }


  Serial.println("TLV config done!");


  // teensy audio stuff

  AudioMemory(10);

  // sine1.amplitude(1.0);
  // sine1.frequency(440);


  // offset volume
  codec.setChannelVolume(false, volume-30);
  codec.setChannelVolume(true, volume-30);


  // error?
  playFile("01 - Afro-Blue.wav");
  
}

void loop() {
  // Serial.printf("%d %d %d %d\n", digitalRead(9), digitalRead(10), digitalRead(11), digitalRead(12));

  int up   = !digitalRead(BUTTON_UP);
  int down = !digitalRead(BUTTON_DOWN);
  int select = !(digitalRead(BUTTON_SELECT));
  int sw = !digitalRead(BUTTON_SWITCH_STATE);

  if (sw) {
    Serial.println("here");
    // what's the thing? tertiary
    if (device_state == VOLUME_SELECT) {
      device_state = FILE_SELECT;
      drawFilenames();
    } else {
      device_state = VOLUME_SELECT;
      drawVolume();
    }
  }

  if (device_state == VOLUME_SELECT) {
    if (up) {
      if (volume < MAX_VOLUME) {
        volume ++;

        // no error check


        // offset volume
        codec.setChannelVolume(false, volume-30);
        codec.setChannelVolume(true, volume-30);
      } 
    }
    else if (down) {
      if (volume > 0) {
        volume --;

        // offset volume 
        codec.setChannelVolume(false, volume-30);
        codec.setChannelVolume(true, volume-30);
      }
    }

    if (up || down) {
      drawVolume();
    }
  }
  else if (device_state == FILE_SELECT) {
    
    if (up) {
      selectUp();
    }
    else if (down) {
      selectDown();
    }

    if (select) {
      fileSelect = fileOffset;

      // error check?
      playFile(filenames[fileSelect]);

    }
    if (up || down || select) {
      drawFilenames();
    }
  }


  // delay for debounce
  delay(100);
}
