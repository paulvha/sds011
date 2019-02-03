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
 */

#include "sds011_lib.h"

uint8_t SDS011_Packet[SDS011_SENDPACKET_LEN];
bool    _PendingConfReq = false;    // indicate configuration request pending
bool    _TryConnect = false;        // when try to connect no display
uint8_t _dev_id[2]= {0xff,0xff};    // device ID
bool    _Display_Data = false;      // display data received
float   _RelativeHumidity = 0;      // for humidity correction
int     _fd;                        // file description to use

/********************************************************************
 * @brief : calculate checksum
 *
 * @param packet : data to checksum
 * @param length : length of data to checksum
 *
 * @return calculated checksum
 ********************************************************************/
uint8_t CalcChecksum(const uint8_t *packet, uint8_t length)
{
    uint8_t checksum = 0, counter;

    for (counter=0; counter<length; counter++) {
        checksum += packet[counter];
    }

    return checksum;
}

/*********************************************************************
 * @brief : process packet received
 *
 * @param packet : received packet from sds011
 * @param length : length of received package
 * @param ret    : structure to store the parsed information
 *
 * @return :
 *  SDS011_ERROR = reponse received
 *  SDS011_OK = all good
 *********************************************************************/
uint8_t ProcessResponse( const uint8_t *packet, uint8_t length, sds011_response_t *ret)
{
    int i;
    if (PrmDebug) {
        printf("Received: ");
        for (i=0 ; i < length; i++) printf("%02X ", packet[i]);
        printf("\n");
    }

    if (length != SDS011_PACKET_LEN || packet[0] != SDS011_BYTE_BEGIN || packet[length-1] != SDS011_BYTE_END)
        return SDS011_ERROR;

    // check CRC
    if (packet[8] != CalcChecksum(packet+2, 6))
        return SDS011_ERROR;

    // set device ID
    ret->devid = (packet[7] << 8) + packet[6];

    if (packet[1] == SDS011_DATA) {
        ret->cmd_id = SDS011_DATA;
        ret->pm25 = (float)(((packet[3] * 256) + packet[2]) / 10.0);
        ret->pm10 = (float)(((packet[5] * 256) + packet[4]) / 10.0);

        /* Humidity correction factor to apply (see detailed document) */
        if (_RelativeHumidity)
            ret->pm25 = ret->pm25 * 2.8 * pow((100 - _RelativeHumidity), -0.3745);

    } else if (packet[1] == SDS011_CONF) {

        ret->cmd_id = packet[2];

        switch (packet[2])
        {
            case SDS011_SLEEP:
            case SDS011_MODE:
                ret->type = packet[3];
                ret->mode = packet[4];

            case SDS011_DEVID:
                break;

            case SDS011_PERIOD:
                ret->type = packet[3];
                ret->value = packet[4];
                break;

            case SDS011_FWVER:
                ret->year = packet[3];
                ret->month = packet[4];
                ret->day = packet[5];
                break;

            default: return SDS011_ERROR;
        }
    } else {
        return SDS011_ERROR;
    }

    return SDS011_OK;
}
/********************************************************************
 * @brief : set to display data or NOT
 ********************************************************************/
void SetDataDisplay(bool instruct)
{
    _Display_Data = instruct;
}

/*********************************************************************
 * @brief : wait for response on conf request
 *
 * The SDS-011 gets lost when a next configuration request
 * is send, while not replied to earlier conf request. Especially when
 * in reporting/streaming mode, the first read-back response often
 * is still a data packet. The _PendingConfReq-flag will prevent sending
 * another configuration request if not received answer on previous yet
 *
 * @return :
 *  SDS011_ERROR = reponse received
 *  SDS011_OK = all good
 *********************************************************************/
int Wait_For_answer()
{
    int i = 0;      // prevent deadlock
    char buf[40];

    if (_PendingConfReq) {
        while (_PendingConfReq)
        {
            if(i++ > 20) return(SDS011_ERROR);
            //printf("i=%d\n",i);

             // read / parse response from sds
            read_sds(1,buf);
        }

        printf("%s", buf);
    }
    return(SDS011_OK);
}

/*********************************************************************
 * @brief : get current reporting mode
 *
 * @return :
 *  SDS011_ERROR = could not send command
 *  SDS011_OK = all good
 *********************************************************************/
int Get_data_reporting_mode()
{
    prepare_packet(SDS011_MODE);

    if (PrmDebug) p_printf(YELLOW,"\n\tGet reporting mode\n");

    if (send_sds(SDS011_Packet) == SDS011_ERROR) return (SDS011_ERROR);

    // read / display response from sds
    return(Wait_For_answer());
}

