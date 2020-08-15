#include <stdio.h>
#include <stdlib.h>
#include <stddef.h> //offsefof
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <termio.h>
#include "dpride.h"
#include "queue.h"
#include "utils.h"
#include "serial.h"
#include "esp3.h"

#define _Debug (0)

#define _QD if (_Debug) 
#define _D if (_Debug) 
#define _DEBUG2 if (_Debug) 
#define _DEBUG3 if (_Debug) 

//
#define msleep(a) usleep((a) * 1000)

#define MAINBUFSIZ (1024)
#define DATABUFSIZ (528)
#define HEADER_SIZE (5)
#define CRC8D_SIZE (1)

#define RESPONSE_TIMEOUT (90)

//
//
//
#if defined(STAILQ_HEAD)
#undef STAILQ_HEAD

#define STAILQ_HEAD(name, type)                                 \
	struct name {                                                   \
	struct type *stqh_first;        /* first element */             \
	struct type **stqh_last;        /* addr of last next element */ \
	int num_control;      \
	pthread_mutex_t lock; \
	}
#endif

#define INCREMENT(a) ((a) = (((a)+1) & 0x7FFFFFFF))

#define QueueSetLength(Buffer, Length) \
	((QUEUE_ENTRY *)(Buffer - offsetof(QUEUE_ENTRY, Data)))->Length = (Length)

#define QueueGetLength(Buffer) \
	(((QUEUE_ENTRY *)((Buffer) - offsetof(QUEUE_ENTRY, Data)))->Length)

//typedef struct QueueHead {...} QEntry;
STAILQ_HEAD(QueueHead, QEntry);
typedef struct QueueHead QUEUE_HEAD;

#if OLD_STRUCT
struct QEntry {
	INT Number;
	INT Length;
	BYTE Data[DATABUFSIZ];
	STAILQ_ENTRY(QEntry) Entries;
};
#else
struct QEntry {
	BYTE Data[DATABUFSIZ];
	INT Number;
	INT Length;
	STAILQ_ENTRY(QEntry) Entries;
};
#endif
typedef struct QEntry QUEUE_ENTRY;

QUEUE_HEAD DataQueue;     // Received Data
QUEUE_HEAD ResponseQueue; // CO Command Responce
QUEUE_HEAD ExtraQueue;    // Event, Smack, etc currently not used
QUEUE_HEAD FreeQueue;     // Free buffer

//
//
//
BOOL stop_read;
BOOL stop_action;
BOOL stop_job;
BOOL read_ready;

typedef struct _THREAD_BRIDGE {
	pthread_t ThRead;
	pthread_t ThAction;
}
THREAD_BRIDGE;

void SetThdata(THREAD_BRIDGE Tb);
THREAD_BRIDGE *GetThdata(void);
static THREAD_BRIDGE _Tb;
void SetThdata(THREAD_BRIDGE Tb) { _Tb = Tb; }
THREAD_BRIDGE *GetThdata(void) { return &_Tb; }

#define CleanUp(i)

VOID FreeQueueInit(VOID);

BOOL InitSerial(OUT int *pFd);
void MainJob(BYTE *buffer);

static const INT QueueCount = 8;
static const INT QueueTryTimes = 10;
static const INT QueueTryWait = 2; //msec
//
void SetFd(int fd);
int GetFd(void);
static int _Fd;
void SetFd(int fd) { _Fd = fd; }
int GetFd(void) { return _Fd; }
//
static EO_CONTROL EoControl;
//
//
//
INT Enqueue(QUEUE_HEAD *Queue, BYTE *Buffer)
{
	QUEUE_ENTRY *qEntry;
	const size_t offset = offsetof(QUEUE_ENTRY, Data);

	_D printf("**Enqueue:%p\n", Buffer);

	qEntry = (QUEUE_ENTRY *)(Buffer - offset);

	pthread_mutex_lock(&Queue->lock);
	qEntry->Number = INCREMENT(Queue->num_control);

	STAILQ_INSERT_TAIL(Queue, qEntry, Entries);
	pthread_mutex_unlock(&Queue->lock);
	//printf("**Enqueue:%p=%d (%p)\n", newEntry->Data, newEntry->Number, newEntry);

	return Queue->num_control;
}

