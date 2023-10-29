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

#include <unistd.h>
#include <inttypes.h>
#include <stdio.h>
#include <math.h>

// Configuration commands
#define SDS011_MODE   0x02 // Set data reporting mode (3rd byte)
#define SDS011_QDATA  0x04 // Get data if in Query mode (3rd byte)
#define SDS011_DEVID  0x05 // Set device ID (3rd byte)
#define SDS011_SLEEP  0x06 // Set sleep and work (3rd byte)
#define SDS011_FWVER  0x07 // Get firmware version (3rd byte)
#define SDS011_PERIOD 0x08 // Set working period (3rd byte)

// message header & trailer
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

typedef struct
{
    uint8_t cmd_id;  // Command ID (SDS011_DATA or SDS011_CONF)
    uint8_t confcmd; // SDS011_DATA = 0;  SDS011_CONF : configuration command
    uint8_t type;    // 0=Query current mode, 1=Set mode
    uint8_t mode;    // set or query.
    uint8_t value;   // 0=Continous, 1-30 (mins) [work 30 sec and sleep n*60-30 sec]
    uint16_t devid;  // Device ID
    uint8_t year;    // Firmware Year
    uint8_t month;   // Firmware Month
    uint8_t day;     // Firmware Day
    float   pm25;    // PM 2.5 value
    float   pm10;    // PM 10 value
} sds011_response_t;

class SDS011
{
  public:
  
    SDS011(void);
    
    /**
     * @brief  Enable or disable the printing of sent/response HEX values.
     *
     * @param act : level of debug to set
     *  0 : no debug message
     *  1 : sending and receiving data
     */
    void EnableDebugging(uint8_t act);

    /**
     * @brief : first call to initiatize the library
     * 
     * @param fd: file descriptor of opened device
     * 
     * @return :
     *  SDS011_ERROR : could not send command
     *  SDS011_OK    : all good
     */
    int begin(int fd);
    
    /**
     * @brief : read firmware version
     * 
     @param data : return firmware data (3 bytes)
     *  fddata[0] = year;
     *  fddata[1] = month;
     *  fddata[2] = day;
     * 
     * @return :
     *  SDS011_ERROR : could not send command
     *  SDS011_OK    : all good
     */
    int Get_Firmware_Version(uint8_t *fwdata);
      
    /**
     * @brief : get current reporting mode
     *
     * @return :
     *  SDS011_ERROR : could not send command
     *  SDS011_OK    : all good
     */
    int Get_data_reporting_mode(uint8_t *p) {return(Get_Param(SDS011_MODE, p));}
       
    /**
     * @brief : set the data reporting mode
     *
     * @param p
     *  REPORT_QUERY = Sensor received query data command to report a measurement data.
     *  REPORT_STREAM = Sensor automatically reports a measurement data in a work period.
     *
     * @return :
     *  SDS011_ERROR : could not send command
     *  SDS011_OK    : all good
     */
    int Set_data_reporting_mode(uint8_t p) {return(Set_Param(SDS011_MODE, p));}

    /**
     * @brief : get current sleep/working mode
     *
     * @return :
     *  SDS011_ERROR : could not send command
     *  SDS011_OK    : all good
     */
    int Get_Sleep_Work_mode(uint8_t *p) {return(Get_Param(SDS011_SLEEP, p));}
    
    /**
     * @brief : set the sleep / work mode
     *
     * @param p
     * MODE_WORK = Sensor starts working. needs 30 seconds before good enough data
     * MODE_SLEEP = Sensor goes in sleep mode
     *
     * @return :
     *  SDS011_ERROR : could not send command
     *  SDS011_OK    : all good
     */    
    int Set_Sleep_Work_Mode(uint8_t p) { return(Set_Param(SDS011_SLEEP, p));}
    /**
     * @brief : get current working period
     *
     * @return :
     *  SDS011_ERROR : could not send command
     *  SDS011_OK    : all good
     */
    int Get_Working_Period(uint8_t *p) {return(Get_Param(SDS011_PERIOD, p));}
    
    /**
     * @brief : set the data reporting mode
     *
     * @param period :
     *  0      = continuus
     *  1 - 30 = set work every n minutes
     *
     * @return :
     *  SDS011_ERROR : could not send command
     *  SDS011_OK    : all good
     */
    int Set_Working_Period(uint8_t p) {return(Set_Param(SDS011_PERIOD, p));}

    /**
     * @brief : get current device ID
     */
    uint16_t Get_DevID();

    /**
     * @brief : set new device ID
     * 
     * @param newid : newid to set
     * 
     * @return :
     *  SDS011_ERROR : could not send command
     *  SDS011_OK    : all good
     */
    int Set_New_Devid(uint8_t *newid);
    
    /**
     * @brief :  set relative humidity correction
     *
     * @param h: relative humidity (like 30.5%) to enable or zero to disable
     *           correction. if zero it will disable the correction
     * 
     * @return :
     *  SDS011_ERROR : could not handle command
     *  SDS011_OK    : all good
     */
    int Set_Humidity_Cor(float h);
    