/*********************************************************************
 * @brief : initialise packet to be send.
 * @param data1 : the data1 byte to be included
 *
 * Data Packet(19bytes): Head+Command ID+Data(15bytes)+checksum+Tail
 *********************************************************************/
void prepare_packet(uint8_t data1)
{
    int i;
    for (i = 0; i < SDS011_SENDPACKET_LEN; i++) SDS011_Packet[i]=0x0;

    SDS011_Packet[0] = SDS011_BYTE_BEGIN;
    SDS011_Packet[1] = SDS011_BYTE_CMD;
    SDS011_Packet[2] = data1;
    SDS011_Packet[18] = SDS011_BYTE_END;
}

/*********************************************************************
 * @brief : set the data reporting mode
 *
 * @param rmode
 *  REPORT_QUERY = Sensor received query data command to report a measurement data.
 *  REPORT_STREAM = Sensor automatically reports a measurement data in a work period.
 *
 * @return :
 *  SDS011_ERROR = could not handle command
 *  SDS011_OK = all good
 *********************************************************************/
int Set_data_reporting_mode(int rmode) {

    if (rmode != REPORT_QUERY && rmode != REPORT_STREAM) return(SDS011_ERROR);

    if (PrmDebug) {
        if (rmode == REPORT_QUERY) p_printf(YELLOW,"\n\tset Query mode\n");
        else p_printf(YELLOW,"\n\tset reporting/streaming mode\n");
    }

    prepare_packet(SDS011_MODE);

    SDS011_Packet[3] = 1;
    SDS011_Packet[4] = rmode;

    if (send_sds(SDS011_Packet) == SDS011_ERROR) return (SDS011_ERROR);

    // read / display response from sds
    return(Wait_For_answer());
}

/*********************************************************************
 * @brief : get current sleep/working mode
 *
 * @return :
 *  SDS011_ERROR = could not handle command
 *  SDS011_OK = all good
 *********************************************************************/
int Get_Sleep_Work_mode()
{
    if (PrmDebug) p_printf(YELLOW,"\n\tGet Sleep/work mode\n");

    prepare_packet(SDS011_SLEEP);

    if (send_sds(SDS011_Packet) == SDS011_ERROR) return (SDS011_ERROR);

    // read / display response from sds
    return(Wait_For_answer());
}

/*********************************************************************
 * @brief : set the sleep / work mode
 *
 * @param rmode
 * MODE_WORK = Sensor starts working. needs 30 seconds before good enough data
 * MODE_SLEEP = Sensor goes in sleep mode
 *
 * @return :
 *  SDS011_ERROR = could not handle command
 *  SDS011_OK = all good
 *********************************************************************/
int Set_Sleep_Work_Mode(int rmode) {

    if (rmode != MODE_SLEEP && rmode != MODE_WORK) return(SDS011_ERROR);

    if (PrmDebug) {
        if (rmode == MODE_WORK) p_printf(YELLOW,"\n\tset working mode\n");
        else p_printf(YELLOW,"\n\tset sleeping mode\n");
    }

    prepare_packet(SDS011_SLEEP);
    SDS011_Packet[3] = 1;
    SDS011_Packet[4] = rmode;

    if (send_sds(SDS011_Packet) == SDS011_ERROR) return (SDS011_ERROR);

    // read / display response from sds
    return(Wait_For_answer());
}

/*********************************************************************
 * @brief : get current working period
 *
 * @return :
 *  SDS011_ERROR = could not send command
 *  SDS011_OK = all good
 *********************************************************************/
int Get_Working_Period()
{
    if (PrmDebug) p_printf(YELLOW,"\n\tGet working period\n");

    prepare_packet(SDS011_PERIOD);

    if (send_sds(SDS011_Packet) == SDS011_ERROR) return (SDS011_ERROR);

    // read / display response from sds
    return(Wait_For_answer());
}

/*********************************************************************
 * @brief : set the data reporting mode
 *
 * @param period :
 *  0      = continuus
 *  1 - 30 = set work every n minutes
 *
 * @return :
 *  SDS011_ERROR = could not handle command
 *  SDS011_OK = all good
 *********************************************************************/
