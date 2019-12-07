/*
 MQTT Gateway. receiving data from the mosquitto broker and sent it to Moteino
 by Felix Laevsky

 Date:  24-11-2014
 File: GatewayBase.c
 This app receives data from Mosquitto relay and forwards it to Monteino which send the command via RFM wireless transmitter
 to Moteino clients

 */

/* Serial protocol between this app (GatewayBase) and monteino:
 * GatewayBase->Monteino
 * CCCCCCNN
 * C - Command
 * N - Receiver ID in HEX-ASCHII
 *
 * Availible commands:
 * SHTOPNnn
 * SHTCLSnn
 * SHTSTOnn
 * SHTSTSnn
 *
 * Monteino->GatewayBase
 * 1. [SENDER ID]   [RSII:<message>]
 *    messages: "CLOSED", "CLOSING", "OPENING", "OPEN", "UNKNOWN"
 * 2. [ACK-sent]
 * */
/******************************************************************************
 * Includes
 * ****************************************************************************/
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mosquitto.h>
#include <pthread.h>
#include <unistd.h>
#include <termios.h>    // POSIX terminal control definitions
#include <fcntl.h>      // File control definitions
#include <stdint.h>
#include "events.h"


/******************************************************************************
 * Private Defines
 * ****************************************************************************/

#define VERSION "0.01"
#define CONTROLLERS_ID "CONTROLLERS"
#define SHUTTERS_ID "SHUTTERS"

#define SERIAL_BAUDRATE B115200
#define SHUTTER_CMD_LEN 		8
#define RBG_LED_STRIP_CMD_LEN 	11
#define AC_CMD_LEN				9
#define TV_CMD_LEN				9

#define MAX_LED_COLOR_VALUE	  	255


#define MAX_CONTROLLER_NUM 99

#define RGB_LED_STRIP_RED_MASK     1
#define RGB_LED_STRIP_GREEN_MASK   2
#define RGB_LED_STRIP_BLUE_MASK    4

#define AC_MODE_BIT				0x01
#define AC_POWER_BIT			0x02
#define AC_FAN_SPEED_BIT		0x04
#define AC_FAN_ANGLE_BIT		0x08
#define AC_TEMPERATURE_BIT		0x10
#define AC_LIGHT_BIT		    0x20

#define COMMAND_SEND_TO_CONTROLLER_TIMEOUT  5000

#define D_BYTES_IN_DWORD 4

/******************************************************************************
 * Private Types
 * ****************************************************************************/
/* This struct is used to pass data to callbacks.
 * An instance "ud" is created in main() and populated, then passed to
 * mosquitto_new(). */
struct userdata
{
    char **topics;
    int topic_count;
    int verbose;
    char *username;
    char *password;
    int fd_serial;
    pthread_mutex_t mxq; /* mutex used as quit flag */
    condition_and_mutex_ts *serial_rx_cm_event;
};

struct s_rgb_value
{
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t status;
};

typedef struct s_ac_config_t
{
	uint8_t	mode		: 3;
	uint8_t power		: 1;
	uint8_t fan_speed	: 2;
	uint8_t fan_angle   : 4;
	uint8_t temperature : 4;
	uint8_t light       : 1;
}__attribute__ ((packed)) s_ac_config;

union u_ac_config
{
	s_ac_config config;
	uint8_t		raw_data[2];
};


union u_device_value
{
    struct s_rgb_value rgb_value;
    int value;
};

enum e_device_type
{
    E_SHUTTER,
    E_LIGTH_RGB,
    E_AC,
    E_SWITCH,
	E_TV,
    E_NUM_OF_DEVICE_TYPES
};

enum e_gateway_controller_response
{
	E_GW_RESPONSE_OK,
	E_GW_RESPONSE_ERROR
};

typedef enum e_tv_opcodes_t
{
	E_TV_OPCODE_IR_CODE,
	E_TV_OPCODE_POWER
}e_tv_opcodes;

struct s_devicedata
{
    enum e_device_type device_type;
    union u_device_value value;
};

const char* device_tipics[E_NUM_OF_DEVICE_TYPES] =
{
    [E_SHUTTER] =   "/CONTROLLERS/SHUTTER/",
    [E_LIGTH_RGB] = "/CONTROLLERS/RGB/",
    [E_AC] =        "/CONTROLLERS/AC/",
    [E_SWITCH]=     "/CONTROLLERS/SWITCH/",
	[E_TV]=         "/CONTROLLERS/TV/"
};

const char* gw_response[] =
{
		[E_GW_RESPONSE_OK] = "ok",
		[E_GW_RESPONSE_ERROR] = "nothing"
};

/******************************************************************************
 * Variables
 * ****************************************************************************/