BYTE *Dequeue(QUEUE_HEAD *Queue)
{
	QUEUE_ENTRY *entry;
	BYTE *buffer;
	//int num;

	//_D printf("**Dequeue:\n");

	if (STAILQ_EMPTY(Queue)) {
		//printf("**Dequeue Empty=%s!\n", 
		//	Queue == &DataQueue ? "Data" : "Free");
		if (Queue == &FreeQueue) {
			printf("**Dequeue Free=Empty!\n"); 
		}
		else {
			sleep(1);
		}
		return NULL;
	}
	pthread_mutex_lock(&Queue->lock);
	entry = STAILQ_FIRST(Queue);
	buffer = entry->Data;
	//num = entry->Number;
	STAILQ_REMOVE(Queue, entry, QEntry, Entries);
	pthread_mutex_unlock(&Queue->lock);

	//printf("**Dequeue list(%d):%p\n", num, buffer);

	return buffer;
}

//
void QueueData(QUEUE_HEAD *Queue, BYTE *DataBuffer, int Length)
{
#ifdef SECURE_DEBUG
	printf("+Q"); PacketDump(DataBuffer);
#endif
	QueueSetLength(DataBuffer, Length);
	Enqueue(Queue, DataBuffer);
}

VOID FreeQueueInit(VOID)
{
	INT i;
	struct QEntry *freeEntry;
	// We need special management space for FreeQueue entries
	//const INT FreeQueueSize = DATABUFSIZ + (sizeof(struct QEntry) - DATABUFSIZ) * 2;

	for(i = 0; i < QueueCount; i++) {
		freeEntry = (struct QEntry *) calloc(sizeof(struct QEntry), 1);
		if (freeEntry == NULL) {
			fprintf(stderr, "InitializeQueue: calloc error=%d\n", i);
			return;
		}
		Enqueue(&FreeQueue, freeEntry->Data);
	}
}

void *ReadThread(void *arg)
{
	INT fd = GetFd();
	ESP_STATUS rType;
	BYTE   *dataBuffer;
	USHORT  dataLength = 0;
	BYTE   optionLength = 0;
	BYTE   packetType = 0;
	INT    totalLength;
	INT count = 0;

	while(!stop_read) {

		do {
			dataBuffer = Dequeue(&FreeQueue);
			if (dataBuffer == NULL) {
				if (QueueTryTimes >= count) {
					fprintf(stderr, "FreeQueue empty at ReadThread\n");
					return (void*) NULL;
				}
				count++;
				msleep(QueueTryWait);
			}
		}
		while(dataBuffer == NULL);

		read_ready = TRUE;
		rType = GetPacket(fd, dataBuffer, (USHORT) DATABUFSIZ);
		if (stop_job) {
			printf("**ReadThread breaked by stop_job-1\n");
			break;
		}
		else if (rType == OK) {
			dataLength = (dataBuffer[0] << 8) + dataBuffer[1];
			optionLength = dataBuffer[2];
			packetType = dataBuffer[3];
			totalLength = HEADER_SIZE + dataLength + optionLength + CRC8D_SIZE;
#if RAW_INPUT
			if (1) {
				BYTE dataType = dataBuffer[5];
				printf("*_:");
				PacketDump(dataBuffer);
				printf("D:dLen=%d oLen=%d tot=%d typ=%02X daT=%02X\n",
					dataLength, optionLength, totalLength, packetType, dataType);
			}
#endif
		}
		else {
			printf("invalid rType==%02X\n\n", rType);
		}

		if (stop_job) {
			printf("**ReadThread breaked by stop_job-2\n");
			break;
		}
		printf("**ReadTh: process=%d\n", packetType);

		switch (packetType) {
		case RADIO_ERP1: //1  Radio telegram
		case RADIO_ERP2: //0x0A ERP2 protocol radio telegram
			QueueData(&DataQueue, dataBuffer, totalLength);
			break;
		case RESPONSE: //2 Response to any packet
			QueueData(&ResponseQueue, dataBuffer, totalLength);
			break;
		case RADIO_SUB_TEL: //3 Radio subtelegram
		case EVENT: //4 Event message
		case COMMON_COMMAND: //5 Common command
		case SMART_ACK_COMMAND: //6 Smart Ack command
		case REMOTE_MAN_COMMAND: //7 Remote management command
		case RADIO_MESSAGE: //9 Radio message
		case CONFIG_COMMAND: //0x0B ESP3 configuration
		default:
			QueueData(&ExtraQueue, dataBuffer, totalLength);
			fprintf(stderr, "Unknown packet=%d\n", packetType);
			break;
		}
	}
	//printf("ReadThread end=%d stop_read=%d\n", stop_job, stop_read);
	return (void*) NULL;
}