    /**
     * @brief : get data when in query mode
     *
     * @param PM25 : to store the measured PM2.5 value
     * @param PM10 : to store the measured PM10 value
     * 
     * @return :
     *  SDS011_ERROR : could not send command
     *  SDS011_OK    : all good
     */
    int Query_data(float *PM25, float *PM10) 
        {return(Report_Data(REPORT_QUERY, PM25, PM10));}

    /**
     * @brief : get data when in continuous mode
     *
     * @param PM25 : to store the measured PM2.5 value
     * @param PM10 : to store the measured PM10 value
     * 
     * @return :
     *  SDS011_ERROR : could not send command
     *  SDS011_OK    : all good
     */
    int Get_data(float *PM25, float *PM10)
        {return(Report_Data(REPORT_STREAM, PM25, PM10));}

  private:
    
    /**
     * @brief : Try to connect to device before executing requested commands
     *
     * @param fd: file descriptor of opened device
     *  *
     * @return :
     *  SDS011_ERROR : could not send command
     *  SDS011_OK    : all good
     */
    int Try_Connect(int fd);
    

    /**
     * @brief Process input from sensor.
     * 
     * @param packet Received packets from sensor.
     * @param length Number of received packets.
     * 
     * @return 
     *  SDS011_ERROR : could not send command
     *  SDS011_OK    : all good
     */
    uint8_t ProcessResponse( const uint8_t *packet, uint8_t length);
    
    /**
     * @brief Calculate checksum from bytes.
     * 
     * @param packet Packets to use for computation.
     * @param length Number of bytes to use.
     * 
     * @return Checksum of bytes.
     */
    uint8_t Calc_Checksum(const uint8_t *packet, uint8_t length);
    
    /**
     * @brief : initialise packet to be send.
     * @param data1 : the data1 byte to be included
     */
    void prepare_packet(uint8_t data1);
    
    /**
     * @brief : read response from sds011
     *
     * @return :
     *  SDS011_ERROR : could not send command
     *  SDS011_OK    : all good
     */
    int read_sds();
    
    /**
     * @brief : add CRC + send to SDS-011
     *
     * @return :
     *  SDS011_ERROR : could not send command
     *  SDS011_OK    : all good
     */
    int send_sds();
  
    /**
     * @brief : wait for response on conf request
     *
     * The SDS-011 gets lost when a next configuration request
     * is send, while not replied to earlier conf request. Especially when
     * in reporting/streaming mode, the first read-back response often
     * is still a data packet. The _PendingConfReq-flag will prevent sending
     * another configuration request if not received answer on previous yet
     *
     * @return :
     *  SDS011_ERROR : could not send command
     *  SDS011_OK    : all good
     */
    int Wait_For_answer();
    
    /**
     * @brief : get current parameter
     * 
     * @param c : parameter requested
     * SDS011_MODE  : current reporting mode
     * SDS011_SLEEP : current sleep working mode
     * SDS011_PERIOD: current working period
     * 
     * @param p : return value
     * SDS011_MODE
     *      REPORT_STREAM
     *      REPORT_QUERY
     * 
     * SDS011_SLEEP:    
     *      MODE_SLEEP
     *      MODE_WORK
     * 
     * SDS011_PERIOD
     *      0 continuous
     *      >0 < 30 minutes
     * 
     * @return :
     *  SDS011_ERROR : could not send command
     *  SDS011_OK    : all good
     */
    int Get_Param(uint8_t c, uint8_t *p);
   
    /**
     * @brief : set the data reporting mode
     *
     * @param mode : parameter to set
     * SDS011_MODE  : current reporting mode
     * SDS011_SLEEP : current sleep working mode
     * SDS011_PERIOD: current working period
     * 
     * @param p : value to set
     * SDS011_MODE
     *      REPORT_STREAM
     *      REPORT_QUERY
     * 
     * SDS011_SLEEP:    
     *      MODE_SLEEP
     *      MODE_WORK
     * 
     * SDS011_PERIOD
     *      0 continuous
     *      >0 < 30 minutes
     *
     * @return :
     *  SDS011_ERROR : could not send command
     *  SDS011_OK    : all good
     */
    int Set_Param (uint8_t mode, uint8_t p);
    
    /**
     * 
     * @brief : read data
     *
     * @param rmode : 
     *  REPORT_STREAM : read response from SDS011 (default)     
     *  REPORT_QUERY  : will sent an additional request for data
     *      
     * @param PM25 : to store the measured PM2.5 value
     * @param PM10 : to store the measured PM10 value
     *
     * @return :
     *  SDS011_ERROR : could not send command
     *  SDS011_OK    : all good
     */
    int Report_Data (uint8_t rmode, float *PM25, float *PM10);
    
};

#endif /* _SDS011_H */