struct s_rgb_value device_rgb_led_strip[MAX_CONTROLLER_NUM+1];
union u_ac_config  device_ac[MAX_CONTROLLER_NUM+1];
uint16_t 		   device_ac_status[MAX_CONTROLLER_NUM+1];

/******************************************************************************
 * Prototypes
 * ****************************************************************************/
void write_serial(uint8_t *msg, int len, int fd);
extern ssize_t readline(int fd, void *vptr, size_t maxlen);
uint8_t val2(const char *str);
uint8_t atob (uint8_t ch);

/******************************************************************************
 * Methods
 * ****************************************************************************/
void print_usage(void)
{
	int major, minor, revision;

	mosquitto_lib_version(&major, &minor, &revision);
	printf(
			"gateway_base is a app that receives data from Mosquitto realy and forwards it to RFM wireless transmitter.\n");
	printf("gateway_base version %s running on libmosquitto %d.%d.%d.\n\n",
			VERSION, major, minor, revision);
	printf(
			"Usage: piGatewayBase [-c] [-h host] [-k keepalive] [-p port] [-q qos] [-R] -t topic ...\n");
	printf("                     [-T filter_out]\n");
#ifdef WITH_SRV
	printf("                     [-A bind_address] [-S]\n");
#else
	printf("                     [-A bind_address]\n");
#endif
	printf("                     [-u username [-P password]]\n");
	printf(
			"                     [--will-topic [--will-payload payload] [--will-qos qos] [--will-retain]]\n");
#ifdef WITH_TLS
	printf("                     [{--cafile file | --capath dir} [--cert file] [--key file]\n");
	printf("                      [--ciphers ciphers] [--insecure]]\n");
#ifdef WITH_TLS_PSK
	printf("                     [--psk hex-key --psk-identity identity [--ciphers ciphers]]\n");
#endif
#endif
	printf("       piGatewayBase --help\n\n");
	printf(
			" -A : bind the outgoing socket to this host/ip address. Use to control which interface\n");
	printf("      the client communicates over.\n");
	printf(" -d : enable debug messages.\n");
	printf(" -h : mqtt host to connect to. Defaults to localhost.\n");
	printf(
			" -i : id to use for this client. Defaults to mosquitto_sub_ appended with the process id.\n");
	printf(" -p : network port to connect to. Defaults to 1883.\n");
	printf(
			" -t : mqtt topic to subscribe to. May be repeated multiple times.\n");
	printf(" -u : provide a username (requires MQTT 3.1 broker)\n");
	printf(" -v : print published messages verbosely.\n");
	printf(" -P : provide a password (requires MQTT 3.1 broker)\n");
	printf(" --help : display this message.\n");
	printf(" --quiet : don't print error messages.\n");
#ifdef WITH_TLS
	printf(" --cafile : path to a file containing trusted CA certificates to enable encrypted\n");
	printf("            certificate based communication.\n");
	printf(" --capath : path to a directory containing trusted CA certificates to enable encrypted\n");
	printf("            communication.\n");
	printf(" --cert : client certificate for authentication, if required by server.\n");
	printf(" --key : client private key for authentication, if required by server.\n");
	printf(" --ciphers : openssl compatible list of TLS ciphers to support.\n");
	printf(" --tls-version : TLS protocol version, can be one of tlsv1.2 tlsv1.1 or tlsv1.\n");
	printf("                 Defaults to tlsv1.2 if available.\n");
	printf(" --insecure : do not check that the server certificate hostname matches the remote\n");
	printf("              hostname. Using this option means that you cannot be sure that the\n");
	printf("              remote host is the server you wish to connect to and so is insecure.\n");
	printf("              Do not use this option in a production environment.\n");
#ifdef WITH_TLS_PSK
	printf(" --psk : pre-shared-key in hexadecimal (no leading 0x) to enable TLS-PSK mode.\n");
	printf(" --psk-identity : client identity string for TLS-PSK mode.\n");
#endif
#endif
	printf(" -s: provide serial port for Monteino Gateway\n");
	printf(" -b: provide baud rate for serial port for Monteino Gateway\n");
	printf(" -b: provide baud rate for serial port for Monteino Gateway\n");
}

int send_command_to_shutter(void *obj, const char* cmd, unsigned int controllerID)
{
	struct userdata *ud;
	char msg[32] = {0};
	int error_code = 0;

	assert(obj);
	ud = (struct userdata *)obj;

	event_waitFor( ud->serial_rx_cm_event, 0); // Clear the event

	strncpy(msg, cmd, SHUTTER_CMD_LEN-2);
	sprintf(msg+SHUTTER_CMD_LEN-2, "%02x", controllerID);
	msg[SHUTTER_CMD_LEN] = 0;
	if(ud->verbose){
		printf("Sending to Monteino: %s\n", msg);
	}
	write_serial((uint8_t*)msg, SHUTTER_CMD_LEN, ud->fd_serial);

	error_code = event_waitFor( ud->serial_rx_cm_event, COMMAND_SEND_TO_CONTROLLER_TIMEOUT);

	return error_code;
}

