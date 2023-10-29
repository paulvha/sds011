/*
 * Copyright (c) 2019 Paulvha. version 1.0
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
 * 
 * version 2.0 paulvha, May 2019
 *  - changed better structure between user level and supporting library
 *  - changed to CPP file structure
 *  - enhanced debugging
 */

#include "sds011_lib.h"

uint8_t SDS011_Packet[SDS011_SENDPACKET_LEN];
bool    _PendingConfReq = false;   // indicate configuration request pending
uint8_t  _dev_id[2]= {0xff,0xff};  //  holds current device ID
float   _RelativeHumidity = 0;     // for humidity correction
int     _fd;                       // file description to use
bool    _sdsDebug = false;         // enable debug messages
sds011_response_t data;            // holds parsed received data

/********************************************************************
 * @brief : constructor and initialize variables
 ********************************************************************/
SDS011::SDS011(void)
{
    _fd = 0xff;
}
/********************************************************************
 * @brief : first call to initiatize the library
 * 
 * @param fd: file descriptor of opened device
 * 
 * @return :
 *  SDS011_ERROR : could not send command
 *  SDS011_OK    : all good
 ********************************************************************/
int SDS011::begin(int fd)
{
    return(Try_Connect(fd));
}

/**
 * @brief : Enable or disable the printing of sent/response HEX values.
 *
 * @param act : level of debug to set
 *  0 : no debug message
 *  1 : sending and receiving data
 */
void SDS011::EnableDebugging(uint8_t act)
{
    _sdsDebug = act;
}

/********************************************************************
 * @brief : calculate checksum
 *
 * @param packet : data to checksum
 * @param length : length of data to checksum
 *
 * @return calculated checksum
 ********************************************************************/
uint8_t SDS011::Calc_Checksum(const uint8_t *packet, uint8_t length)
{
    uint8_t checksum = 0, counter;

    for (counter=0; counter < length; counter++) {
        checksum += packet[counter];
    }

    return checksum;
}

/*********************************************************************
 * @brief : process packet received from SDS011
 *
 * @param packet : received packet from sds011
 * @param length : length of received package (SDS011_PACKET_LEN ?)
 *
 * @return :
 *  SDS011_ERROR : could not send command
 *  SDS011_OK    : all good
 *********************************************************************/
uint8_t SDS011::ProcessResponse(const uint8_t *packet, uint8_t length)
{
    int i; 
    
    if (_sdsDebug) {
        printf("Received: ");
        for (i=0 ; i < length; i++) printf("%02X ", packet[i]);
        printf("\n");
    }

    if (length != SDS011_PACKET_LEN || packet[0] != SDS011_BYTE_BEGIN || packet[length-1] != SDS011_BYTE_END)
        return (SDS011_ERROR);

    // check CRC
    if (packet[8] != Calc_Checksum(packet+2, 6)) return(SDS011_ERROR);

    // set device ID
    data.devid = (packet[7] << 8) + packet[6];

    // indicate configuration or data package
    data.cmd_id = packet[1];
    
    if (data.cmd_id == SDS011_DATA) {
        
        data.pm25 = (float)(((packet[3] * 256) + packet[2]) / 10.0);
        data.pm10 = (float)(((packet[5] * 256) + packet[4]) / 10.0);

        /* Humidity correction factor to apply (see detailed document) */
        if (_RelativeHumidity){
            data.pm25 = data.pm25 * 2.8 * pow((100 - _RelativeHumidity), -0.3745);
        }
        
        return(SDS011_OK);
    } 
    else if (data.cmd_id == SDS011_CONF) {

        data.confcmd = packet[2];

        switch (data.confcmd)
        {
            case SDS011_SLEEP:
            case SDS011_MODE:
            case SDS011_PERIOD:
                data.type = packet[3];
                data.mode = packet[4];

            case SDS011_DEVID:      // already handled 
                break;

            case SDS011_FWVER:
                data.year = packet[3];
                data.month = packet[4];
                data.day = packet[5];
                break;

            default: return(SDS011_ERROR);
        }
            
        // got response on configuration
        _PendingConfReq = false;
        
        return(SDS011_OK);
    } 

    return(SDS011_ERROR);
}

/*********************************************************************
 * @brief : wait for response on configuration request
 *
 * The SDS011 gets lost when a next configuration request is send, 
 * while not replied to earlier conf request. Especially when
 * in reporting/streaming mode, the first read-back response often
 * is still a data packet. The _PendingConfReq-flag will prevent sending
 * another configuration request if not received answer on previous yet
 *
 * @return :
 *  SDS011_ERROR : could not send command
 *  SDS011_OK    : all good
 *********************************************************************/
int SDS011::Wait_For_answer()
{
    int i = 0;      // prevent deadlock
     
    while (_PendingConfReq)
    {
        // max 20 times
        if(i++ > 20) return(SDS011_ERROR);

         // read & parse response from sds
        read_sds();
    }
    
    return(SDS011_OK);
}