//
void *ActionThread(void *arg)
{
	BYTE *data;

	_D printf("**ActionThread:\n");
#if 0
	buffer = malloc(DATABUFSIZ);
	if (buffer == NULL) {
		fprintf(stderr, "malloc error at ActionThread\n");
		//return OK;
		return (void*) NULL;
	}
#endif
	while(!stop_action && !stop_job) {
		data = Dequeue(&DataQueue);
		if (data == NULL)
			msleep(1);
		else {
			MainJob(data);
			Enqueue(&FreeQueue, data);
		}
	}
	return (void*) NULL;
}

//
//
//
void USleep(int Usec)
{
	const int mega = (1000 * 1000);
	struct timespec t;
	t.tv_sec = 0;

	int sec = Usec / mega;

	if (sec > 0) {
		t.tv_sec = sec;
	}
	t.tv_nsec = (Usec % mega) * 1000 * 2; ////DEBUG////
	nanosleep(&t, NULL);
}

BOOL InitSerial(OUT int *pFd)
{
	static char *ESP_PORTS[] = {
		EO_ESP_PORT_USB,
		EO_ESP_PORT_S0,
		EO_ESP_PORT_AMA0,
		NULL
	};
	char *pp;
	int i;
	EO_CONTROL *p = &EoControl;
	int fd;
	struct termios tio;

	if (1 /*p->ESPPort[0] == '\0'*/) {
		// default, check for available port
		pp = ESP_PORTS[0];
		for(i = 0; pp != NULL && *pp != '\0'; i++) {
			if ((fd = open(pp, O_RDWR)) >= 0) {
				close(fd);
				p->ESPPort = pp;
				//printf("##%s: found=%s\n", __func__, pp);
				break;
			}
			pp = ESP_PORTS[i+1];
		}
	}

	if (p->ESPPort && p->ESPPort[0] == '\0') {
		fprintf(stderr, "PORT access admission needed.\n");
		return 1;
	}
	else if ((fd = open(p->ESPPort, O_RDWR)) < 0) {
		fprintf(stderr, "open error:%s\n", p->ESPPort);
		return 1 ;
	}
	bzero((void *) &tio, sizeof(tio));
	//tio.c_cflag = B57600 | CRTSCTS | CS8 | CLOCAL | CREAD;
	tio.c_cflag = B57600 | CS8 | CLOCAL | CREAD;
	tio.c_cc[VTIME] = 0; // no timer
	tio.c_cc[VMIN] = 1; // at least 1 byte
	//tcsetattr(fd, TCSANOW, &tio);
	cfsetispeed( &tio, B57600 );
	cfsetospeed( &tio, B57600 );

	cfmakeraw(&tio);
	tcsetattr( fd, TCSANOW, &tio );
	ioctl(fd, TCSETS, &tio);
	*pFd = fd;

	printf("ESP port: %s\n", p->ESPPort);
	
	return 0;
}

//
// support ESP3 functions
// Common response
//
ESP_STATUS GetResponse(OUT BYTE *Buffer)
{
	INT startMSec;
	INT timeout;
	INT length;
	BYTE *data;
	ESP_STATUS responseMessage;

	startMSec = SystemMSec();
	do {
		data = Dequeue(&ResponseQueue);
		if (data != NULL) {
			break;
		}
		timeout = SystemMSec() - startMSec;
		if (timeout > RESPONSE_TIMEOUT) {
			printf("****GetResponse() Timeout=%d\n", timeout);
			return TIMEOUT;
		}
		msleep(1);
	}
	while(1);

	length = QueueGetLength(data);
	memcpy(Buffer, data, length);
	Enqueue(&FreeQueue, data);

	switch(Buffer[5]) {
	case 0:
	case 1:
	case 2:
	case 3:
		responseMessage = Buffer[5];
		break;
	default:
		responseMessage = INVALID_STATUS;
		break;
	}

	//PacketDump(Buffer);
	//printf("**GetResponse=%d\n", responseMessage);
	return responseMessage;
}

//
//
void SendData(BYTE *dataBuffer)
{
	int length = dataBuffer[2] + dataBuffer[3] + 7;
	printf("**SendData: fd=%d len=(%d,%d) %d\n", GetFd(),
		dataBuffer[2], dataBuffer[3], length);
	write(GetFd(), dataBuffer, length);
}