int send_command_to_ac(void *obj, union u_ac_config *value, unsigned int controllerID)
{
	struct userdata *ud;
	uint8_t msg[32] = {0};
	int error_code = 0;
	int i;

	assert(value);
	assert(obj);
	ud = (struct userdata *)obj;

	event_waitFor( ud->serial_rx_cm_event, 0); // Clear the event

	sprintf((char*)msg, "AC%02xCFG", controllerID);
	msg[7] = value->raw_data[0];
	msg[8] = value->raw_data[1];
	if(ud->verbose){
		printf("Sending to AC controller ID %d: ", controllerID);
		for ( i = 0 ; i < 7 ; i++)
		{
			printf("%c", msg[i]);
		}
		printf("[0x%02x]", msg[i++]);
		printf("[0x%02x]", msg[i++]);
		printf("\n");
	}

	write_serial((uint8_t*)msg, AC_CMD_LEN, ud->fd_serial);

	error_code = event_waitFor( ud->serial_rx_cm_event, COMMAND_SEND_TO_CONTROLLER_TIMEOUT);

	return error_code;
}

int send_command_to_rgb_led_strip(void *obj, struct s_rgb_value *value, unsigned int controllerID)
{
	struct userdata *ud;
	uint8_t msg[32] = {0};
	int error_code = 0;
	unsigned int repeat, count = 0;

	assert(value);
	assert(obj);
	ud = (struct userdata *)obj;

	repeat = (controllerID & 0xFF00) >> 8;

	event_waitFor( ud->serial_rx_cm_event, 0); // Clear the event

	do
	{
		sprintf((char*)msg, "RGB%02xCLR", controllerID & 0x00FF);
		msg[8] = value->red;
		msg[9] = value->green;
		msg[10] = value->blue;
		if(ud->verbose){
			printf("Sending to RGB controller ID %d: CLR 0x%02x 0x%02x 0x%02x\n", controllerID & 0x00FF, msg[8], msg[9], msg[10]);
		}
		write_serial(msg, RBG_LED_STRIP_CMD_LEN, ud->fd_serial);

		error_code = event_waitFor( ud->serial_rx_cm_event, COMMAND_SEND_TO_CONTROLLER_TIMEOUT);
		controllerID++;
	}while(count++ < repeat);

	return error_code;
}

int send_ir_code_to_tv(void *obj, unsigned int controllerID, uint8_t *ir_code, e_tv_opcodes opcode)
{
	struct userdata *ud;
	uint8_t msg[32] = {0};
	int error_code = 0;
	int i;

	assert(obj);
	ud = (struct userdata *)obj;

	event_waitFor( ud->serial_rx_cm_event, 0); // Clear the event

	sprintf((char*)msg, "TV%02x", controllerID);

	if (opcode == E_TV_OPCODE_IR_CODE)
	{
		msg[4] = 'C';
	}
	else if (opcode == E_TV_OPCODE_POWER)
	{
		msg[4] = 'P';
	}
	msg[5] = ir_code[3];
	msg[6] = ir_code[2];
	msg[7] = ir_code[1];
	msg[8] = ir_code[0];
	if(ud->verbose){
		printf("Sending to TV controller ID %d: ", controllerID);
		for ( i = 0 ; i < 5 ; i++)
		{
			printf("%c", msg[i]);
		}
		printf("[0x%02x]", msg[i++]);
		printf("[0x%02x]", msg[i++]);
		printf("[0x%02x]", msg[i++]);
		printf("[0x%02x]", msg[i++]);
		printf("\n");
	}

	write_serial((uint8_t*)msg, TV_CMD_LEN, ud->fd_serial);

	error_code = event_waitFor( ud->serial_rx_cm_event, COMMAND_SEND_TO_CONTROLLER_TIMEOUT);

	return error_code;
}

void my_connect_callback(struct mosquitto *mosq, void *obj, int result)
{
	int i;
	struct userdata *ud;

	assert(obj);
	ud = (struct userdata *) obj;

	if (!result)
	{
		for (i = 0; i < ud->topic_count; i++)
		{
			mosquitto_subscribe(mosq, NULL, ud->topics[i], 0);
		}
	}
	else
	{
		if (result)
		{
			fprintf(stderr, "%s\n", mosquitto_connack_string(result));
		}
	}
}

