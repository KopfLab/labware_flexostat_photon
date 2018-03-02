/*
 * Display.h
 * Created on: Feb 4, 2016
 * Author: Sebastian Kopf <seb.kopf@gmail.com>
 * Requires LiquidCrystal_I2C_Spark library from https://github.com/BulldogLowell/LiquidCrystal_I2C_Spark
 *
 * Convenience wrapper for LCD I2C display that allows easy displaying of temporary info messages that show up in a reserverd part of the display.
 *
 * Create Display object with the lcd i2c address, the size of the display (columns and rows) and, optionally the width of space (in the last 2 lines) reserved for the temporary messages.
 *
 * Most basic code example for a 20x4 display showing a 5s temporary message at the beginning of startup:
 *
 * Display lcd (0x27, 20, 4, 7);
 * void setup(void) { lcd.init(); lcd.show_message("temp", "message", 5000); }
 * void loop(void) { lcd.update(); }
 *
 */

#pragma once
#include "LiquidCrystal_I2C_Spark.h"

// requirement: https://github.com/BulldogLowell/LiquidCrystal_I2C_Spark
//#include "HttpClient.h" // script in particle dev (requires local copies of HttpClient.h/.cpp)
// #include "HttpClient/HttpClient.h" // script in particle cloud

// PIN definitions
#define LCDData			4 // I2C SDA, physicially this is A4 on Arduino Uno, 20 on Mega and D0 on photon
#define LCDClock		5 // I2C SCL, physically this is A5 on Arduino Uno, 21 on Mega and D1 on photon

// Display class handles displaying information
// for the peristaltic pumping system
class Display : public LiquidCrystal_I2C {
public:
	uint8_t cols, rows; // display layout
	const char space[1] = {' '};

	uint8_t info_width;			// width of info messages
	int info_msg_show_time;		// how long current info message is being shown for (in ms), 0 = no message shown
	long info_msg_show_start;	// when the last info message was started

	Display (uint8_t lcd_Addr, uint8_t lcd_cols, uint8_t lcd_rows) :
		   // addr, en,rw,rs,d4,d5,d6,d7,bl,blpol
		   LiquidCrystal_I2C(lcd_Addr, lcd_cols, lcd_rows), cols(lcd_cols), rows(lcd_rows),
			 info_width(0), info_msg_show_time(0), info_msg_show_start(0) {}

	Display (uint8_t lcd_Addr, uint8_t lcd_cols, uint8_t lcd_rows, uint8_t info_width) :
 		   // addr, en,rw,rs,d4,d5,d6,d7,bl,blpol
 		   LiquidCrystal_I2C(lcd_Addr, lcd_cols, lcd_rows), cols(lcd_cols), rows(lcd_rows),
 			 info_width(info_width), info_msg_show_time(0), info_msg_show_start(0) {}


	// convenience functions for writing text
	// clears the line (starting from the beginning of the text) and the writes the String over it
	// both line and start are indexed from 1
	void init ();
	void go_to_line (uint8_t line);
	void clear_line (uint8_t line);
	void clear_line (uint8_t line, uint8_t start);
	void clear_line (uint8_t line, uint8_t start, uint8_t end);
	void print_line (uint8_t line, const String &text);
	void print_line (uint8_t line, const String &text, uint8_t start);
	void print_line (uint8_t line, const String &text, uint8_t start, uint8_t end);
	void print_line_right (uint8_t line, const String &text);

	void clear_screen(); // clears everything except the messages
	void clear_screen(uint8_t start_line); // clears everything except the messages
	void show_message (const String &line1, const String &line2);
	void show_message (const String &line1, const String &line2, int info_msg_show_time);
	void clear_message ();
	void update();
};

void Display::init() {
	LiquidCrystal_I2C::init();
	backlight();
	clear();
}

void Display::go_to_line(uint8_t line) {
	setCursor(0, line - 1);
}

void Display::clear_line(uint8_t line) {
	clear_line(line, 1);
}

void Display::clear_line(uint8_t line, uint8_t start) {
	clear_line(line, start, cols);
}

void Display::clear_line(uint8_t line, uint8_t start, uint8_t end) {
	setCursor(start - 1, line - 1);
	for (int i = start - 1; i < end; i++) {
		print(space[0]);
	}
	setCursor(start - 1, line - 1);
}

void Display::print_line (uint8_t line, const String &text, uint8_t start, uint8_t end) {
	// overwrite existing text in the line
	clear_line(line, start, end);

	// truncate message if too long
	print(text.substring(0, end - start + 1));
}

void Display::print_line (uint8_t line, const String &text, uint8_t start) {
	if (line == rows - 1 || line == rows) {
		print_line(line, text, 1, cols - info_width);
	} else {
		print_line(line, text, 1, cols);
	}
}

void Display::print_line (uint8_t line, const String &text) {
	print_line(line, text, 1);
}

void Display::print_line_right (uint8_t line, const String &text) {
	print_line(line, text, cols - text.length() + 1, cols);
}

void Display::clear_screen() {
	clear_screen(1);
}

void Display::clear_screen(uint8_t start_line) {
	for (uint8_t i = start_line; i <= rows; i++) {
		print_line(i, "");
	}
}

void Display::show_message (const String &line1, const String &line2) {
	show_message(line1, line2, 0);
}

void Display::show_message (const String &line1, const String &line2, int show_time) {
	clear_message();
	info_msg_show_start = millis();
	info_msg_show_time = show_time;
	print_line_right(rows - 1, line1.substring(0, info_width));
	print_line_right(rows, line2.substring(0, info_width));
}

void Display::clear_message () {
	clear_line(rows - 1, cols - info_width + 1);
	clear_line(rows, cols - info_width + 1);
	info_msg_show_time = 0;
}

void Display::update() {
	if (info_msg_show_time > 0 && (millis() - info_msg_show_start) > info_msg_show_time) {
		clear_message();
	}
}
