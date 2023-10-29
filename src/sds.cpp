/*
 * Copyright (c) 2018 Paulvha.      version 1.0
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
 * Version 2.1 paulvha Ocobter 2023
 *  - fixed issue with wakeup after setting to sleep
 * 
 * version 2.0 paulvha, May 2019
 *  - changed better structure between user level and supporting library
 *  - changed to CPP file structure
 *  - enhanced debugging
 * 
 */

#include "sds011_lib.h"
#include <fcntl.h>
#include <string.h>
#include <termios.h>
#include <stdlib.h>
#include <signal.h>
#include <stdbool.h>
#include <getopt.h>
#include <stdarg.h>

#define PROGVERSION "2.1 / October 2023 / paulvha"

/* indicate these serial calls are C-programs and not to be linked */
extern "C" {
    void configure_interface(int fd, int speed);
    void set_blocking(int fd, int should_block);
    void restore_ser(int fd);
}

// global variables
int  fd = 0xff;                   // file pointer
char progname[20];
char port[20] = "/dev/ttyUSB0";

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
bool NoColor = false;            // no color output

// command line options
typedef struct settings
{
    bool        g_firmware;       // display firmware
    bool        g_devid;          // display device id
    bool        g_working_mode;   // display working mode (sleep/working)
    bool        g_working_period; // display working period (continuous / interval)
    bool        g_reporting_mode; // display reporting mode (reporting / query)

    bool        g_data;           // display data: true continuous, false : query
    uint16_t    loop;             // how many loop or reading
    uint16_t    delay;            // delay between reading data

    bool        s_devid;          // change devid
    uint8_t     newid[2];         // hold new device id
    uint8_t     s_working_mode;   // set working mode
    uint8_t     s_working_period; // set working period
} settings ;

// global structure
struct settings action;

/* global constructor SDS011 */ 
SDS011 MySensor;

/*********************************************************************
*  @brief close program correctly
*  @param val : exit value
**********************************************************************/
void closeout(int val)
{
    if (fd != 0xff) {
        
        // restore serial/USB to orginal setting
        restore_ser(fd);

        close(fd);
    }

    exit(val);
}

/*********************************************************************
 * @brief Display in color
 * @param format : Message to display and optional arguments
 *                 same as printf
 * @param level :  1 = RED, 2 = GREEN, 3 = YELLOW 4 = BLUE 5 = WHITE
 *
 * if NoColor was set, output is always WHITE.
 *********************************************************************/
void p_printf(int level, char *format, ...) {

    char    *col;
    int     coll=level;
    va_list arg;

    //allocate memory
    col = (char *) malloc(strlen(format) + 20);

    if (NoColor) coll = WHITE;

    switch(coll)
    {
        case RED:
            sprintf(col,REDSTR, format);
            break;
        case GREEN:
            sprintf(col,GRNSTR, format);
            break;
        case YELLOW:
            sprintf(col,YLWSTR, format);
            break;
        case BLUE:
            sprintf(col,BLUSTR, format);
            break;
        default:
            sprintf(col,"%s",format);
    }

    va_start (arg, format);
    vfprintf (stdout, col, arg);
    va_end (arg);

    fflush(stdout);

    // release memory
    free(col);
}

/*********************************************************************
 * @brief : catch signals to close out correctly
 * @param sig_num : signal raised to program
 *********************************************************************/
void signal_handler(int sig_num)
{
    switch(sig_num)
    {
        case SIGINT:
        case SIGKILL:
        case SIGABRT:
        case SIGTERM:
        default:
            p_printf(YELLOW, (char *) "\nStopping SDS-011 monitor\n");
            closeout(EXIT_SUCCESS);
            break;
    }
}

/*********************************************************************
 * @brief : setup signals
 *********************************************************************/
void set_signals()
{
    struct sigaction act;

    memset(&act, 0x0,sizeof(act));
    act.sa_handler = &signal_handler;
    sigemptyset(&act.sa_mask);

    sigaction(SIGTERM,&act, NULL);
    sigaction(SIGINT,&act, NULL);
    sigaction(SIGABRT,&act, NULL);
    sigaction(SIGSEGV,&act, NULL);
    sigaction(SIGKILL,&act, NULL);
}

/*********************************************************************
 * @brief : initialise variables before start
 *********************************************************************/
