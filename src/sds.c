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
 */

#include "serial.h"
#include "sds011_lib.h"

#define PROGVERSION "1.0 / January 2019 / paulvha"

// global variables
int fd = 0xff;                   // filepointer
char progname[20];
char port[20] = "/dev/ttyUSB0";
bool PrmDebug;                   // enable debug messages
bool NoColor = false;            // no color output


// command line options
typedef struct settings
{
    bool        g_firmware;       // display firmware
    bool        g_devid;          // display device id
    bool        g_working_mode;   // display working mode (sleep/working)
    bool        g_working_period; // display working period (continuous or interval)
    bool        g_reporting_mode; // display reporting mode (reporting/ query)

    bool        g_data;           // display data
    int         q_loop;           // how many reads in query mode
    int         q_delay;          // delay between readin query mode

    bool        s_devid;          // change devid
    uint8_t     newid[2];         // hold new device id
    uint8_t     s_reporting_mode; // set working mode
    uint8_t     s_working_mode;   // set working mode
    uint8_t     s_working_period; // set working period
} settings ;

struct settings action;

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
            p_printf(YELLOW, "\nStopping SDS-011 monitor\n");
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

    action.g_data = false;           // display data
    action.q_loop  = 0xff;           // how many reads in query mode
    action.q_delay = 0xff;           // delay between read in query mode

    action.s_devid = false;          // change devid

    action.s_reporting_mode = 0xff;  // set working mode
    action.s_working_mode = 0xff;    // set working mode
    action.s_working_period = 0xff;  // set working period
}

/*********************************************************************
 * @brief : usage information
 *********************************************************************/
void usage ()
{
    p_printf(YELLOW, (char *)

    "%s [options]  (version %s)\n\n"

    "\nSDS-011 Options: \n\n"

    "-m             get current working mode\n"
    "-p             get current working period\n"
    "-r             get current reporting mode\n"
    "-d             get Device ID\n"
    "-f             get firmware version\n"
    "-o             get data                    (default : NO data)\n"

    "\nSDS-011 setting: \n\n"

    "-M [ S / W  ]  Set working mode (sleep or work)\n"
    "-P [ 0 - 30 ]  Set working period (minutes)\n"
    "-R [ Q / R  ]  Set reporting mode (query or reporting)\n"
    "-D [ 0xaabb ]  Set new device ID\n"

    "\nProgram setting: \n\n"

    "-q x:y         get data in query mode x times (0 = endless), y seconds delay.\n"
    "-H #           set correction for humidity  (e.g. 33.5 for 33.5%)\n"
    "-u device      set new device               (default = %s)\n"
    "-b             set no color output          (default : color)\n"
    "-h             show help info\n"
    "-v             set verbose / debug info     (default : NOT set\n",
     progname,PROGVERSION, port);
}

/*********************************************************************
 * @brief Parse parameter input (either commandline or file)
 *
 * @param opt : option character detected
 * @param option : pointer to potential option value
 * @param mm : measurement variables
 *
 *********************************************************************/