int Set_Working_Period(uint8_t period) {

    if (period < 0 || period > 30) {
        p_printf(RED," %d is invalid period, must be 0 to 30 minutes\n", period);
        return(SDS011_ERROR);
    }

    if (PrmDebug) {
        if (period) p_printf(YELLOW,"\n\tset work every %d minutes\n", period);
        else p_printf(YELLOW,"\n\tset continuous mode\n");
    }

    prepare_packet(SDS011_PERIOD);
    SDS011_Packet[3] = 1;           // set
    SDS011_Packet[4] = period;

    if (send_sds(SDS011_Packet) == SDS011_ERROR) return (SDS011_ERROR);

    // read / display response from sds
    return(Wait_For_answer());
}

/*********************************************************************
 * @brief : get data when in query mode
 *
 * @param loop  : the amount of time to perform a read (0 = endless)
 * @param delay : seconds between reads
 *
 * @return :
 *  SDS011_ERROR = could not send command
 *  SDS011_OK = all good
 *********************************************************************/
int Query_data(int loop, int delay)
{
    char buf[40];
    bool save_gdata = _Display_Data;
    int count = loop;
    if (count == 0) count = 1;

    _Display_Data = true;   // enable data output

    prepare_packet(SDS011_QDATA);

    while (count)
    {
        if (PrmDebug) p_printf(YELLOW,"\n\tQuery for data\n");

        if (send_sds(SDS011_Packet) == SDS011_ERROR) goto q_error;

        // read / parse response from sds
        if (read_sds(1, buf) == SDS011_ERROR) goto q_error;

        printf("%s", buf);

        // if not endless
        if (loop) count--;

        // if not finished
        if (count) sleep(delay);
    }

    _Display_Data = save_gdata;
    return(SDS011_OK);

q_error:
    _Display_Data = save_gdata;
    return (SDS011_ERROR);
}

/*********************************************************************
 * @brief : read firmware version
 *
 * @return :
 *  SDS011_ERROR = could not handle command
 *  SDS011_OK = all good
 *********************************************************************/
int Get_Firmware_Version()
{
    if (PrmDebug) p_printf(YELLOW,"\n\tRead Version information\n");

    prepare_packet(SDS011_FWVER);

    if (send_sds(SDS011_Packet) == SDS011_ERROR) return (SDS011_ERROR);

    // read / display response from sds
    return(Wait_For_answer());
}

/*********************************************************************
 * @brief :  set relative humidity correction (in ProcesResponse())
 *
 * @param h: relative humidity (like 30.5%) to enable or zero to disable
 *           correction
 * @return :
 *  SDS011_ERROR = could not handle command
 *  SDS011_OK = all good
 *********************************************************************/