void my_subscribe_callback(struct mosquitto *mosq, void *obj, int mid, int qos_count, const int *granted_qos)
{
	int i;
//	struct userdata *ud;

//	assert(obj);
//	ud = (struct userdata *)obj;

	printf("Subscribed (mid: %d): %d", mid, granted_qos[0]);
	for(i=1; i<qos_count; i++){
		printf(", %d", granted_qos[i]);
	}
	printf("\n");
}

/* Returns 1 (true) if the mutex is unlocked, which is the
 * thread's signal to terminate.
 */
int needQuit(pthread_mutex_t *mtx)
{
  switch(pthread_mutex_trylock(mtx)) {
    case 0: /* if we got the lock, unlock and return 1 (true) */
      pthread_mutex_unlock(mtx);
      return 1;
    case EBUSY: /* return 0 (false) if the mutex was locked */
      return 0;
  }
  return 1;
}

void write_serial(uint8_t *msg, int len, int fd)
{
	int n_written = 0;

	do{
		n_written += write( fd, &msg[n_written], 1 );
	}while(n_written < len && n_written > 0);
}

/* Return number of bytes actually read.
 * If 0, errno will contain the reason.
*/
size_t read_port(void *const data, size_t const size, int fd)
{
    ssize_t           r;

/*
    do {
        r = read(fd, data, size);
    } while (r == (ssize_t)-1 && errno == EINTR);
*/
    do {
    	r = readline(fd, data, size);
    } while (r == (ssize_t)-1);
    if (r > (ssize_t)0)
        return (size_t)r;

    if (r == (ssize_t)-1)
        return (size_t)0;

    if (r == (ssize_t)0) {
        errno = 0;
        return (size_t)0;
    }

    /* r < -1, should never happen. */
    errno = EIO;
    return (size_t)0;
}

void *read_serial(void *obj)
{
	char buffer[512] = {0};
	size_t result;
	struct userdata *ud;
	int error_code;

	assert(obj);
	ud = (struct userdata *) obj;

	if (ud->verbose)
		printf("read_serial thread is started\n");

	while (!needQuit(&(ud->mxq)))
	{
		// Data received from serial, parse and forward it to mosquito broker
		result = read_port(buffer, sizeof(buffer), ud->fd_serial);
		if (result)
		{
			if (ud->verbose)
			{
				printf("Received from moteino: ");
				fwrite(buffer, 1, result, stdout);
				printf("\n");
			}
			/* Signal to the mosquito thread we have received a response. */
			error_code = event_signal(ud->serial_rx_cm_event);
			if (ud->verbose)
			{
				if (error_code != 0)
				{
					printf("event_signal exit with error %d\n", error_code);
				}
			}
		}

		if (result > 0)
		{
			/* Sleep for a millisec, then retry */
			usleep(1000);
			continue;
		}

		fprintf(stderr,	"Error: read_serial thread is FAILED\n");
		/* Failure. */
		return NULL;
	}

	fprintf(stderr,	"Error: read_serial thread is FAILED\n");
	return NULL;
}

int open_serial(const char *serial_port)
{
	int fd = open(serial_port, O_RDWR | O_NOCTTY);

	struct termios tty;
//	struct termios tty_old;
	memset(&tty, 0, sizeof tty);

	/* Error Handling */
	if (tcgetattr(fd, &tty) != 0)
	{
		fprintf(stderr,	"Error: %d from tcgetattr: %s.\n\n", errno, strerror(errno));
		return 0;
	}

	/* Save old tty parameters */
// 	tty_old = tty;

	/* Set Baud Rate */
	cfsetospeed(&tty, (speed_t) SERIAL_BAUDRATE);
	cfsetispeed(&tty, (speed_t) SERIAL_BAUDRATE);

	/* Setting other Port Stuff */
	tty.c_cflag &= ~PARENB; // Make 8n1
	tty.c_cflag &= ~CSTOPB;
	tty.c_cflag &= ~CSIZE;
	tty.c_cflag |= CS8;

	tty.c_cflag &= ~CRTSCTS; // no flow control
	tty.c_cc[VMIN] = 1; // read doesn't block
	tty.c_cc[VTIME] = 5; // 0.5 seconds read timeout
	tty.c_cflag |= CREAD | CLOCAL; // turn on READ & ignore ctrl lines

	/* Make raw */
	cfmakeraw(&tty);

	/* Flush Port, then applies attributes */
	tcflush(fd, TCIFLUSH);
	if (tcsetattr(fd, TCSANOW, &tty) != 0)
	{
		fprintf(stderr,	"Error: %d from tcsetattr: %s.\n\n", errno, strerror(errno));
		return 0;
	}

	return fd;
}

