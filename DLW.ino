#include <LiquidCrystal.h>
#include <SPI.h>
#include <SD.h>
#include <Adafruit_NeoPixel.h>

/*Code for Timer, used only for debugging:
#include <TimerOne\TimerOne.h>
Timer1.initialize(1 * 1000000);
Timer1.attachInterrupt(FUNCTION);
Timer1.start();
*/

//Pin assignment
#define NPX1Pin 30 //Strip 1
#define NPX2Pin 31 //Strip 2
#define BuzzerPin 3
#define LCDBACKLIGHT 10

//Button values
#define sw_none -1
#define sw_select 4
#define sw_up 1
#define sw_down 2
#define sw_left 3
#define sw_right 0


#define STRIPPIXELS 144		//Leds per Strip
#define TotalPixels = 288	//Total Leds
#define LCDDark 5			//Percentage LED Backlight for darkened display
#define LCDBright 100		//Percentage LED Backlight for bright display

//defaults
int brightness = 25;	//LED Brightness in %
int init_delay = 5000;		//Delay before start in ms
int frame_delay = 0;	//Delay between each line in MICROSECONDS
int repeat = 0;			//Repeats
int repeat_delay = 0;	//Delay between repeats in ms

//Internal variables
int menupos = -2;
int keydelay = 500;		//delay before speed is increased
String menu[8] = {
	"null",				//menu 0
	"File select",		//menu 1
	"Brightness",		//menu 2
	"Init delay",		//menu 3
	"Frame delay",		//menu 4
	"Repeat",			//menu 5
	"Repeat delay" };	//menu 6

//file stuff
String files[256];
int filecount;
int selectedfile = 1;

//Button stuff
int adc_key_val[5] = { 30, 170, 390, 600, 800 };
int NUM_KEYS = 5;
int adc_key_in;
int oldkey = -1;
int key = -1;
int key_old;

//create display
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