void init_variables()
{
    action.g_firmware = false;       // display firmware
    action.g_devid = false;          // display device id
    action.g_working_mode=false;     // display working mode (sleep/working)
    action.g_working_period = false; // display working period (continuous or interval)
    action.g_reporting_mode = false; // display reporting mode (reporting/ query)

    action.g_data = true;            // use continuous data
    action.loop  = 10;                // how many read loops 
    action.delay = 5;                 // delay between query read data

    action.s_devid = false;          // change devid

    action.s_working_mode = 0xff;     // set working mode (sleep/work)
    action.s_working_period = 0xff;   // set working period ( 0 - 30 min)
}

/*********************************************************************
 * @brief : usage information
 *********************************************************************/
void usage ()
{
    p_printf(YELLOW, (char *)

    "%s [options]  (version %s)\n"

    "\nSDS-011 Display Options: \n\n"

    "-m             get current working mode\n"
    "-p             get current working period\n"
    "-r             get current reporting mode\n"
    "-d             get Device ID\n"
    "-f             get firmware version\n"
    "-q             use query reporting mode       (default : continuous)\n"

    "\nSDS-011 setting: \n\n"

    "-M [ S / W  ]  Set working mode (sleep or work)\n"
    "-P [ 0 - 30 ]  Set working period (minutes)\n"
    "-D [ 0xaabb ]  Set new device ID\n"

    "\nProgram setting: \n\n"
    "-l x           loop x times (0 = endless)   (default : %d loops)\n"
    "-w x           x seconds between query data (default : %d seconds)\n"
    "-H #           set correction for humidity  (e.g. 33.5 for 33.5%)\n"
    "-u device      set new device-port          (default : %s)\n"
    "-b             set no color output          (default : color)\n"
    "-h             show help info\n"
    "-v             set verbose / debug info     (default : NOT set\n",
     progname, PROGVERSION, action.loop, action.delay,port);
}

/**
 * read the PM values either in query or continuous mode
 */

void read_PM()
{
    float pm25, pm10;
    int loopcount = action.loop;
    uint8_t rmode = REPORT_QUERY;
  
    if (action.g_data) {
        rmode = REPORT_STREAM;
        p_printf(GREEN, (char *) "Continuously capturing data\n");
    }
    else
        p_printf(GREEN, (char *) "Query for data with an %d seconds interval\n", action.delay);

    if (MySensor.Set_data_reporting_mode(rmode) == SDS011_ERROR) {
        p_printf(RED, (char *)"error during setting reading mode\n");
        closeout(EXIT_FAILURE);
    }
    
    // if endless
    if (loopcount == 0) loopcount = 1; 
    
    while (loopcount)
    {
        if (action.g_data){
            
            // continuous mode
            if (MySensor.Get_data(&pm25, &pm10) == SDS011_ERROR) {
                p_printf(RED, (char *)"error during reading data\n");
                closeout(EXIT_FAILURE);
            }
        }
        else {
            
            /* query data */
            if (MySensor.Query_data(&pm25, &pm10) == SDS011_ERROR) {
                p_printf(RED, (char *)"error during query data\n");
                closeout(EXIT_FAILURE);
            }
        }

        printf("PM 2.5 %f, PM10 %f\n", pm25, pm10);
        
        // if not endless loop
        if (action.loop != 0)  loopcount--;
   
        if (loopcount) {
            
            // wait in between reading during query (if set)
            if (action.delay && action.g_data == false) sleep(action.delay);
        }
    }
    
    printf("Number of requested loops reached\n");
}

/*********************************************************************
 * @brief Parse parameter input (either commandline or file)
 *
 * @param opt : option character detected
 * @param option : pointer to potential option value
 *
 *********************************************************************/