/*********************************************************************
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
 *********************************************************************/
int SDS011::Get_Param(uint8_t c, uint8_t *p)
{
    prepare_packet(c);

    if (_sdsDebug) {
        
        if (c == SDS011_SLEEP) printf("\n\tget working mode\n");
        else if (c == SDS011_MODE) printf("\n\tget reporting mode\\n");
        else if (c == SDS011_PERIOD) printf("\n\tget working period\n");
        else printf("\n\tGet unknown parameter : %02x\n", c);
    }
    
    if (send_sds() == SDS011_ERROR) return (SDS011_ERROR);

    // read / display response from sds
    if (Wait_For_answer() == SDS011_ERROR) {
        if (_sdsDebug) printf("Error during sending\n");
        return (SDS011_ERROR);
    }

    *p = data.mode;
    
    return(SDS011_OK);
}

/*********************************************************************
 * @brief : initialise packet to be send.
 * @param data1 : the data1 byte to be included
 *********************************************************************/
void SDS011::prepare_packet(uint8_t data1)
{
    int i;
    for (i = 0; i < SDS011_SENDPACKET_LEN; i++) SDS011_Packet[i]=0x0;

    SDS011_Packet[0] = SDS011_BYTE_BEGIN;
    SDS011_Packet[1] = SDS011_BYTE_CMD;
    SDS011_Packet[2] = data1;
    SDS011_Packet[18] = SDS011_BYTE_END;
    
    // add device ID
    SDS011_Packet[15] = _dev_id[0];
    SDS011_Packet[16] = _dev_id[1];
}

/*********************************************************************
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
 *********************************************************************/
int SDS011::Set_Param (uint8_t mode, uint8_t p) 
{
    if (mode == SDS011_PERIOD) {
        
        if (p < 0 || p > 30) {
            
            if (_sdsDebug)  {
                printf("%d is invalid period, must be 0 to 30 minutes\n", p);
            }
            return(SDS011_ERROR);
        }
    }

    if (_sdsDebug) {
        if (mode == SDS011_SLEEP){
            printf("\n\tSet working mode to ");
            if (p == MODE_WORK) printf("Working\n");
            else if (p == MODE_SLEEP) printf("Sleeping\n");
            else printf("unknown\n");
        }
        else if (mode == SDS011_MODE) {
            printf("\n\tSet reporting mode to ");
            if (p == REPORT_QUERY) printf("Query\n");
            else if (p == REPORT_STREAM) printf("streaming\n");
            else printf("unknown\n");
        }
        else if (mode == SDS011_PERIOD) {
            printf("\n\tSet working period to %d\n", p);
        }
    }

    prepare_packet(mode);

    SDS011_Packet[3] = 1;       // SET mode
    SDS011_Packet[4] = p;       // set parameter

    if (send_sds() == SDS011_ERROR) {
        if (_sdsDebug) printf("Error during sending\n");
        return (SDS011_ERROR);
    }

    // read / display response from sds
    return(Wait_For_answer());
}

/*********************************************************************
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
 *********************************************************************/
int SDS011::Report_Data(uint8_t rmode, float *PM25, float *PM10)
{
   if(rmode == REPORT_QUERY) {
        
        prepare_packet(SDS011_QDATA);

        if (_sdsDebug) printf("\n\tQuery for data\n");

        if (send_sds() == SDS011_ERROR) return(SDS011_ERROR);
    }
    else
        if (_sdsDebug) printf("\n\tObtain data in continuous mode\n");
        
    // read / parse response from sds
    if (read_sds() == SDS011_ERROR) return(SDS011_ERROR);

    // return received data
    *PM25 = data.pm25;
    *PM10 = data.pm10;

    return(SDS011_OK);
}

/*********************************************************************
 * @brief : read firmware version
 * 
 * @param data :
 * fddata[0] = year;
 * fddata[1] = month;
 * fddata[2] = day;
 * 
 * @return :
 *  SDS011_ERROR : could not send command
 *  SDS011_OK    : all good
 *********************************************************************/
int SDS011::Get_Firmware_Version(uint8_t *fwdata)
{
    if (_sdsDebug) printf("\n\tRead Version information data\n");

    prepare_packet(SDS011_FWVER);

    if (send_sds() == SDS011_ERROR) return(SDS011_ERROR);

    // read / display response from sds
    if (Wait_For_answer() == SDS011_ERROR) return(SDS011_ERROR);

    fwdata[0] = data.year;
    fwdata[1] = data.month;
    fwdata[2] = data.day;

    return(SDS011_OK);
}

/*********************************************************************
 * @brief :  set relative humidity correction (in ProcesResponse())
 *
 * @param h: relative humidity (like 30.5%) to enable or zero to disable
 *           correction. if zero it will disable the correction
 * 
 * @return :
 *  SDS011_ERROR : could not send command
 *  SDS011_OK    : all good
 *********************************************************************/
