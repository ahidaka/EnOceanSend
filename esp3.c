#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <termio.h>
#include <pthread.h>
#include <time.h>
#include "utils.h"
#include "serial.h"
#include "esp3.h"

#define UNUSED_VARIABLE(x) (void)(x)
#define UNREFERENCED_PARAMETER(x) (void)(x)
#define IN     /* input parameter */
#define OUT    /* output parameter */
#define INOUT  /* input/output parameter */
#define IN_OPT     /* input parameter, optional */
#define OUT_OPT    /* output parameter, optional */
#define INOUT_OPT  /* input/output parameter, optional */

//
//#define UARTPORT "/dev/ttyS0"
#define UARTPORT "/dev/ttyUSB0"

//
//
//
#define msleep(a) usleep((a) * 1000)

#define DATABUFSIZ (256)
#define HEADER_SIZE (5)
#define CRC8D_SIZE (1)

#define RESPONSE_TIMEOUT (60)

#define TRUE (1)
#define FALSE (0)
typedef void VOID;
typedef int BOOL;
typedef unsigned char BYTE;
typedef unsigned short USHORT;
typedef int INT;
typedef unsigned int UINT;
typedef long LONG;
typedef unsigned long ULONG;

static INT DebugLevel = 0;
//
void ESP_Debug(IN INT Level)
{
	DebugLevel = Level;
}

#define _DEBUG  if (DebugLevel > 0)    // -D:   _ESP_NOTICE
#define _DEBUG2 if (DebugLevel > 1)    // -DD:  _ESP_VERBOSE
#define _DEBUG3 if (DebugLevel > 2)    // -DDD: _ESP_NOISY

//
// CO Command / Response
//
ESP_STATUS CO_WriteSleep(IN INT Period) //sleep msec
{
	ESP_STATUS status = OK;
	BYTE buffer[DATABUFSIZ];
	BYTE msBuffer[4];
	int ms10 = Period / 10;

	_DEBUG2 printf("%s: ENTER\n", __func__);
	msBuffer[3] = ms10 % 0xFF;
	ms10 >>= 8;
	msBuffer[2] = ms10 % 0xFF;
	ms10 >>= 8;
	msBuffer[1] = ms10 % 0xFF;
	ms10 >>= 8;
	msBuffer[0] = 0;

	SetCommand(CO_WR_SLEEP, buffer, msBuffer);
	SendCommand(buffer);
	msleep(1);
	status = GetResponse(buffer);
	_DEBUG3 printf("%s: status=%d\n", __func__, status);
	return status;
}

ESP_STATUS CO_WriteReset(VOID) //no params
{
	ESP_STATUS status = OK;
	BYTE buffer[DATABUFSIZ];

	_DEBUG2 printf("%s: ENTER\n", __func__);
	SetCommand(CO_WR_RESET, buffer, 0);
	SendCommand(buffer);
	msleep(1);
	//PacketDump(buffer);
	status = GetResponse(buffer);
	_DEBUG3 printf("%s: status=%d\n", __func__, status);
	return status;
}

ESP_STATUS CO_ReadVersion(OUT BYTE *VersionStr)
{
	ESP_STATUS status = OK;
	BYTE buffer[DATABUFSIZ];
	BYTE *p = &buffer[6];

	_DEBUG2 printf("%s: ENTER\n", __func__);
	SetCommand(CO_RD_VERSION, buffer, 0);
	SendCommand(buffer);
	msleep(1);
	status = GetResponse(buffer);
	printf("%s: status=%d\n", __func__, status);
	if (status == OK) {
		_DEBUG printf("ver=%d.%d.%d.%d %d.%d.%d.%d id=%02X%02X%02X%02X\n",
		       p[0],p[1],p[2],p[3],p[4],p[5],p[6],p[7],
		       p[8],p[9],p[10],p[11]);
		p[32] = '\0';
		_DEBUG printf("chip=%02X%02X%02X%02X App=<%s>\n",
		       p[12],p[13],p[14],p[15],&p[16]);
	}
	return status;
}

ESP_STATUS CO_WriteFilterAdd(IN BYTE *Id) //add filter id
{
	ESP_STATUS status = OK;
	BYTE buffer[DATABUFSIZ];
	BYTE param[6];

	_DEBUG printf("%s: ENTER add=%02X%02X%02X%02X\n", __func__, Id[0], Id[1], Id[2], Id[3]);
	if (Id != NULL) {
		param[0] = 0; // Device source ID
		param[1] = Id[0];
		param[2] = Id[1];
		param[3] = Id[2];
		param[4] = Id[3];
		param[5] = 0x80; // Apply radio
		SetCommand(CO_WR_FILTER_ADD, buffer, param);
		SendCommand(buffer);
		msleep(1);
		status = GetResponse(buffer);
	}
	_DEBUG3 printf("%s: status=%d\n", __func__, status);
	return status;
}