//create both strips
Adafruit_NeoPixel npx1 = Adafruit_NeoPixel(STRIPPIXELS, NPX1Pin, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel npx2 = Adafruit_NeoPixel(STRIPPIXELS, NPX2Pin, NEO_GRB + NEO_KHZ800);

//Make it easy to add new strips
class NPX
{
public:
	NPX(){

	}

	void begin(){
		npx1.begin();
		npx2.begin();
	}

	void setBrightness(int i)
	{
		npx1.setBrightness(i);
		npx2.setBrightness(i);
	}

	void show(){
		npx1.show();
		npx2.show();
	}

	void clear(){
		npx1.clear();
		npx2.clear();
	}

	void setPixelColor(int col, uint32_t color){
		if (col < STRIPPIXELS){
			npx1.setPixelColor(col, color);
		}
		else{
			npx2.setPixelColor(col - STRIPPIXELS, color);
		}
	}
};
NPX npx;

//Print function for both display lines
//print(line 1, line 2, postfix of line 2)
void print(String s, String s2 = "", String px = ""){
	lcd.setCursor(0, 0);
	lcd.print(s);
	lcd.print("               ");
	lcd.setCursor(0, 1);
	lcd.print(s2);
	lcd.print(px);
	lcd.print("               ");
}

void debug(String s){
	Serial.println(s);
}

void setup(){
	lcd.begin(16, 2);
	Serial.begin(115200);
	Serial.println("NeoPX MacConker & Lesani");
	menupos = 1;
	npx.begin();
	npx.show();
	SD.begin(53);
	File root = SD.open("/");
	for (int i = 1; i <= 255; i++){
		File file = root.openNextFile();
		if (!file) {
			filecount = i-1;
			break;
		}
		files[i] = file.name();
		Serial.println(file.name());
	}
	Serial.print("Got ");
	Serial.print(filecount);
	Serial.println(" Files:");
	for (int i = 1; i <= filecount; i++){
		Serial.println(files[i]);
	}
}

//print spaces after what was printed before, to clear remaining characters from display
void clear(){
	lcd.print("                ");
}

//Set LCD Brightness in %
void LCDBrightness(int dimmer) {
	dimmer = map(dimmer, 0, 100, 0, 255);
	analogWrite(LCDBACKLIGHT, dimmer);
}

//Set LED Brightness in %
void LEDBrightness(int dimmer){
	dimmer = map(dimmer, 0, 100, 0, 255);
	npx.setBrightness(dimmer);
	npx.show();
}

//Delay that shows in screen
//light=false = completely dark, true = LCDDark until 3s before delay end, then completely dark
void showdelay(long delay, bool light = false, bool beep = false){
	debug("Delaying:");
	Serial.println(delay);
	lcd.clear();
	if (light){
		LCDBrightness(LCDDark);	// Switch LCD Backlight very low
	}
	else{
		LCDBrightness(0);	// Switch LCD Backlight off
	}
	if (delay > 0){ //only start on positive delay
		long ms = millis();
		if (delay > 4000){
			while (millis() < ms + delay - 4000){  //delay with light on before start, countdown on LCD
				print("Starting in:", String((ms + delay - millis()) / 1000), " seconds");
			}
		}
		LCDBrightness(0);	//Switch LCD Backlight off
		int lastseconds = 3;
		while (millis() < ms + delay){ //delay finallight off, countdown on LCD
			print("Starting in:", String((ms + delay - millis()) / 1000), " seconds");
			if (beep){
				switch (lastseconds){
				case 3:
					if ((ms + delay - millis()) <3120)
					{
						tone(BuzzerPin, 440, 50);
						
						lastseconds = 2;
					}
					break;
				case 2:
					if ((ms + delay - millis()) <2120)
					{
						tone(BuzzerPin, 440, 50);
						lastseconds = 1;
					}
					break;
				case 1:
					if ((ms + delay - millis()) <1120)
					{
						tone(BuzzerPin, 440, 50);
						lastseconds = 0;
					}
					break;
				case 0:
					if ((ms + delay - millis()) < 120)
					{
						tone(BuzzerPin, 880, 100);
						lastseconds = -1;
					}
					break;
				default:
					break;
				}
			}
		}
	}
	lcd.clear();	//clear LCD
}

	void bmpDraw(char *filename) {
#define BUFFPIXEL 20

		File     bmpFile;
		int      bmpWidth, bmpHeight;   // W+H in pixels
		uint8_t  bmpDepth;              // Bit depth (currently must be 24)
		uint32_t bmpImageoffset;        // Start of image data in file
		uint32_t rowSize;               // Not always = bmpWidth; may have padding
		uint8_t  sdbuffer[3 * BUFFPIXEL]; // pixel buffer (R+G+B per pixel)
		uint8_t  buffidx = sizeof(sdbuffer); // Current position in sdbuffer
		boolean  goodBmp = false;       // Set to true on valid header parse
		boolean  flip = true;        // BMP is stored bottom-to-top
		int      w, h, row, col;
		uint8_t  r, g, b;
		uint32_t pos = 0, startTime = millis();

		
		Serial.println();
		Serial.print("Loading image '");
		Serial.print(filename);
		Serial.println('\'');

		// Open requested file on SD card
		if ((bmpFile = SD.open(filename)) == NULL) {
			Serial.print("File not found");
			return;
		}

		// Parse BMP header
		if (read16(bmpFile) == 0x4D42) { // BMP signature
			Serial.print("File size: "); Serial.println(read32(bmpFile));
			(void)read32(bmpFile); // Read & ignore creator bytes
			bmpImageoffset = read32(bmpFile); // Start of image data
			Serial.print("Image Offset: "); Serial.println(bmpImageoffset, DEC);
			// Read DIB header
			Serial.print("Header size: "); Serial.println(read32(bmpFile));
			bmpWidth = read32(bmpFile);
			bmpHeight = read32(bmpFile);
			if (read16(bmpFile) == 1) { // # planes -- must be '1'
				bmpDepth = read16(bmpFile); // bits per pixel
				Serial.print("Bit Depth: "); Serial.println(bmpDepth);
				if ((bmpDepth == 24) && (read32(bmpFile) == 0)) { // 0 = uncompressed

					goodBmp = true; // Supported BMP format -- proceed!
					Serial.print("Image size: ");
					Serial.print(bmpWidth);
					Serial.print('x');
					Serial.println(bmpHeight);

					// BMP rows are padded (if needed) to 4-byte boundary
					rowSize = (bmpWidth * 3 + 3) & ~3;

					// If bmpHeight is negative, image is in top-down order.
					// This is not canon but has been observed in the wild.
					if (bmpHeight < 0) {
						bmpHeight = -bmpHeight;
						flip = false;
					}
					h = bmpHeight;
					w = bmpWidth;
					noInterrupts();
					for (row = 0; row<h; row++) { // For each scanline...

						// Seek to start of scan line.  It might seem labor-
						// intensive to be doing this on every line, but this
						// method covers a lot of gritty details like cropping
						// and scanline padding.  Also, the seek only takes
						// place if the file position actually needs to change
						// (avoids a lot of cluster math in SD library).
						if (flip) // Bitmap is stored bottom-to-top order (normal BMP)
							pos = bmpImageoffset + (bmpHeight - 1 - row) * rowSize;
						else     // Bitmap is stored top-to-bottom
							pos = bmpImageoffset + row * rowSize;
						if (bmpFile.position() != pos) { // Need seek?
							bmpFile.seek(pos);
							buffidx = sizeof(sdbuffer); // Force buffer reload
						}

						for (col = 0; col<w; col++) { // For each pixel...
							// Time to read more pixel data?
							if (buffidx >= sizeof(sdbuffer)) { // Indeed
								bmpFile.read(sdbuffer, sizeof(sdbuffer));
								buffidx = 0; // Set index to beginning
							}

							// Convert pixel from BMP to TFT format, push to display
							b = sdbuffer[buffidx++];
							g = sdbuffer[buffidx++];
							r = sdbuffer[buffidx++];
							
							npx.setPixelColor(col, npx1.Color(r, g, b));

						} // end pixel
						npx.show();
						delayMicroseconds(frame_delay);
					} // end scanline
					interrupts();
					Serial.print("Processed in ");
					Serial.print(millis() - startTime);
					Serial.println(" ms");
				} // end goodBmp
			}
		}

		bmpFile.close();
		if (!goodBmp) Serial.println("BMP format not recognized.");
	}

	// These read 16- and 32-bit types from the SD card file.
	// BMP data is stored little-endian, Arduino is little-endian too.
	// May need to reverse subscript order if porting elsewhere.

	uint16_t read16(File & f) {
		uint16_t result;
		((uint8_t *)&result)[0] = f.read(); // LSB
		((uint8_t *)&result)[1] = f.read(); // MSB
		return result;
	}

	uint32_t read32(File & f) {
		uint32_t result;
		((uint8_t *)&result)[0] = f.read(); // LSB
		((uint8_t *)&result)[1] = f.read();
		((uint8_t *)&result)[2] = f.read();
		((uint8_t *)&result)[3] = f.read(); // MSB
		return result;
	}

	//Get Keypad values
	int ReadKeypad() {
		adc_key_in = analogRead(0);             // read the value from the sensor  
		digitalWrite(13, HIGH);
		key = get_key(adc_key_in);              // convert into key press

		if (key != oldkey) {                    // if keypress is detected
			delay(50);                            // wait for debounce time
			adc_key_in = analogRead(0);           // read the value from the sensor  
			key = get_key(adc_key_in);            // convert into key press
			if (key != oldkey) {
				oldkey = key;
				if (key >= 0){
					return key;
				}
			}
		}
		return key;
	}

	//Display a value to set on the screen
	//value(What to set, e.g. "Brightness", minimum value, maximum value, default value, unit postfix, size of steps to change)
	int value(String s, int minwert, int maxwert, int wert, String pt = "", int stepsize = 1){
		lcd.clear();
		print(s);
		bool done = false;
		long key_old_millis;
		while (!done){
			int key = ReadKeypad();
			//String pt2;
			if (key != key_old || (key == key_old && key_old_millis + keydelay <= millis() && (key == sw_right || key == sw_left))){
				//int stepsize2;
				if (key != key_old){
					key_old_millis = millis();
				}
				/*
				if (key_old_millis + (keydelay +5000) <= millis()){
					stepsize2 = stepsize * 10;
					pt2 = pt + " +";
				}
				if (key_old_millis + (keydelay + 10000) <= millis()){
					stepsize2 = stepsize * 100;
					pt2 = pt + " ++";
				}*/
				key_old = key;

				switch (key){
				case sw_none:
					key_old_millis = 0;
					key_old = sw_none;
					break;
				case sw_up:
					menupos--;
					done = true;
					break;
				case sw_down:
					menupos++;
					done = true;
					break;
				case sw_right:
					wert = wert + stepsize;
					if (wert > maxwert && maxwert > minwert){ wert = maxwert; }
					delay(20);
					break;
				case sw_left:
					wert = wert - stepsize;
					if (wert < minwert){ wert = minwert; }
					delay(20);
					break;
				case sw_select:
					menupos = 80; //if select is pressed, start the LED sequence
					done = true;
					break;
				}

				lcd.setCursor(0, 1);
				lcd.print(wert);
				lcd.print(pt);
				clear();

			}
		}
		return wert;
	}

	// Convert ADC value to key number
	int get_key(unsigned int input) {
		int k;
		for (k = 0; k < NUM_KEYS; k++) {
			if (input < adc_key_val[k]) {
				return k;
			}
		}
		if (k >= NUM_KEYS)
			k = -1;                               // No valid key pressed
		return k;
	}

	//File selection, uses stored files in array files[], user selects index
	void fileselect(){
		lcd.clear();
		print(menu[menupos]);
		bool done = false;
		while (!done){
			int key = ReadKeypad();
			lcd.setCursor(0, 1);
			lcd.print(files[selectedfile]);
			clear();

			if (key != key_old){
				key_old = key;
				switch (key){
				case sw_none:
					break;
				case sw_up:
					menupos--;
					done = true;
					break;
				case sw_down:
					menupos++;
					done = true;
					break;
				case sw_right:
					selectedfile++;
					if (selectedfile > filecount){ selectedfile = 1; }
					break;
				case sw_left:
					selectedfile--;
					if (selectedfile < 1){ selectedfile = filecount; }
					break;
				case sw_select:
					menupos = 80; //if "Select" is pressed, start the display
					done = true;
					break;
				}
				Serial.println(selectedfile);
				Serial.println("count");
				Serial.println(filecount);
			}
		}

	}



	void loop(){


		/*		"File select",		//menu 1
				"Brightness",		//menu 2
				"Init delay",		//menu 3
				"Frame delay",		//menu 4
				"Repeat",			//menu 5
				"Repeat delay" };	//menu 6*/
		switch (menupos)
		{
		case 1:
			npx.clear();
			LCDBrightness(LCDBright);
			fileselect();
			break;
		case 2:
			brightness = value(menu[menupos], 0, 100, brightness, "%",1);
			break;
		case 3:
			init_delay = value(menu[menupos], 0, 0, init_delay, "ms",10);
			break;
		case 4:
			frame_delay = value(menu[menupos], 0, 0, frame_delay, "us",1);
			break;
		case 5:
			repeat = value(menu[menupos], 0, 0, repeat);
			break;
		case 6:
			repeat_delay = value(menu[menupos], 0, 0, repeat_delay, "ms",10);
			break;
		case 7: //When this is reached, do the LED stuff
			menupos = 80;
			break;
		case 80:
			LEDBrightness(brightness); //set LED to brightness

			showdelay(init_delay, true, true); //Initial delay before sequence start

			for (int i = 0; i < repeat + 1; i++){
				char char_file[15];
				files[selectedfile].toCharArray(char_file,15);
				debug("Showing:");
				Serial.println(char_file);

				bmpDraw(char_file);

				if (repeat > 0) //beep for repeats when previous display ends
				{
					npx.clear();
					npx.show();
					tone(BuzzerPin, 440, 50);
					if (repeat_delay > 0) //beep for repeats before next display starts
					{
						delay(repeat_delay);
						tone(BuzzerPin, 440, 50);
					}
				}
			}
			tone(BuzzerPin, 880, 500); //beep when whoe sequence ends
			npx.clear();
			npx.show();
			menupos = 99; //go to loop with dark display
			break;
		case 99: //Loop with dark display until a button is pressed
			LCDBrightness(LCDDark);
			print("Finished...");
			key = ReadKeypad();
			while (key == -1){
				key = ReadKeypad();
			}
			oldkey = key_old = key;
			menupos = 1;
			break;
		default: //in case no or invalid menu position is set, go to file select
			menupos = 1;
			break;
		}
	}