void parse_cmdline(int opt, char *option)
{
    char *p = option;
    uint8_t i = 0;
    char buf[4];

    switch (opt) {

    case 'b':   // set NO color output
        NoColor = true;
        break;

    case 'd':   // get device-id
        action.g_devid = true;
        break;

    case 'f':   // get firmware
        action.g_firmware = true;
        break;

    case 'm':   // get current working mode  (sleep/work)
        action.g_working_mode = true;
        break;

    case 'p':   // get working period
        action.g_working_period = true;
        break;

    case 'r':   // get current reporting mode (report / query)
        action.g_reporting_mode = true;
        break;

    case 'u':   // Set new device
        strncpy(port,option,sizeof(port));
        break;

    case 'v':   // set debug output
        MySensor.EnableDebugging(1);
        break;

    case 'q':   // Set query reporting mode
        action.g_data = false;
        break;
    
    case 'l':   // set loop count
        i = 0;
        while (*p != 0x0){
        
             buf[i++] = *p++;
        
             if (i > sizeof(buf) - 1) {
                p_printf(RED, (char*) "Loop amount too long %s\n", option);
                exit(EXIT_FAILURE);
             }
        }
        
        buf[i] = 0x0;
        action.loop = (uint8_t) strtod(buf, NULL);

        break;
    
    case 'w':   // delay between data reading
        i = 0;
        while (*p != 0x0){

             buf[i++] = *p++;

             if (i > sizeof(buf) -1) {
                p_printf(RED, (char*) "Delay amount too long %s\n", option);
                exit(EXIT_FAILURE);
             }
        }

        buf[i] = 0x0;
        action.delay = (uint8_t) strtod(buf, NULL);
        
        if (action.delay < 3)
        {
            p_printf(RED, (char*) "Delay of %d is less than 3 seconds\n", action.delay);
            exit(EXIT_FAILURE);
        }           

        break;

    case 'D':   // Set new device ID (0xaabb)
        if (strlen(p) == 6) {
            if (*p++ == '0'){
                if (*p++ == 'x'){
                    buf[0] = *p++;
                    buf[1] = *p++;
                    buf[2]  = 0x0;
                    action.newid[1] = (uint8_t)strtol(buf, NULL, 16);

                    buf[0] = *p++;
                    buf[1] = *p++;
                    action.newid[0] = (uint8_t)strtol(buf, NULL, 16);

                    action.s_devid = true;
                    action.g_devid = true;
                    break;
                }
            }
        }
        p_printf(RED,(char *) "Invalid Device Id %s\n", p);
        exit(EXIT_FAILURE);
        break;

    case 'M':   // Set working  mode devID needs to be added
        if (*option == 's' || *option == 'S') action.s_working_mode = MODE_SLEEP;
        else if (*option == 'w'|| *option == 'W') action.s_working_mode = MODE_WORK;
        else {
            p_printf(RED,(char *) "invalid working mode %s [ s or w ]\n", option);
            exit(EXIT_FAILURE);
        }
        break;

    case 'P':   // set working period
        action.s_working_period  = (uint8_t)strtod(option, NULL);

        if (action.s_working_period  < 0 || action.s_working_period  > 30){
            p_printf(RED,(char *)"invalid working period %d minutes. [ 0 - 30 ]\n", action.s_working_period );
            exit(EXIT_FAILURE);
        }

        break;

    case 'H':   // set relative humidity correction
        if (MySensor.Set_Humidity_Cor(strtod(option, NULL)))
        {
            p_printf(RED,(char *) "Invalid Humidity : %s [1 - 100%]\n",option );
            exit(EXIT_FAILURE);
        }
        break;

    default:    /* '?' */
        usage();
        exit(EXIT_FAILURE);
    }
}

/*********************************************************************
 * @brief : execute requested action(s)
 *
 * @return :
 *  SDS011_ERROR = reponse received
 *  SDS011_OK = all good
 *********************************************************************/