ESP_STATUS CO_WriteFilterDel(IN BYTE *Id) //delete filter id
{
	ESP_STATUS status = OK;
	BYTE buffer[DATABUFSIZ];

	_DEBUG printf("%s: ENTER del=%02X%02X%02X%02X\n", __func__, Id[0], Id[1], Id[2], Id[3]);
	SetCommand(CO_WR_FILTER_DEL, buffer, Id);
	SendCommand(buffer);
	msleep(1);
	status = GetResponse(buffer);
	_DEBUG3 printf("%s: status=%d\n", __func__, status);
	return status;
}

ESP_STATUS CO_WriteFilterDelAll(VOID) //no params
{
	ESP_STATUS status = OK;
	BYTE buffer[DATABUFSIZ];

	_DEBUG printf("%s: ENTER\n", __func__);
	SetCommand(CO_WR_FILTER_DEL_ALL, buffer, 0);
	SendCommand(buffer);
	msleep(1);
	status = GetResponse(buffer);
	_DEBUG3 printf("%s: status=%d\n", __func__, status);
	return status;
}

ESP_STATUS CO_WriteFilterEnable(IN BOOL On) //enable or disable
{
	ESP_STATUS status = OK;
	BYTE buffer[DATABUFSIZ];
	BYTE param[2];

	_DEBUG printf("%s: ENTER opt=%s\n", __func__, On ? "ON" : "OFF");
	param[0] = (BYTE) On;  //enable(1) or disable(0)
	param[1] = 0;  // OR condition
	SetCommand(CO_WR_FILTER_ENABLE, buffer, param);
	SendCommand(buffer);
	msleep(1);
	status = GetResponse(buffer);
	_DEBUG3 printf("%s: status=%d\n", __func__, status);
	return status;
}

ESP_STATUS CO_ReadFilter(OUT INT *count, OUT BYTE *Ids)
{
	ESP_STATUS status = OK;
	BYTE buffer[DATABUFSIZ];
	INT length;
	INT i;
	BYTE *p;
	
	_DEBUG printf("%s: ENTER\n", __func__);
	SetCommand(CO_RD_FILTER, buffer, 0);
	SendCommand(buffer);
	msleep(1);
	status = GetResponse(buffer);

	if (status == OK) {
		length = buffer[1];
		if (count != NULL) {
			*count = (length - 1) / 5;
		}
		if (Ids != NULL) {
			p = Ids;
			for(i = 0; i < *count; i++) {
				*p++ = buffer[i * 5 + 7]; 
				*p++ = buffer[i * 5 + 8]; 
				*p++ = buffer[i * 5 + 9]; 
				*p++ = buffer[i * 5 + 10]; 
				*p++ = '\0'; 
			}
		}
		_DEBUG printf("length=%d count=%d Id=%02X%02X%02X%02X\n",
	       buffer[1], *count, Ids[0], Ids[1], Ids[2], Ids[3]);
	}
	return status;
}

ESP_STATUS CO_WriteMode(IN INT Mode) //0:ERP1, 1:ERP2
{
	ESP_STATUS status = OK;
	BYTE buffer[DATABUFSIZ];
	BYTE bMode = Mode;

	_DEBUG printf("%s: ENTER\n", __func__);
	SetCommand(CO_WR_MODE, buffer, &bMode);
	SendCommand(buffer);
	msleep(1);
	status = GetResponse(buffer);
	_DEBUG3 printf("%s: status=%d\n", __func__, status);
	return status;
}

ESP_STATUS CFG_WriteESP3Mode(IN INT Mode) //1:ERP1, 2:ERP2
{
	ESP_STATUS status = OK;
	BYTE buffer[DATABUFSIZ];
	BYTE bMode = Mode;

	_DEBUG printf("%s: ENTER\n", __func__);
	SetCommand(CFG_WR_ESP3_MODE, buffer, &bMode);
	SendCommand(buffer);
	msleep(1);
	status = GetResponse(buffer);
	_DEBUG3 printf("%s: status=%d\n", __func__, status);
	return status;
}

