/*
 * Copyright (c) 2019 Paulvha.  version 1.0
 *
 * This program will set and get information from an SDS-011 sensor
 *
 * A starting point and still small part of this code is based on the work from
 * karl, found on:  github https://github.com/karlchen86/SDS011
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */



#ifndef _SDS011_H
#define _SDS011_H

#ifdef __cplusplus
extern "C" {
#endif

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <time.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <getopt.h>
#include <signal.h>
#include <stdbool.h>
#include <getopt.h>
#include <stdarg.h>
#include <math.h>

// Command IDs
#define SDS011_MODE   0x02 // Set data reporting mode (3rd byte)
#define SDS011_QDATA  0x04 // Get data if in Queary mode (3rd byte)
#define SDS011_DEVID  0x05 // Set device ID (3rd byte)
#define SDS011_SLEEP  0x06 // Set sleep and work (3rd byte)
#define SDS011_FWVER  0x07 // Get firmware version (3rd byte)
#define SDS011_PERIOD 0x08 // Set working period (3rd byte)
#define SDS011_BYTE_BEGIN 0xAA // 1st byte of message
#define SDS011_BYTE_END   0xAB // Last byte of message

//(µC/PC/ -> Sensor)
#define SDS011_BYTE_CMD   0xB4   // sending message (2nd byte)
#define SDS011_SENDPACKET_LEN 19 // Number of bytes per sending packet

//(Sensor -> µC/PC)
#define SDS011_DATA   0xC0       // Measured data (2nd byte)
#define SDS011_CONF   0xC5       // Configuration mode response (2nd byte)
#define SDS011_PACKET_LEN   10   // Number of bytes per response

// status
#define SDS011_OK    0x00
#define SDS011_ERROR 0xFF

// Set data reporting mode
#define REPORT_STREAM 0x00
#define REPORT_QUERY  0x01

// sleep / work mode
#define MODE_SLEEP    0x0
#define MODE_WORK     0x1

extern bool PrmDebug;

typedef struct
{
    uint8_t cmd_id; // Command ID
    uint8_t type;   // 0=Query current mode, 1=Set mode
    uint8_t mode;   //
    uint8_t value;  // 0=Continous, 1-30 (mins) [work 30 sec and sleep n*60-30 sec]
    uint16_t devid; // Device ID
    uint8_t year;   // Year
    uint8_t month;  // Month
    uint8_t day;    // Day
    float pm25;     // PM 2.5
    float pm10;     // PM 10
} sds011_response_t;

/**
 * @brief Process input from sensor.
 * @param packet Received packets from sensor.
 * @param length Number of received packets.
 * @param response_obj Buffer to store response struct.
 * @return SDS011_OK if ok, SDS011_ERROR on error.
 */
uint8_t sds011_process( const uint8_t *packet, uint8_t length, sds011_response_t *ret);
/**
 * @brief Calculate checksum from bytes.
 * @param packet Packets to use for computation.
 * @param length Number of bytes to use.
 * @return Checksum of bytes.
 */
uint8_t sds011_checksum(const uint8_t *packet, uint8_t length);
int Get_Firmware_Version();
int Set_data_reporting_mode(int rmode);
int Get_data_reporting_mode();
int Set_Sleep_Work_Mode(int rmode) ;
int Get_Sleep_Work_mode();
int Set_Working_Period( uint8_t period);
int Get_Working_Period();
int Set_New_Devid(uint8_t * newid);
int Query_data();

void prepare_packet(uint8_t data1);
int read_sds(int loop, char * ret);
int send_sds(uint8_t * packet);
void SetDataDisplay(bool instruct);
int Set_Humidity_Cor(float h);
int Try_Connect(int fd);
uint16_t Get_DevID();


/*=======================================================================
    to display in color
  -----------------------------------------------------------------------*/
void p_printf (int level, char *format, ...);

/*! color display enable */
#define RED     1
#define GREEN   2
#define YELLOW  3
#define BLUE    4
#define WHITE   5

#define REDSTR "\e[1;31m%s\e[00m"
#define GRNSTR "\e[1;92m%s\e[00m"
#define YLWSTR "\e[1;93m%s\e[00m"
#define BLUSTR "\e[1;34m%s\e[00m"

/*! set to disable color output */
extern bool NoColor;

#ifdef __cplusplus
}
#endif

#endif /* _SDS011_H */