void main_action()
{
    uint8_t data[3];
    
    if (action.g_firmware){
        
        /* get firmware version */
        if (MySensor.Get_Firmware_Version(data) == SDS011_ERROR) {
            p_printf(RED,(char *)"error during reading firmware\n");
            closeout(EXIT_FAILURE);
        }
        
        printf("Firmware date (Y-M-D): %d-%d-%d\n", data[0], data[1], data[2]);
    }

    /* the device ID is captured during begin() */
    if (action.g_devid){
        printf("Current DeviceID: 0x%04x\n", MySensor.Get_DevID());
    }

    if (action.s_devid){
        
        /* set new devID */
        if (MySensor.Set_New_Devid(action.newid) == SDS011_ERROR){
            p_printf(RED,(char *) "error during setting new Device ID\n");
            closeout(EXIT_FAILURE);
        }
        printf("New DeviceID: 0x%04x\n", MySensor.Get_DevID());
    }

    if (action.g_reporting_mode){
        /* get current reporting mode */
        if (MySensor.Get_data_reporting_mode(data) == SDS011_ERROR) {
            p_printf(RED,(char *)"error during getting reporting mode\n");
            closeout(EXIT_FAILURE);
        }
        
        if (data[0] == REPORT_STREAM)
            printf("Currently in streaming mode\n");
        else
            printf("Currently in Query mode\n");
    }

    if (action.g_working_mode){
        /* current sleep/working mode */
        if (MySensor.Get_Sleep_Work_mode(data) == SDS011_ERROR) {
            p_printf(RED,(char *) "error during getting sleep/working mode\n");
            closeout(EXIT_FAILURE);
        }
        
        if (data[0] == MODE_SLEEP)
            printf("Currently in sleeping mode\n");
        else
            printf("Currently in Working mode\n");
    }

    if (action.g_working_period){
        /* current current working period */
        if (MySensor.Get_Working_Period(data) == SDS011_ERROR) {
            p_printf(RED,(char *) "error during getting current working period\n");
            closeout(EXIT_FAILURE);
        }
        
        if (data[0] == 0)
            printf("Working period in continuous mode");
        else
            printf("Working period every %d minutes\n", data[0]);
    }

    if (action.s_working_mode != 0xff){
        /* set working mode */
        if (MySensor.Set_Sleep_Work_Mode(action.s_working_mode) == SDS011_ERROR) {
            p_printf(RED,(char *)"error during setting sleeping mode\n");
            closeout(EXIT_FAILURE);
        }
        
        if (action.s_working_mode == MODE_WORK){ // added V 2.1
            p_printf(YELLOW, (char*)"wait 30 seconds to stabalize working mode\n");
    
            sleep(30);
            tcflush(fd,TCIOFLUSH);  // remove any received package during wait

        }
        else     // added V 2.1
        {
            p_printf(YELLOW, (char *)"Set to sleep\n");
            closeout(EXIT_SUCCESS); // do not start reading after setting to sleep
        }
           
    }

    if (action.s_working_period != 0xff){
        /* set working period to every x minutes
         * 0 = set working period to continuous
         * needs 30 seconds before the first result will show*/

        if (MySensor.Set_Working_Period(action.s_working_period ) == SDS011_ERROR) {
            p_printf(RED,(char *) "error during setting working period\n");
            closeout(EXIT_FAILURE);
        }
    }
    
    // check for reading PM values
    read_PM();
}

/*********************************************************************
 * @brief : main program start
 *********************************************************************/
int main(int argc, char *argv[])
{
    int opt;

    if (geteuid() != 0)  {
        p_printf(RED,(char *)"You must be super user\n");
        exit(EXIT_FAILURE);
    }

    /* save name for (potential) usage display */
    strncpy(progname,argv[0],20);
    
    init_variables();

    /* parse commandline */
    while ((opt = getopt(argc, argv, "H:hbmprdfvM:P:D:u:ql:w:")) != -1)
       parse_cmdline(opt, optarg);

    /* set signals */
    set_signals();

    /* you need the driver to be loaded before opening /dev/ttyUSBx
     * otherwise it will hang. The SDS-011 has an HL-341 chip, checked
     * with lsusb. The name of the driver is ch341.
     * One can change the system setup so this is done automatically
     * when reboot and then you can remove the call below. */
    system("modprobe usbserial");
    system("modprobe ch341");

    fd = open(port, O_RDWR | O_NOCTTY | O_SYNC);

    if (fd < 0) {
        p_printf(RED, (char *) "could not open %s\n", port);
        exit(EXIT_FAILURE);
    }

    configure_interface(fd, B9600);
    set_blocking(fd, 0);

   /* There is a problem with flushing buffers on a serial USB that can
    * not be solved. The only thing one can try is to flush any buffers
    * after some delay:
    *
    * https://bugzilla.kernel.org/show_bug.cgi?id=5730
    * https://stackoverflow.com/questions/13013387/clearing-the-serial-ports-buffer
    */
    usleep(10000);                      // required to make flush work, for some reason
    tcflush(fd,TCIOFLUSH);

    p_printf(YELLOW, (char *) "Connecting to SDS-011\n");
    
    /* try overcome connection problems before real actions (see document)
     * this will also inform the driver about the file description to use for writting
     * and reading */
    if (MySensor.begin(fd) == SDS011_ERROR)
    {
        p_printf(RED, (char*) "Error during trying to connect\n");
        closeout(EXIT_FAILURE);
    }
    p_printf(GREEN, (char *) "Connected\n");
    
    /* perform the requested actions */
    main_action();

    closeout(EXIT_SUCCESS);
}