void parse_cmdline(int opt, char *option, struct settings *action)
{
    char *p = option;
    int i = 0;
    char buf[10];

    switch (opt) {

    case 'b':   // set NO color output
        NoColor = true;
        break;

    case 'd':   // get device-id
        action->g_devid = true;
        break;

    case 'f':   // get firmware
        action->g_firmware = true;
        break;

    case 'm':   // get current working mode  (sleep/work)
        action->g_working_mode = true;
        break;

    case 'o':   // get data continuous
        action->g_data = true;
        SetDataDisplay(true);
        break;

    case 'p':   // get working period
        action->g_working_period = true;
        break;

    case 'r':   // get current reporting mode (report/ query)
        action->g_reporting_mode = true;
        break;

    case 'u':   // Set new device
        strncpy(port,option,sizeof(port));
        break;

    case 'v':   // set debug output
        PrmDebug = true;
        break;

    case 'q':   // Set query reads and interval
        i = 0;
        while (*p != ':'){

             buf[i++] = *p++;

             if (i > sizeof(buf)){
                p_printf(RED,"query read amount too long %s\n", option);
                exit(EXIT_FAILURE);
             }
        }
        buf[i] = 0x0;
        action->q_loop = (uint8_t)strtod(buf, NULL);
        p++; // skip :

        i = 0;
        while (*p != 0x0){

             buf[i++] = *p++;

             if (i > sizeof(buf)){
                p_printf(RED,"query delay amount too long %s\n", option);
                exit(EXIT_FAILURE);
             }
        }

        buf[i] = 0x0;
        action->q_delay = (uint8_t)strtod(buf, NULL);

        break;

    case 'D':   // Set new device ID (0xaabb)
        if (strlen(p) == 6) {
            if (*p++ == '0'){
                if (*p++ == 'x'){
                    buf[0] = *p++;
                    buf[1] = *p++;
                    buf[2]= 0x0;
                    action->newid[1] = (uint8_t)strtol(buf, NULL, 16);

                    buf[0] = *p++;
                    buf[1] = *p++;
                    action->newid[0] = (uint8_t)strtol(buf, NULL, 16);

                    action->s_devid = true;
                    break;
                }
            }
        }
        p_printf(RED,"Invalid Device Id %s\n", option);
        exit(EXIT_FAILURE);
        break;

    case 'M':   // Set working  mode
        if (*option == 's' || *option == 'S') action->s_working_mode = MODE_SLEEP;
        else if (*option == 'w'|| *option == 'W') action->s_working_mode = MODE_WORK;
        else {
            p_printf(RED,"invalid working mode %s [ s or w ]\n", option);
            exit(EXIT_FAILURE);
        }
        break;

    case 'P':   // set working period
        action->s_working_period  = (uint8_t)strtod(option, NULL);

        if (action->s_working_period  < 0 || action->s_working_period  > 30){
            p_printf(RED,"invalid working period %d minutes. [ 0 - 30 ]\n", action->s_working_period );
            exit(EXIT_FAILURE);
        }

        break;

    case 'H':   // set relative humidity correction
        if (Set_Humidity_Cor(strtod(option, NULL)))
        {
            p_printf(RED,"Invalid Humidity : %s [1 - 100%]\n",option );
            exit(EXIT_FAILURE);
        }
        break;

    case 'R':   // Set reporting  mode
        if (*option == 'r' || *option == 'R') action->s_reporting_mode = REPORT_STREAM;
        else if (*option == 'q' || *option == 'Q') action->s_reporting_mode = REPORT_QUERY;
        else {
            p_printf(RED,"invalid reporting mode %s [ r or q ]\n", option);
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
void main_action(struct settings *action)
{
    if (action->g_firmware){
        /* get firmware version */
        if (Get_Firmware_Version() == SDS011_ERROR) {
            p_printf(RED,"error during reading firmware\n");
            closeout(EXIT_FAILURE);
        }
    }

    /* the device ID is captured during TryConnect() */
    if (action->g_devid){
        printf("Current DeviceID: 0x%04x\n", Get_DevID());
    }

    if (action->g_reporting_mode){
        /* current reporting mode */
        if (Get_data_reporting_mode() == SDS011_ERROR) {
            p_printf(RED,"error during getting reporting mode\n");
            closeout(EXIT_FAILURE);
        }
    }

    if (action->g_working_mode){
        /* current sleep/working mode */
        if (Get_Sleep_Work_mode() == SDS011_ERROR) {
            p_printf(RED,"error during getting sleep/working mode\n");
            closeout(EXIT_FAILURE);
        }
    }

    if (action->g_working_period){
        /* current current working period */
        if (Get_Working_Period() == SDS011_ERROR) {
            p_printf(RED,"error during getting current working period\n");
            closeout(EXIT_FAILURE);
        }
    }

    if (action->s_devid){
        /* set new devID */
        if (Set_New_Devid(action->newid) == SDS011_ERROR){
            p_printf(RED,"error during setting new Device ID\n");
            closeout(EXIT_FAILURE);
        }
    }

    if (action->s_working_mode != 0xff){
        /* set sleeping / working mode  */
        if (Set_Sleep_Work_Mode(action->s_working_mode) == SDS011_ERROR) {
            p_printf(RED,"error during setting sleeping/working mode\n");
            closeout(EXIT_FAILURE);
        }
    }

    if (action->s_working_period != 0xff){
        /* set working period to every x minutes
         * 0 = set working period to continuous
         * needs 30 seconds before the first result will show*/

        if (Set_Working_Period(action->s_working_period ) == SDS011_ERROR) {
            p_printf(RED,"error during setting working period\n");
            closeout(EXIT_FAILURE);
        }
    }

    if (action->s_reporting_mode != 0xff){
        /* set query mode  or reporting/streaming mode */
        if (Set_data_reporting_mode(action->s_reporting_mode) == SDS011_ERROR) {
            p_printf(RED,"error during setting reporint mode\n");
            closeout(EXIT_FAILURE);
        }
    }

    if (action->q_loop != 0xff){
        /* query data */
        if (Query_data( action->q_loop,  action->q_delay) == SDS011_ERROR) {
            p_printf(RED,"error during query data\n");
            closeout(EXIT_FAILURE);
        }
    }

    // last in chain... as it keeps looping
    if (action->g_data) read_sds(0, NULL);
}

/*********************************************************************
 * @brief : main program start
 *********************************************************************/
int main(int argc, char *argv[])
{
    int opt;

    /* save name for (potential) usage display */
    strncpy(progname,argv[0],20);

    init_variables();

    /* parse commandline */
    while ((opt = getopt(argc, argv, "H:hbmprdfovM:R:P:D:u:q:")) != -1)
       parse_cmdline(opt, optarg, &action);

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
        p_printf(RED, "could not open %s\n", port);
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

    /* try overcome connection problems before real actions (see document)
     * this will also inform the driver about the file description to use for writting
     * and reading */
    if (Try_Connect(fd) == SDS011_ERROR)
    {
        printf("Error during trying to connect\n");
        closeout(EXIT_FAILURE);
    }

    /* perform the real actions */
    main_action(&action);

    closeout(EXIT_SUCCESS);
}
