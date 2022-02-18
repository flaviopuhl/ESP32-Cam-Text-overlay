/*
 _______  _______  __   __  _______    _______  __   __  _______  ______    ___      _______  __   __ 
|       ||       ||  |_|  ||       |  |       ||  | |  ||       ||    _ |  |   |    |   _   ||  | |  |
|_     _||    ___||       ||_     _|  |   _   ||  |_|  ||    ___||   | ||  |   |    |  |_|  ||  |_|  |
  |   |  |   |___ |       |  |   |    |  | |  ||       ||   |___ |   |_||_ |   |    |       ||       |
  |   |  |    ___| |     |   |   |    |  |_|  ||       ||    ___||    __  ||   |___ |       ||_     _|
  |   |  |   |___ |   _   |  |   |    |       | |     | |   |___ |   |  | ||       ||   _   |  |   |  
  |___|  |_______||__| |__|  |___|    |_______|  |___|  |_______||___|  |_||_______||__| |__|  |___|  

 Name:     ESP32 Cam Text Overlay
 Date:     FEB 2022
 Author:   Flavio L Puhl Jr <flavio_puhl@hotmail.com> 
 GIT:      
 About:    Example of text overlay in a picture taken by ESP32 Cam
 
Update comments                                      
+-----------------------------------------------------+------------------+---------------+
|               Feature added                         |     Version      |      Date     |
+-----------------------------------------------------+------------------+---------------+
| Initial Release                                     |      1.0.0       |     FEB/22    |
|                                                     |                  |               |
|                                                     |                  |               |
+-----------------------------------------------------+------------------+---------------+


Library versions                                       
+-----------------------------------------+------------------+-------------------------- +
|       Library                           |     Version      |          Creator          |
+-----------------------------------------+------------------+-------------------------- +
|	LittleFS_esp32                          |     @^1.0.6      |      lorol                |
+-----------------------------------------+------------------+-------------------------- +

Upload settings 
+----------------------------------------------------------------------------------------+
| PLATFORM: Espressif 32 (3.3.0) > AI Thinker ESP32-CAM                                  |
| HARDWARE: ESP32 240MHz, 320KB RAM, 4MB Flash                                           |
| PACKAGES:                                                                              |
|  - framework-arduinoespressif32 3.10006.210326 (1.0.6)                                 |
|  - tool-esptoolpy 1.30100.210531 (3.1.0)                                               |
|  - toolchain-xtensa32 2.50200.97 (5.2.0)                                               |
|                                                                                        |
| RAM:   [=         ]   9.2% (used 30220 bytes from 327680 bytes)                        |
| Flash: [==        ]  15.1% (used 475610 bytes from 3145728 bytes)                      |
+----------------------------------------------------------------------------------------+

*/

/*+--------------------------------------------------------------------------------------+
 *| Libraries                                                                            |
 *+--------------------------------------------------------------------------------------+ */

// Libraries built into IDE


#include "esp_camera.h"
#include "Arduino.h"

#include "img_converters.h"                                 // Txt overlay testing
#include "esp_timer.h"
#include "img_converters.h"
#include "fb_gfx.h"
#include "fd_forward.h"
#include "fr_forward.h"

#include "SD_MMC.h"                                         // SD Card ESP32
#include "soc/soc.h"                                        // Disable brownout problems
#include "soc/rtc_cntl_reg.h"                               // Disable brownout problems
#include "driver/rtc_io.h"
#include <EEPROM.h>                                         // read and write from flash memory
#include <SPIFFS.h>
#include "LittleFS.h"
#include "FS.h"


/*+--------------------------------------------------------------------------------------+
 *| Constants declaration                                                                |
 *+--------------------------------------------------------------------------------------+ */

// define the number of bytes you want to access
#define EEPROM_SIZE 512

// Pin definition for CAMERA_MODEL_AI_THINKER
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

#define FILE_PHOTO_FS "/data/txtOvl_photo.jpg"                // Photo File Name to save in File System

boolean takeNewPhoto = true;
bool taskCompleted = false;


/*+--------------------------------------------------------------------------------------+
 *| Global Variables                                                                     |
 *+--------------------------------------------------------------------------------------+ */

  
  String fileName = "empty";                                  // filename that will be sent on MQTT

 
/*+--------------------------------------------------------------------------------------+
 *| Text Overlay                                                                         |
 *+--------------------------------------------------------------------------------------+ */

static void rgb_print(dl_matrix3du_t *image_matrix, uint32_t color, const char * str){
    fb_data_t fb;
    fb.width = image_matrix->w;
    fb.height = image_matrix->h;
    fb.data = image_matrix->item;
    fb.bytes_per_pixel = 3;
    fb.format = FB_BGR888;
    //fb_gfx_print(&fb, (fb.width - (strlen(str) * 14)) / 2, 10, color, str);
    fb_gfx_print(&fb, (fb.width - (strlen(str) * 14)) / 2, fb.height-20, color, str);
}