int SDS011::Set_Humidity_Cor (float h)
{
   if (h < 0 || h > 100) return(SDS011_ERROR);
   _RelativeHumidity = h;
   return(SDS011_OK);
}

 /*********************************************************************
 * @brief : Try to connect to device before executing requested commands
 *
 * @param fd: file descriptor of opened device
 *
 * There is a known problem with flushing buffers on a serial USB that can
 * not be solved. The only thing one can try is to flush any buffers after
 * some delay as implemented in main().
 *
 * https://bugzilla.kernel.org/show_bug.cgi?id=5730
 * https://stackoverflow.com/questions/13013387/clearing-the-serial-ports-buffer
 *
 * Here we try to recover at start to get the version number and device ID.
 * This is actually a terrible way to handle... but there is no other option
 * on serial-USB.
 *
 * @return :
 *  SDS011_ERROR : could not send command
 *  SDS011_OK    : all good
 *********************************************************************/
int SDS011::Try_Connect(int fd)
{
    if (_sdsDebug) printf("\n\tTry to connect\n");

    _fd = fd;
    
    // try to read firmware
    prepare_packet(SDS011_FWVER);

    // sent packet. this is the first time so not connected
    if (send_sds() == SDS011_ERROR) return (SDS011_ERROR);

    int i = 0;              // prevent deadlock
    int j = 0;

    while (_PendingConfReq) // as long as no answer
    {
        usleep(10000);       // wait
        read_sds();          // check for answer

        if(i++ == 2 && _PendingConfReq) {
            i = 0;
            
            // resubmit
            _PendingConfReq = false;      // enable resend

            if (send_sds() == SDS011_ERROR || j++ > 10){
                 _fd = 0xff;              // No device connection.
                 return(SDS011_ERROR);
            }
        }
    }

    return(SDS011_OK);
}

/*********************************************************************
 * @brief : get current device ID
 *********************************************************************/
uint16_t SDS011::Get_DevID()
{
    return((_dev_id[1]<<8) + _dev_id[0]);
}

/*********************************************************************
 * @brief : set new device ID
 * @param newid : newid to set
 * 
 * @return :
 *  SDS011_ERROR : could not send command
 *  SDS011_OK    : all good
 *********************************************************************/
int SDS011::Set_New_Devid(uint8_t *newid)
{
       
    // has device been connected ?
    if (_fd == 0xff)    return(SDS011_ERROR);
    
    if (_sdsDebug) printf("\n\tSet new Device ID\n");

    // create command
    prepare_packet(SDS011_DEVID);
    SDS011_Packet[13] = newid[0];
    SDS011_Packet[14] = newid[1];

    // send it
    if (send_sds() == SDS011_ERROR)
    {
        if (_sdsDebug) printf("Error during sending\n");
        return (SDS011_ERROR);
    }

    return(Wait_For_answer());
}


/*********************************************************************
 * @brief : add the device_id + CRC + send to SDS-011
 *
 * @return :
 *  SDS011_ERROR : could not send command
 *  SDS011_OK    : all good
 *********************************************************************/
int SDS011::send_sds(){

    int i;

    // has device been connected ?
    if (_fd == 0xff)    return(SDS011_ERROR);
            
    // any pending configuration answer ?
    if (Wait_For_answer() == SDS011_ERROR) return(SDS011_ERROR);
    
    // add crc
    SDS011_Packet[17] = Calc_Checksum(SDS011_Packet+2, 15);

    if (_sdsDebug)
    {
        printf("Sending:  ");
        for (i=0 ; i < SDS011_SENDPACKET_LEN; i++) printf("%02X ",SDS011_Packet[i] & 0xff);
        printf("\n");
    }

    // send command
    if (write(_fd, SDS011_Packet, SDS011_SENDPACKET_LEN) != SDS011_SENDPACKET_LEN) return(SDS011_ERROR);

    // indicate pending config request (EXCEPT when requested data)
    if (SDS011_Packet[2] != SDS011_QDATA || SDS011_Packet[2] != SDS011_SLEEP)    _PendingConfReq = true;
    return(SDS011_OK);
}

/*********************************************************************
 * @brief : read response from sds
 *
 * @return :
 *  SDS011_ERROR : could not send command
 *  SDS011_OK    : all good
 *********************************************************************/
int SDS011::read_sds() {
    
    uint8_t buf[15];
    uint8_t retry = 5;      // try 5 times to read a valid response
    
    // has device been connected ?
    if (_fd == 0xff) return(SDS011_ERROR);
    
    while (retry--)
    {
        // read from device
        if (read(_fd, buf, SDS011_PACKET_LEN) == SDS011_PACKET_LEN )
            break;
  
        // if retries counted down
        if (retry == 0) return(SDS011_ERROR);
    }
    
    // parse response
    if (ProcessResponse(buf, SDS011_PACKET_LEN) == SDS011_ERROR) 
        return(SDS011_ERROR);

    // save latest device ID
    _dev_id[0] = data.devid & 0xff;
    _dev_id[1] = (data.devid >> 8) & 0xff;

    return(SDS011_OK);
}