//
//
//
VOID SetERP1Data(BYTE ROrg, BYTE *Buffer, BYTE *Data, INT Length)
{
	INT i;
	enum { optionalDataLength = 7, };
	static const BYTE optionalData[optionalDataLength] = {
		3, //send subtel
		0xFF, // DestID
		0xFF, // DestID
		0xFF, // DestID
		0xFF, // DestID
		0xFF, // dBm
		0, // security level
	};

	if (Length > 0) {
		Buffer[0] = 0x55; // Sync Byte
		Buffer[1] = 0; // Data Length[0]
		Buffer[2] = Length + 1; // DataLength[1], Len+rORG
		Buffer[3] = optionalDataLength;
		Buffer[4] = 1; // Packet Type, RADIO_ERP1
		Buffer[5] = Crc8CheckEx(Buffer, 1, 4); // CRC8H
		Buffer[6] = ROrg; // R-ORG

		for(i = 0; i < Length; i++) {
			Buffer[7 + i] = Data[i];
		}
		for(i = 0; i < optionalDataLength; i++) {
			Buffer[7 + Length + i] = optionalData[i];
		}

		Buffer[7 + Length + optionalDataLength] =
			Crc8CheckEx(Buffer, 6, 1 + Length + optionalDataLength); // CRC8D
	}
}

//
// 
//
BYTE Erp2TelegramType(BYTE ROrg, BYTE *ExtendedTelegramType)
{
	BYTE type = 0;
	BYTE ext;

	switch(ROrg) {
	case 0xF6: // RPS
		type = 0x20;
		break;
	case 0xD5: // 1BS
		type = 0x21;
		break;
	case 0xA5: // 4BS
		type = 0x22;
		break;
	case 0xD0: // Smack-Sig
		type = 0x23;
		break;
	case 0xD2: // VLD
		type = 0x24;
		break;
	case 0xD4: // UTE
		type = 0x25;
		break;
	case 0xD1: // MSC
		type = 0x26;
		break;
	case 0x30: // SEC
		type = 0x27;
		break;
	case 0x31: // SEC-ENC
		type = 0x28;
		break;
	case 0x35: // SEC-SW
		type = 0x29;
		break;
	case 0xB3: // GP-SD
		type = 0x2A;
		break;

	case 0xC5: // SYS_EX
		type = 0x2F;
		ext = 0;
		break;
	case 0xC6: // Smack Learn Req
		type = 0x2F;
		ext = 1;
		break;
	case 0xC7: // Smack Learn Req
		type = 0x2F;
		ext = 2;
		break;
	case 0x40: // CDM
		type = 0x2F;
		ext = 3;
		break;
	case 0x32: // Secure telegram
		type = 0x2F;
		ext = 4;
		break;
	case 0xB0: // GP_TI
		type = 0x2F;
		ext = 5;
		break;
	case 0xB1: // GP_TR
		type = 0x2F;
		ext = 6;
		break;
	case 0xB2: // GP_CD
		type = 0x2F;
		ext = 7;
		break;

	default:
		break;
	}

	if (type == 0x2F && ExtendedTelegramType != NULL) {
		*ExtendedTelegramType = ext;
	}
	return type;
}

//
// Not worked !
//
VOID SetERP2Data(BYTE ROrg, BYTE *Buffer, BYTE *Data, INT Length)
{
	INT i;
	BYTE extendedTelegramType = 0xFF;
	BYTE eTTLen = 0; // Extended Telegram Type Length

	if (Length > 0) {
		Buffer[0] = 0x55; // Sync Byte
		Buffer[1] = 0; // Data Length[0]
		//Buffer[2] = Length; // Data Length[1]
		Buffer[3] = 0; // Optional Length
		// check Config command
		Buffer[4] = 0xA; // Packet Type, RADIO_ERP2
		Buffer[5] = Crc8CheckEx(Buffer, 1, 4); // CRC8H
		Buffer[6] = Erp2TelegramType(ROrg, &extendedTelegramType);
		if (extendedTelegramType != 0xFF) {
			eTTLen = 1;
			// Extended Telegram Type available
			Buffer[7] = extendedTelegramType;
		}
		Buffer[2] = 1 + eTTLen + 4 + Length;

		//for(i = 0; i < 4; i++) {
		//	Buffer[7 + eTTLen + i] = 0xFF;
		//}
		Buffer[7 + eTTLen + 0] = 0x04;
		Buffer[7 + eTTLen + 1] = 0x00;
		Buffer[7 + eTTLen + 2] = 0x78;
		Buffer[7 + eTTLen + 3] = 0xF1;

		for(i = 0; i < (Length); i++) {
			Buffer[7 + 4 + eTTLen + i] = Data[i];
			printf("%d:%d %02X \n", i, 7 + 4 + eTTLen + i, Data[i]);			
		}
		Buffer[6 + 1 + eTTLen + 4 + Length ] = Crc8CheckEx(Buffer, 6, 1 + eTTLen + 4 + Length); // CRC8D
		printf("**SetERP2Data: pos/len=%d\n", 1 + eTTLen + 4 + Length);
	}
}