/*+--------------------------------------------------------------------------------------+
 *| Initiate File System                                                                 |
 *+--------------------------------------------------------------------------------------+ */

void initFS(){

  if (!SPIFFS.begin(true)) {
    log_i("An Error has occurred while mounting FS");
      ESP.restart();
  }  else {
    log_i("File System mounted successfully");
      delay(500);
  }
}

/*+--------------------------------------------------------------------------------------+
 *| Initiate SDCARD                                                                      |
 *+--------------------------------------------------------------------------------------+ */
  
  void initSDCARD(){

    //if(!SD_MMC.begin()){
    if(!SD_MMC.begin("/sdcard", true)){               // note: ("/sdcard", true) = 1 wire - see: https://www.reddit.com/r/esp32/comments/d71es9/a_breakdown_of_my_experience_trying_to_talk_to_an/
      log_i("SD Card Mount Failed");
      return;
    } else {
      log_i("SD Card Mount successfull");
    }
  

    uint8_t cardType = SD_MMC.cardType();

    if(cardType == CARD_NONE){
      log_i("No SD Card attached");
        return;
    } else {
      log_i("SD Card attached");
    }

  }

/*+--------------------------------------------------------------------------------------+
 *| Initiate EEPROM                                                                      |
 *+--------------------------------------------------------------------------------------+ */
  
void initEEPROM(){

  if (!EEPROM.begin(EEPROM_SIZE)){                                  // Initialize EEPROM with predefined size
      log_i("failed to initialise EEPROM...");
//      ESP.restart();
    } else {
      log_i("Success to initialise EEPROM...");
    }

}
/*+--------------------------------------------------------------------------------------+
 *| Initiate CAMERA                                                            |
 *+--------------------------------------------------------------------------------------+ */
  
  void initCAMERA(){

    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sscb_sda = SIOD_GPIO_NUM;
    config.pin_sscb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 10000000;                             // https://github.com/espressif/esp32-camera/issues/93
    config.pixel_format = PIXFORMAT_JPEG;                       // YUV422,GRAYSCALE,RGB565,JPEG
    
    if(psramFound()){                                           // https://github.com/espressif/esp-who/issues/83
      log_i("PSRAM found");
      //config.frame_size = FRAMESIZE_UXGA; 						        // FRAMESIZE_ + QVGA  ( 320 x 240 )
      config.frame_size = FRAMESIZE_VGA; 						            // FRAMESIZE_ + QVGA  ( 320 x 240 ) 
      config.jpeg_quality = 10;                                 //              CIF   ( 352 x 288)
      config.fb_count = 1;                                      //              VGA   ( 640 x 480 )
    } else {                                                    //              SVGA  ( 800 x 600 )
      log_i("PSRAM not found");                                 //              XGA   ( 1024 x 768 )
      config.frame_size = FRAMESIZE_SVGA;                       //              SXGA  ( 1280 x 1024 )
      config.jpeg_quality = 12;                                 //              UXGA  ( 1600 x 1200 )
      config.fb_count = 1;
    }

    if(psramInit()){
      log_i("PSRAM initiated");
    } else {
      log_i("PSRAM initiation failed");
    }


    int tt = 0;
    esp_err_t err;
    do{
      err = esp_camera_init(&config);
        if (err != ESP_OK) {
          log_i("Camera init failed with error 0x%x", err);
          log_i("Init trial %i", tt);        
          tt++;
        } else {
          log_i("Camera init successfull");
        }
    } while (err != ESP_OK && tt<=20);

    if(tt >= 20){
      delay(500);
      ESP.restart();
      }

  }


/*+--------------------------------------------------------------------------------------+
 *| Take a picture, save to SD Card, save to SPPIFFS, Send to firebase                   |
 *+--------------------------------------------------------------------------------------+ */
 