void my_message_callback(struct mosquitto *mosq, void *obj, const struct mosquitto_message *message)
{
	struct userdata *ud;
	int i;
//	bool res;
//	char *token;
//	char *topic;
//	char msg[32] = {0};

	assert(obj);
	ud = (struct userdata *)obj;

#if 0
	topic = strdup(message->topic);
	assert(topic);
#endif
	assert(message->topic);

	if(message->retain) return;

	if(ud->verbose){
		if(message->payloadlen){
			printf("%s ", message->topic);
			fwrite(message->payload, 1, message->payloadlen, stdout);
			printf("\n");
		}else{
			printf("%s (null)\n", message->topic);
		}
		fflush(stdout);
	}else{
		if(message->payloadlen){
			fwrite(message->payload, 1, message->payloadlen, stdout);
			printf("\n");
			fflush(stdout);
		}
	}

	//TODO: parse message payload and send it to appropriate client
#if 0
	strtok(topic, "/"); // Skip first '/'
	token = strtok(topic, "/");
	if (strstr(token, CONTROLLERS_ID))
	{
		token = strtok(topic, "/");
		if (strstr(token, SHUTTERS_ID))
		{
			/* Send message to shutter controller */
			token = strtok(topic, "/"); // token contain controller id
			strncpy(msg, message->payload, SHUTTER_CMD_LEN-2);
			sprintf(msg+SHUTTER_CMD_LEN-2, "%02x", atoi(token));
			msg[SHUTTER_CMD_LEN] = 0;
			if(ud->verbose){
				printf("To Menteino: %s\n", msg);
			}
			write_serial(msg, SHUTTER_CMD_LEN, ud->fd_serial);
		}
		else
		{
			fprintf(stderr,	"Unknown string from mosquitto: %s\n", token);
		}
	}
	else
	{
		fprintf(stderr,	"Unknown string from mosquitto: %s\n", token);
	}
#endif
	if (strncmp ( message->topic, device_tipics[E_SHUTTER], strlen(device_tipics[E_SHUTTER]) ) == 0)
	{
		const char* controllerIdStr = &(((const char*)message->topic)[strlen(device_tipics[E_SHUTTER])]);
		send_command_to_shutter(obj, (const char*)message->payload, atoi(controllerIdStr));

	}
	else if (strncmp ( message->topic, device_tipics[E_LIGTH_RGB], strlen(device_tipics[E_LIGTH_RGB]) ) == 0)
	{
		const char* controllerIdStr = &(((const char*)message->topic)[strlen(device_tipics[E_LIGTH_RGB])]);
		unsigned int controllerIdFull = atoi(controllerIdStr);
		unsigned int controllerId = controllerIdFull & 0x00FF;
		if ((controllerId) >= 0 && (controllerId) <= MAX_CONTROLLER_NUM)
		{
			int tmp;
			tmp = atoi((const char*)message->payload);
			if (tmp < 0 || tmp > 100)
			{
				if (ud->verbose)
				{
					printf("Invalid RGB strip led color value: %d. should be between 0 to 100\n", tmp);
				}
				return;
			}
			tmp = tmp*MAX_LED_COLOR_VALUE/100;
			const char* color = &controllerIdStr[4];
			if (strncmp(color, "RED", 3) == 0)
			{

				device_rgb_led_strip[controllerId].red = tmp;
				device_rgb_led_strip[controllerId].status = RGB_LED_STRIP_RED_MASK;
			}
			else if ((strncmp(color, "GREEN", 5) == 0) &&
					 (device_rgb_led_strip[controllerId].status == RGB_LED_STRIP_RED_MASK))
			{
				device_rgb_led_strip[controllerId].green = tmp;
				device_rgb_led_strip[controllerId].status |= RGB_LED_STRIP_GREEN_MASK;
			}
			else if ((strncmp(color, "BLUE", 4) == 0) &&
					device_rgb_led_strip[controllerId].status == (RGB_LED_STRIP_GREEN_MASK | RGB_LED_STRIP_RED_MASK))
			{
				device_rgb_led_strip[controllerId].blue = tmp;
				device_rgb_led_strip[controllerId].status = 0;
				send_command_to_rgb_led_strip(obj, &device_rgb_led_strip[controllerId], controllerIdFull);
			}
		}
		else if (ud->verbose)
		{
			printf("Invalid RGB led strip controller: %ud\n", controllerId);
		}
	}
	else if (strncmp ( message->topic, device_tipics[E_AC], strlen(device_tipics[E_AC]) ) == 0)
	{
		const char* controllerIdStr = &(((const char*)message->topic)[strlen(device_tipics[E_AC])]);
		unsigned int controllerId = atoi(controllerIdStr);
		if (controllerId > MAX_CONTROLLER_NUM)
		{
			printf("Invalid controller ID");
			return;
		}
		const char* config = &controllerIdStr[4];
		if (strncmp(config, "MODE", 4) == 0)
		{
			device_ac[controllerId].config.mode = atoi((const char*)message->payload);
			device_ac_status[controllerId] |= AC_MODE_BIT;
		}
		else if (strncmp(config, "POWER", 5) == 0)
		{
			if (strncmp((const char*)message->payload, "ON", 2) == 0)
				device_ac[controllerId].config.power = 1;
			else
				device_ac[controllerId].config.power = 0;
			device_ac_status[controllerId] |= AC_POWER_BIT;
		}
		else if (strncmp(config, "FAN", 3) == 0)
		{
			device_ac[controllerId].config.fan_speed = atoi((const char*)message->payload);
			device_ac_status[controllerId] |= AC_FAN_SPEED_BIT;
		}
		else if (strncmp(config, "ANGLE", 5) == 0)
		{
			device_ac[controllerId].config.fan_angle = atoi((const char*)message->payload);
			device_ac_status[controllerId] |= AC_FAN_ANGLE_BIT;
		}
		else if (strncmp(config, "TEMPERATURE", 11) == 0)
		{
			uint8_t temp = atoi((const char*)message->payload);
			if (temp < 16) temp = 16;
			if (temp > 30) temp = 30;
			temp -= 16;
			device_ac[controllerId].config.temperature = temp;
			device_ac_status[controllerId] |= AC_TEMPERATURE_BIT;
		}
		else if (strncmp(config, "DISPLAY", 7) == 0)
		{
			if (strncmp((const char*)message->payload, "ON", 2) == 0)
				device_ac[controllerId].config.light = 1;
			else
				device_ac[controllerId].config.light = 0;
			device_ac_status[controllerId] |= AC_LIGHT_BIT;
		}

		if ((device_ac_status[controllerId] &
		   (AC_MODE_BIT | AC_POWER_BIT | AC_FAN_SPEED_BIT | AC_FAN_ANGLE_BIT | AC_TEMPERATURE_BIT | AC_LIGHT_BIT)) ==
		   (AC_MODE_BIT | AC_POWER_BIT | AC_FAN_SPEED_BIT | AC_FAN_ANGLE_BIT | AC_TEMPERATURE_BIT | AC_LIGHT_BIT))
		{
			device_ac_status[controllerId] = 0;
			send_command_to_ac(obj, &device_ac[controllerId], controllerId);
		}

	}
	else if (strncmp ( message->topic, device_tipics[E_SWITCH], strlen(device_tipics[E_SWITCH]) ) == 0)
	{
		//TODO: add switch device command handler
    }
	else if (strncmp ( message->topic, device_tipics[E_TV], strlen(device_tipics[E_TV]) ) == 0)
	{
		const char* controllerIdStr = &(((const char*)message->topic)[strlen(device_tipics[E_AC])]);
		unsigned int controllerId = atoi(controllerIdStr);
		uint8_t ir_code[D_BYTES_IN_DWORD];
		char * payload = message->payload;

		for ( i = 0; i < D_BYTES_IN_DWORD ; i++ )
		{
			ir_code[i] = val2(payload);
			payload += 2;
		}

		const char* opcode = &controllerIdStr[4];
		if (strncmp(opcode, "PWER", 4) == 0)
		{
			send_ir_code_to_tv(obj, controllerId, ir_code, E_TV_OPCODE_POWER);
		}
		else if (strncmp(opcode, "CODE", 4) == 0)
		{
			send_ir_code_to_tv(obj, controllerId, ir_code, E_TV_OPCODE_IR_CODE);
		}

	}
	else if (ud->verbose)
	{
		printf("Unknown topic: %s\n", message->topic);
	}

//	free(topic);
}