void SendCommand(BYTE *cmdBuffer)
{
	int length = cmdBuffer[2] + 1 + 5 + 1;
	//printf("**SendCommand: fd=%d len=%d\n", GetFd(), length);

	write(GetFd(), cmdBuffer, length);
}

//
//
//
INT SendTeachIn(VOID)
{
	//enum { length = 5 * 1 + 2, };
	enum { length = 5 * 2 + 2, };
	//enum { length = 5 * 1 + 2, };
	ESP_STATUS status = OK;
	BYTE buffer[DATABUFSIZ];
	BYTE dataBuffer[length];
	INT i, j;

	_DEBUG2 printf("%s: ENTER\n", __func__);

	dataBuffer[0] = 0x01;
	dataBuffer[1] = 0x60;
	dataBuffer[2] = 0x41;
	dataBuffer[3] = 0x98;
	dataBuffer[4] = 0x00;
	dataBuffer[5] = 0x12;
	dataBuffer[6] = 0x82;

#if 1
	for(i = 1; i < 3; i++)
	//for(i = 1; i < 2; i++)
	for(j = 0; j < 5; j++) {
		dataBuffer[i * 5 + j + 2] = dataBuffer[2 + j];
	}
#endif
	for(i = 0; i < length; i++) {
		printf("%d: %02X \n", i, dataBuffer[i]);
	}
	printf("\n");

	SetERP1Data(0xB0, buffer, dataBuffer, length);
	//SetERP2Data(0xB0, buffer, dataBuffer, length);
	SendData(buffer);

	PacketDebug(2);
	PacketDump(buffer + 1);

	msleep(1);
	//status = GetResponse(buffer);
	_DEBUG3 printf("%s: status=%d\n", __func__, status);
	return status;
}

void MainJob(BYTE *buffer)
{
	//_D printf("*** MainJob\n");
}

//
//
//
int main(int ac, char **av)
{
	int fd;
	int rtn;
	pthread_t thRead;
	pthread_t thAction;
	THREAD_BRIDGE *thdata;

	//INT limitCount = 1000;
	//INT limitCount = 100;
	//INT tryCount = 0;
	INT status;

	////////////////////////////////
	// Threads, queues, and serial pot

	STAILQ_INIT(&DataQueue);
	STAILQ_INIT(&ResponseQueue);
	STAILQ_INIT(&ExtraQueue);
	STAILQ_INIT(&FreeQueue);

	FreeQueueInit();

	pthread_mutex_init(&DataQueue.lock, NULL);
	pthread_mutex_init(&ResponseQueue.lock, NULL);
	pthread_mutex_init(&ExtraQueue.lock, NULL);

	if (InitSerial(&fd)) {
		fprintf(stderr, "InitSerial error\n");
		CleanUp(0);
		exit(1);
	}

	thdata = calloc(sizeof(THREAD_BRIDGE), 1);
	if (thdata == NULL) {
		printf("calloc error\n");
		CleanUp(0);
		exit(1);
	}
	SetFd(fd);

	rtn = pthread_create(&thAction, NULL, ActionThread, (void *) thdata);
	if (rtn != 0) {
		fprintf(stderr, "pthread_create() ACTION failed for %d.",  rtn);
		CleanUp(0);
		exit(EXIT_FAILURE);
	}

	rtn = pthread_create(&thRead, NULL, ReadThread, (void *) thdata);
	if (rtn != 0) {
		fprintf(stderr, "pthread_create() READ failed for %d.",  rtn);
		CleanUp(0);
		exit(EXIT_FAILURE);
	}

	thdata->ThAction = thAction;
	thdata->ThRead = thRead;
	SetThdata(*thdata);

	status = CO_WriteReset();
	printf("CO_WriteReset, status=%d\n", status);
	msleep(200);

	//status = CO_ReadVersion(buffer);
	//printf("CO_ReadVersion, status=%d\n", status);

	//status = CO_WriteMode(1); //0:ERP1, 1:ERP2
	//printf("CO_WriteMode, status=%d\n", status);

	status = SendTeachIn();
	printf("SendTeachIn, status=%d\n", status);

	pthread_join(thAction, NULL);
	pthread_join(thRead, NULL);

	return 0;
}