void takeImage(){
  
  log_i("Taking a photo...");
  
  int pictureNumber = 0;
   
  camera_fb_t * fb = NULL;                                            // Reset camera pointer
  
  bool CamCaptureSucess = true;                                       // Boolean indicating if the picture has been taken correctly
  bool CamCaptureSize = true;

  fb = esp_camera_fb_get();                                           // Take Picture with Camera  
    if(!fb) {
      CamCaptureSucess = false;
        log_i("Camera capture failed...");

    } else {
      CamCaptureSucess = true;
        log_i("Camera capture success...");

      EEPROM.get(8,pictureNumber);                                            // Recover picture counter from EEPROM
        log_i("Current picture number counter : %i",pictureNumber);
        
        pictureNumber++;                                                    
        EEPROM.put(8,pictureNumber);                                                                                                            
        EEPROM.commit();                                                                                                                                        
        EEPROM.get(8,pictureNumber);                                          // Recover new picture counter and use it to name the file on SD CARD
          log_i("New picture number counter : %i",pictureNumber);

    } 

      // Text overlay START

        String txtOverlay = "ESP32 Cam Text Overlay example - " + String(pictureNumber); 
        const char* txtOverlay_char = txtOverlay.c_str();

        dl_matrix3du_t *image_matrix = dl_matrix3du_alloc(1, fb->width, fb->height, 3);
        fmt2rgb888(fb->buf, fb->len, fb->format, image_matrix->item);  
        rgb_print(image_matrix, 0x00000000, txtOverlay_char);               // https://www.littlewebhut.com/css/value_color/

        size_t _jpg_buf_len = 0;
        uint8_t * _jpg_buf = NULL;
        fmt2jpg(image_matrix->item, fb->width*fb->height*3, fb->width, fb->height, PIXFORMAT_RGB888, 90, &_jpg_buf, &_jpg_buf_len);
          //int available_PSRAM_size = ESP.getFreePsram();
          int available_PSRAM_size = ESP.getFreeHeap();
          log_i("PSRAM Size available (bytes)           : %i", available_PSRAM_size);

        dl_matrix3du_free(image_matrix);

          int available_PSRAM_size_after = ESP.getFreeHeap();
          log_i("PSRAM Size available after free (bytes): %i", available_PSRAM_size_after);
      
      // Text overlay END

  if (CamCaptureSucess == true){ 

    int t = 0;
    unsigned int pic_sz = 0;
      
    File fileFS = SPIFFS.open(FILE_PHOTO_FS, FILE_WRITE);             // Save picture on FS
    log_i("Picture file name (FS): %s", FILE_PHOTO_FS);               // FS Photo file name
    // Insert the data in the photo file
    if (!fileFS) {
      log_i("Failed to open file (FS) in writing mode");
    } else {
      log_i("File (FS) open in writing mode : %s",FILE_PHOTO_FS);

        do{
        fileFS.write(_jpg_buf, _jpg_buf_len);
        log_i("The picture has been saved in (FS) ");
        pic_sz = fileFS.size();                                     
          log_i("File Size: %i bytes | read trial: %i of 20", pic_sz, t);
          t++;
        } while (pic_sz == 0 && t <= 20);
          if(t >= 20){ESP.restart();}                                 // Addedd due to error "fmt2jpg(): JPG buffer malloc failed" (not enough chunk of RAM to store data)
    }

    fileFS.close();

   
    if ( pic_sz > 0 ){
      log_i("Picture size is valid ... ");
        CamCaptureSize = true;
    } else {
      log_i("Picture size is not valid ... ");
        CamCaptureSize = false;
    };

      
    if(CamCaptureSize == true){

        String path = "/picture" + String(pictureNumber) +".jpg";             // Path where new picture will be saved in SD Card 

        fs::FS &fs = SD_MMC;
        fileName = path.c_str();
          log_i("Picture file name (SDCARD): %s", path.c_str());
        
        File fileSDCARD = fs.open(path.c_str(), FILE_WRITE);                  // Save image on SD Card with dynamic name
          if(!fileSDCARD){
            fileName = "fail to save pic";
              log_i("Failed to open file (SDCARD) in writing mode...");
          } else {
            fileSDCARD.write(_jpg_buf, _jpg_buf_len); // payload (image), payload length
              log_i("The picture has been saved in (SDCARD) : %s", path.c_str());
          }
          
          fileSDCARD.close();

        esp_camera_fb_return(fb);
    
    } //CamCaptureSize = true;
        
  } //CamCaptureSucess = false;

  
  // Turns off the ESP32-CAM white on-board LED (flash) connected to GPIO 4
  //pinMode(4, OUTPUT);                   // Those lines must be commented to allow
  //digitalWrite(4, LOW);                 //  the SD card pin to be released
  //rtc_gpio_hold_en(GPIO_NUM_4);         // If not commented, SD card file saving on
                                          //  2nd loop will result in critical fault

 delay(100);

}


/*+--------------------------------------------------------------------------------------+
 *| Setup                                                                                |
 *+--------------------------------------------------------------------------------------+ */
 
void setup() {

  Serial.begin(115200);                                       // Start serial communication at 115200 baud 
  delay(5000); 
  
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); 				          //disable brownout detector

  initSDCARD();
  initFS();
  initCAMERA();
  initEEPROM();

  log_i("Internal Total heap %d, internal Free Heap %d", ESP.getHeapSize(), ESP.getFreeHeap());
  log_i("SPIRam Total heap %d, SPIRam Free Heap %d", ESP.getPsramSize(), ESP.getFreePsram());
  log_i("ChipRevision %d, Cpu Freq %d, SDK Version %s", ESP.getChipRevision(), ESP.getCpuFreqMHz(), ESP.getSdkVersion());
  log_i("Flash Size %d, Flash Speed %d", ESP.getFlashChipSize(), ESP.getFlashChipSpeed());

  takeImage();

 
}


/*+--------------------------------------------------------------------------------------+
 *| main loop                                                                            |
 *+--------------------------------------------------------------------------------------+ */

void loop() {
  

 
}