int Set_Humidity_Cor (float h)
{
   if (h < 1 || h > 100) return(SDS011_ERROR);
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
 *  SDS011_ERROR = reponse received
 *  SDS011_OK = all good
 *********************************************************************/
int Try_Connect(int fd)
{
    if (PrmDebug) p_printf(GREEN,"\n\tTry to connect\n");

    _fd = fd;
    prepare_packet(SDS011_FWVER);

    if (send_sds(SDS011_Packet) == SDS011_ERROR) return (SDS011_ERROR);

    _TryConnect = true;      // prevent display results
    int i = 0;              // prevent deadlock
    int j = 0;

    while (_PendingConfReq)  // as long as no answer
    {
        usleep(10000);      // wait
        read_sds(1, NULL);  // check for answer

        if(i++ == 2 && _PendingConfReq) {
            i = 0;
            // resubmit
            _PendingConfReq = false;      // enable resend

            if (send_sds(SDS011_Packet) == SDS011_ERROR || j++ > 10){
                 _TryConnect = false;     // enable display results
                 return(SDS011_ERROR);
            }
        }
    }

    _TryConnect = false;     // enable display results
    return(SDS011_OK);
}

/*********************************************************************
 * @brief : get current device ID
 *********************************************************************/
uint16_t Get_DevID()
{
    return((_dev_id[1]<<8) + _dev_id[0]);
}

/*********************************************************************
 * @brief : set new device ID
 *
 * @return :
 *  SDS011_ERROR = could not handle command
 *  SDS011_OK = all good
 *********************************************************************/
int Set_New_Devid(uint8_t *newid)
{
    if (PrmDebug) p_printf(YELLOW,"\n\tSet new Device ID\n");

    // create command
    prepare_packet(SDS011_DEVID);
    SDS011_Packet[13] = newid[0];
    SDS011_Packet[14] = newid[1];

    // send it
    if (send_sds(SDS011_Packet) == SDS011_ERROR)
    {
        p_printf(RED, (char *)"Error during sending\n");
        return (SDS011_ERROR);
    }

    return(Wait_For_answer());
}

/*********************************************************************
 * @brief: add the device_id + CRC + send to SDS-011
 *
 * @param packet : packet to send
 *
 * @return :
 *  SDS011_ERROR = could not send command
 *  SDS011_OK = all good
 *********************************************************************/
int send_sds(uint8_t * packet) {

    int i;
    // any pending configuration answer ?
    if (Wait_For_answer() == SDS011_ERROR) return(SDS011_ERROR);

    // add device ID
    packet[15] = _dev_id[0];
    packet[16] = _dev_id[1];

    // add crc
    packet[17] = CalcChecksum(packet+2, 15);

    if (PrmDebug)
    {
        p_printf(BLUE,"Sending:  ");
        for (i=0 ; i < SDS011_SENDPACKET_LEN; i++) printf("%02X ",packet[i] & 0xff);
        printf("\n");
    }

    // send command
    if (write(_fd, packet, SDS011_SENDPACKET_LEN) != SDS011_SENDPACKET_LEN) return(SDS011_ERROR);

    // indicate pending config request (except when requested data)
    if (packet[2] != SDS011_QDATA)    _PendingConfReq = true;

    return(SDS011_OK);
}

/*********************************************************************
 * @brief : read / create display response from sds
 *
 * @param loop : number of read retry ( 0 = endless)
 * @param ret1 : return info to display
 *
 * in case ret1 is NULL OR loop is zero : 1ocal buffer will be used
 * in case loop is not one (1) buffer will be displayed
 *
 * @return :
 *  SDS011_ERROR = could not handle received message
 *  SDS011_OK = all good
 *********************************************************************/
int read_sds(int loop, char *ret1) {

    uint8_t buf[20];
    int num = 0;
    int count = loop;
    sds011_response_t data;
    char *ret, l_ret[40];

    // if endless loop use local buffer for display data
    if (count == 0) {
        ret = &l_ret[0];
        count = 1;
    }
    else {
        // if no buffer provided use local buffer
        if (ret1 == NULL) ret = &l_ret[0];

        // use provided buffer
        else ret = ret1;
    }

    while (count) {
        num = read(_fd, buf, sizeof(buf));

        if (ProcessResponse(buf, num, &data) == SDS011_OK) {

            switch (data.cmd_id)
            {
                case SDS011_DATA: {

                    // if not a pending answer on a configuration request
                    // AND expecting data to be displayed
                    if ( ! _PendingConfReq && _Display_Data)
                        sprintf(ret,"PM2.5: %.2f, PM10: %.2f\n", data.pm25, data.pm10);

                    goto add_dev;
                    break;
                }
                case SDS011_MODE: {

                    if (data.type) sprintf(ret,"Type: set mode: ");
                    else sprintf(ret,"Type: get mode: ");

                    if (data.mode) sprintf(ret,"%squery\n",ret);
                    else sprintf(ret,"%sReport / streaming\n",ret);

                    goto res_pending;
                    break;
                }
                case SDS011_DEVID: {

                    sprintf(ret,"New DeviceID: 0x%4x\n", data.devid);

                    goto res_pending;
                    break;
                }
                case SDS011_SLEEP: {

                    if (data.type) sprintf(ret,"Type: set mode: ");
                    else sprintf(ret,"Type: get mode: ");

                    if (data.mode) sprintf(ret,"%sWork\n",ret);
                    else sprintf(ret,"%sSleep\n",ret);

                    goto res_pending;
                    break;
                }
                case SDS011_PERIOD: {

                    if (data.type) sprintf(ret,"Type: set period: ");
                    else sprintf(ret,"Type: get period: ");

                    if (data.value) sprintf(ret,"%s%" PRIu8 " minute(s)\n",ret, data.value);
                    else sprintf(ret,"%scontinuous\n", ret);

                    goto res_pending;
                    break;
                }
                case SDS011_FWVER: {
                    // if not trying to connect
                    if (_TryConnect == false)
                      sprintf(ret,"Date: %" PRIu8 "-%" PRIu8 "-%" PRIu8 "\n", data.year, data.month, data.day);

                    goto res_pending;
                    break;
                }
                default: {
                    sprintf(ret,"Unknown data type\n");
                    return(SDS011_ERROR);
                }
            }
res_pending:
        _PendingConfReq = false;
add_dev:
        _dev_id[0] = data.devid & 0xff;
        _dev_id[1] = (data.devid >> 8) & 0xff;
        }

        // if not endless loop
        if (loop) count--;

        // print results
        if (loop != 1) printf("%s", ret);
    }

    return(SDS011_OK);
}