ESP_STATUS CFG_ReadESP3Mode(OUT INT *Mode) //1:ERP1, 2:ERP2
{
	ESP_STATUS status = OK;
	BYTE buffer[DATABUFSIZ];
	byte *p = &buffer[6];

	_DEBUG printf("%s: ENTER\n", __func__);
	SetCommand(CFG_RD_ESP3_MODE, buffer, 0);
	SendCommand(buffer);
	msleep(1);
	status = GetResponse(buffer);
	_DEBUG3 printf("%s: status=%d mode=%d\n", __func__, status, *p);
	return status;
}

//
//
//
void SetCommand(ESP3_CMD Cmd, BYTE *Buffer, BYTE *Param)
{
	enum LENGTH { NOT_SUPPORTED = -1};
	int i;
	int length = 1;

	switch(Cmd) {
	case CO_WR_SLEEP: /*1 Order to enter in energy saving mode */
		length = 5;
		break;
	case CO_WR_RESET: /*2 Order to reset the device */
	case CO_RD_VERSION: /*3 Read the device (SW) version / (HW) version, chip ID etc. */
	case CO_RD_SYS_LOG: /*4 Read system log from device databank */
	case CO_WR_SYS_LOG: /*5 Reset System log from device databank */
	case CO_WR_BIST: /*6 Perform Flash BIST operation */
	case CO_WR_IDBASE: /*7 Write ID range base number */
		break;
	case CO_RD_IDBASE: /*8 Read ID range base number */
		length = NOT_SUPPORTED;
		break;
	case CO_WR_REPEATER: /*9 Write Repeater Level off,1,2 */
		length = 3;
		break;
	case CO_RD_REPEATER: /*10 Read Repeater Level off,1,2 */
		break;
	case CO_WR_FILTER_ADD: /*11 Add filter to filter list */
		length = 7;
		break;
	case CO_WR_FILTER_DEL: /*12 Delete filter from filter list */
		length = 6;
		break;
	case CO_WR_FILTER_DEL_ALL: /*13 Delete all filter */
		break;
	case CO_WR_FILTER_ENABLE: /*14 Enable/Disable supplied filters */
		length = 3;
		break;
	case CO_RD_FILTER: /*15 Read supplied filters */
		break;
	case CO_WR_WAIT_MATURITY: /*16 Waiting till end of maturity time before received radio telegrams will transmitted */
	case CO_WR_SUBTEL: /*17 Enable/Disable transmitting additional subtelegram info */
	case CO_WR_MEM: /*18 Write x bytes of the Flash, XRAM, RAM0 …. */
	case CO_RD_MEM: /*19 Read x bytes of the Flash, XRAM, RAM0 …. */
	case CO_RD_MEM_ADDRESS: /*20 Feedback about the used address and length of the config area and the Smart Ack Table */
	case CO_RD_SECURITY: /*21 Read own security information (level, key) */
	case CO_WR_SECURITY: /*22 Write own security information (level, key) */
	case CO_WR_LEARNMODE: /*23 Enable/disable learn mode */
	case CO_RD_LEARNMODE: /*24 Read learn mode */
	case CO_WR_SECUREDEVICE_ADD: /*25 Add a secure device */
	case CO_WR_SECUREDEVICE_DEL: /*26 Delete a secure device */
	case CO_RD_SECUREDEVICE_BY_INDEX: /*27 Read secure device by index */
		length = NOT_SUPPORTED;
		break;
	case CO_WR_MODE: /*28 Sets the gateway transceiver mode */
		length = 2;
		break;

	/* Special function */
	case CFG_WR_ESP3_MODE: /*28 Sets the gateway transceiver mode */
		length = 2;
	case CFG_RD_ESP3_MODE: /*28 Sets the gateway transceiver mode */
		length = 1;

	default:
		length = NOT_SUPPORTED;
		break;
	}

	if (length != NOT_SUPPORTED) {
		Buffer[0] = 0x55; // Sync Byte
		Buffer[1] = 0; // Data Length[0]
		Buffer[2] = length; // Data Length[1]
		Buffer[3] = 0; // Optional Length
		// check Config command
		Buffer[4] = Cmd >= CFG_WR_ESP3_MODE ? 11 : 5; // Packet Type = CO (5) or CFG(11)
		Buffer[5] = Crc8CheckEx(Buffer, 1, 4); // CRC8H
		Buffer[6] = Cmd; // Command Code
		if (Cmd >= CFG_WR_ESP3_MODE) {
			/* Set Config commang */
			Buffer[6] = Cmd - CFG_COMMAND_BASE; // Command Code
		}
		for(i = 0; i < (length - 1); i++)
			Buffer[7 + i] = Param[i];
		Buffer[6 + length] = Crc8CheckEx(Buffer, 6, length); // CRC8D
	}
}