int main(int argc, char* argv[])
{
	int rc, i;
	struct mosquitto *mosq = NULL;
	char *id = NULL;
	char *id_prefix = NULL;
	bool clean_session = true;
	struct userdata ud;
	int port = 1883;
	int keepalive = 60;
	char *bind_address = NULL;

// 	bool insecure = false;
	char *cafile = NULL;
	char *capath = NULL;
	char *certfile = NULL;
	char *keyfile = NULL;
// 	char *tls_version = NULL;

	char *psk = NULL;
	char *psk_identity = NULL;

// 	char *ciphers = NULL;
	bool debug = false;
	char *host = "localhost";

	char *serial_port = NULL;

	pthread_t th;


	memset(&ud, 0, sizeof(struct userdata));
	memset(device_rgb_led_strip, 0, sizeof(device_rgb_led_strip));
	memset(device_ac, 0, sizeof(device_ac));
	memset(device_ac_status, 0, sizeof(device_ac_status));

	for (i = 1; i < argc; i++)
	{
		if (!strcmp(argv[i], "-p") || !strcmp(argv[i], "--port"))
		{
			if (i == argc - 1)
			{
				fprintf(stderr,
						"Error: -p argument given but no port specified.\n\n");
				print_usage();
				return 1;
			}
			else
			{
				port = atoi(argv[i + 1]);
				if (port < 1 || port > 65535)
				{
					fprintf(stderr, "Error: Invalid port given: %d\n", port);
					print_usage();
					return 1;
				}
			}
			i++;
		}
		else if (!strcmp(argv[i], "-A"))
		{
			if (i == argc - 1)
			{
				fprintf(stderr,
						"Error: -A argument given but no address specified.\n\n");
				print_usage();
				return 1;
			}
			else
			{
				bind_address = argv[i + 1];
			}
			i++;
		}
		else if (!strcmp(argv[i], "--cafile"))
		{
			if (i == argc - 1)
			{
				fprintf(stderr,
						"Error: --cafile argument given but no file specified.\n\n");
				print_usage();
				return 1;
			}
			else
			{
				cafile = argv[i + 1];
			}
			i++;
		}
		else if (!strcmp(argv[i], "--capath"))
		{
			if (i == argc - 1)
			{
				fprintf(stderr,
						"Error: --capath argument given but no directory specified.\n\n");
				print_usage();
				return 1;
			}
			else
			{
				capath = argv[i + 1];
			}
			i++;
		}
		else if (!strcmp(argv[i], "--cert"))
		{
			if (i == argc - 1)
			{
				fprintf(stderr,
						"Error: --cert argument given but no file specified.\n\n");
				print_usage();
				return 1;
			}
			else
			{
				certfile = argv[i + 1];
			}
			i++;
		}
#if 0
		else if (!strcmp(argv[i], "--ciphers"))
		{
			if (i == argc - 1)
			{
				fprintf(stderr,
						"Error: --ciphers argument given but no ciphers specified.\n\n");
				print_usage();
				return 1;
			}
			else
			{
				ciphers = argv[i + 1];
			}
			i++;
		}
#endif
		else if (!strcmp(argv[i], "-d") || !strcmp(argv[i], "--debug"))
		{
			debug = true;
		}
		else if (!strcmp(argv[i], "--help"))
		{
			print_usage();
			return 0;
		}
		else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--host"))
		{
			if (i == argc - 1)
			{
				fprintf(stderr,
						"Error: -h argument given but no host specified.\n\n");
				print_usage();
				return 1;
			}
			else
			{
				host = argv[i + 1];
			}
			i++;
		}
#if 0
		else if (!strcmp(argv[i], "--insecure"))
		{
			insecure = true;
		}
#endif
		else if (!strcmp(argv[i], "-i") || !strcmp(argv[i], "--id"))
		{
			if (id_prefix)
			{
				fprintf(stderr,
						"Error: -i and -I argument cannot be used together.\n\n");
				print_usage();
				return 1;
			}
			if (i == argc - 1)
			{
				fprintf(stderr,
						"Error: -i argument given but no id specified.\n\n");
				print_usage();
				return 1;
			}
			else
			{
				id = argv[i + 1];
			}
			i++;
		}
		else if (!strcmp(argv[i], "--key"))
		{
			if (i == argc - 1)
			{
				fprintf(stderr,
						"Error: --key argument given but no file specified.\n\n");
				print_usage();
				return 1;
			}
			else
			{
				keyfile = argv[i + 1];
			}
			i++;
		}
		else if (!strcmp(argv[i], "--psk"))
		{
			if (i == argc - 1)
			{
				fprintf(stderr,
						"Error: --psk argument given but no key specified.\n\n");
				print_usage();
				return 1;
			}
			else
			{
				psk = argv[i + 1];
			}
			i++;
		}
		else if (!strcmp(argv[i], "--psk-identity"))
		{
			if (i == argc - 1)
			{
				fprintf(stderr,
						"Error: --psk-identity argument given but no identity specified.\n\n");
				print_usage();
				return 1;
			}
			else
			{
				psk_identity = argv[i + 1];
			}
			i++;
		}
		else if (!strcmp(argv[i], "-t") || !strcmp(argv[i], "--topic"))
		{
			if (i == argc - 1)
			{
				fprintf(stderr,
						"Error: -t argument given but no topic specified.\n\n");
				print_usage();
				return 1;
			}
			else
			{
				ud.topic_count++;
				ud.topics = (char**)realloc(ud.topics, ud.topic_count * sizeof(char *));
				ud.topics[ud.topic_count - 1] = argv[i + 1];
			}
			i++;
		}
#if 0
		else if (!strcmp(argv[i], "--tls-version"))
		{
			if (i == argc - 1)
			{
				fprintf(stderr,
						"Error: --tls-version argument given but no version specified.\n\n");
				print_usage();
				return 1;
			}
			else
			{
				tls_version = argv[i + 1];
			}
			i++;
		}
#endif
		else if (!strcmp(argv[i], "-u") || !strcmp(argv[i], "--username"))
		{
			if (i == argc - 1)
			{
				fprintf(stderr,
						"Error: -u argument given but no username specified.\n\n");
				print_usage();
				return 1;
			}
			else
			{
				ud.username = argv[i + 1];
			}
			i++;
		}
		else if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--verbose"))
		{
			ud.verbose = 1;
		}
		else if (!strcmp(argv[i], "-P") || !strcmp(argv[i], "--pw"))
		{
			if (i == argc - 1)
			{
				fprintf(stderr,
						"Error: -P argument given but no password specified.\n\n");
				print_usage();
				return 1;
			}
			else
			{
				ud.password = argv[i + 1];
			}
			i++;
		}
		else if (!strcmp(argv[i], "-s") || !strcmp(argv[i], "--serial"))
		{
			if (i == argc - 1)
			{
				fprintf(stderr,
						"Error: -s argument given but no serial port specified.\n\n");
				print_usage();
				return 1;
			}
			else
			{
				serial_port = argv[i + 1];
			}
			i++;
		}
	}

	if (ud.password && !ud.username)
	{
		fprintf(stderr,
				"Warning: Not using password since username not set.\n");
	}
	if ((certfile && !keyfile) || (keyfile && !certfile))
	{
		fprintf(stderr,
				"Error: Both certfile and keyfile must be provided if one of them is.\n");
		print_usage();
		return 1;
	}
	if ((cafile || capath) && psk)
	{
		fprintf(stderr,
					"Error: Only one of --psk or --cafile/--capath may be used at once.\n");
		return 1;
	}
	if (psk && !psk_identity)
	{
		fprintf(stderr, "Error: --psk-identity required if --psk used.\n");
		return 1;
	}
	if (serial_port == NULL)
	{
		fprintf(stderr, "Error: serial port must be provided. For example -s /dev/ttyUSB0.\n");
		return 1;
	}

	event_create(&ud.serial_rx_cm_event);

	// Init serial port to communicate with monteino gateway
	ud.fd_serial = open_serial(serial_port);
	assert(ud.fd_serial);
	pthread_mutex_init(&(ud.mxq),NULL);
	pthread_mutex_lock(&(ud.mxq));
	pthread_create(&th,NULL,read_serial,&ud);

	mosq = mosquitto_new(id, clean_session, &ud);
	if (!mosq)
	{
		switch (errno)
		{
		case ENOMEM:
			fprintf(stderr, "Error: Out of memory.\n");
			break;
		case EINVAL:
			fprintf(stderr, "Error: Invalid id and/or clean_session.\n");
			break;
		}
		mosquitto_lib_cleanup();
		return 1;
	}
	mosquitto_connect_callback_set(mosq, my_connect_callback);
	mosquitto_message_callback_set(mosq, my_message_callback);
	if(debug){
		mosquitto_subscribe_callback_set(mosq, my_subscribe_callback);
	}
	rc = mosquitto_connect_bind(mosq, host, port, keepalive, bind_address);

	rc = mosquitto_loop_forever(mosq, -1, 1);

	mosquitto_destroy(mosq);
	mosquitto_lib_cleanup();

	if (rc)
	{
		fprintf(stderr, "Error: %s\n", mosquitto_strerror(rc));
	}

	/* unlock mxq to tell the thread to terminate, then join the thread */
	pthread_mutex_unlock(&(ud.mxq));
	pthread_join(th,NULL);
	return 0;
}

/***************************************************************************//**
* \fn atob
* \brief ascii value of hex digit -> real val
* 		 the value of a hex char, 0=0,1=1,A=10,F=15
* \param ch
* \retval real val
*******************************************************************************/
uint8_t atob (uint8_t ch)
{
	if (ch >= '0' && ch <= '9')
    	return ch - '0';
	else{
		switch(ch)	{
  		case 'A': return 10;
	   	case 'B': return 11;
		case 'C': return 12;
		case 'D': return 13;
		case 'E': return 14;
		case 'F': return 15;
		case 'a': return 10;
	   	case 'b': return 11;
		case 'c': return 12;
		case 'd': return 13;
		case 'e': return 14;
  		case 'f': return 15;
  }

	}
	return 0xFF; //error - char recieved not in range
}

/***************************************************************************//**
* \fn val2
* \brief like atob for 2 chars representing nibbles of byte
* \param None
* \retval None
*******************************************************************************/
uint8_t val2(const char *str)
{
	uint8_t tmp;
	tmp = atob(str[0]);
	tmp <<= 4;
	tmp |= atob(str[1]);
	return tmp;

}
