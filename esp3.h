//
// ESP3.h
//

#if defined(ESP3_H)
#else
#define ESP3_H
//
#include "typedefs.h"
//
typedef enum CommonCommand {
        /*01*/ CO_WR_SLEEP=1, // Order to enter in energy saving mode
        /*02*/ CO_WR_RESET=2, // Order to reset the device
        /*03*/ CO_RD_VERSION=3, // Read the device (SW) version / (HW) version, chip ID etc.
        /*04*/ CO_RD_SYS_LOG=4, // Read system log from device databank
        /*05*/ CO_WR_SYS_LOG=5, // Reset System log from device databank
        /*06*/ CO_WR_BIST=6, //  Perform Flash BIST operation
        /*07*/ CO_WR_IDBASE=7, // Write ID range base number
        /*08*/ CO_RD_IDBASE=8, // Read ID range base number
        /*09*/ CO_WR_REPEATER=9, // Write Repeater Level off,1,2
        /*10*/ CO_RD_REPEATER=10, // Read Repeater Level off,1,2
        /*11*/ CO_WR_FILTER_ADD=11, // Add filter to filter list
        /*12*/ CO_WR_FILTER_DEL=12, // Delete filter from filter list
        /*13*/ CO_WR_FILTER_DEL_ALL=13, // Delete all filter
        /*14*/ CO_WR_FILTER_ENABLE=14, // Enable/Disable supplied filters
        /*15*/ CO_RD_FILTER=15, // Read supplied filters
        /*16*/ CO_WR_WAIT_MATURITY=16, // Waiting till end of maturity time before received radio telegrams will transmitted
        /*17*/ CO_WR_SUBTEL=17, // Enable/Disable transmitting additional subtelegram info
        /*18*/ CO_WR_MEM=18, // Write x bytes of the Flash, XRAM, RAM0 ….
        /*19*/ CO_RD_MEM=19, // Read x bytes of the Flash, XRAM, RAM0 ….
        /*20*/ CO_RD_MEM_ADDRESS=20, // Feedback about the used address and length of the config area and the Smart Ack Table
        /*21*/ CO_RD_SECURITY=21, // Read own security information (level, key)
        /*22*/ CO_WR_SECURITY=22, // Write own security information (level, key)
        /*23*/ CO_WR_LEARNMODE=23, // Enable/disable learn mode
        /*24*/ CO_RD_LEARNMODE=24, // Read learn mode
        /*25*/ CO_WR_SECUREDEVICE_ADD=25, // Add a secure device
        /*26*/ CO_WR_SECUREDEVICE_DEL=26, // Delete a secure device
        /*27*/ CO_RD_SECUREDEVICE_BY_INDEX=27, //  Read secure device by index
        /*28*/ CO_WR_MODE=28, //  Sets the gateway transceiver mode

	/* special for Config Command */
	CFG_COMMAND_BASE=1000,
	CFG_WR_ESP3_MODE=1001,
	CFG_RD_ESP3_MODE=1002,
}
ESP3_CMD;

//
typedef enum PacketType {
        /*0x01*/ RADIO_ERP1=1, // Radio telegram
        /*0x02*/ RESPONSE=2, // Response to any packet
        /*0x03*/ RADIO_SUB_TEL=3, // Radio subtelegram
        /*0x04*/ EVENT=4, // Event message
        /*0x05*/ COMMON_COMMAND=5, // Common command
        /*0x06*/ SMART_ACK_COMMAND=6, // Smart Ack command
        /*0x07*/ REMOTE_MAN_COMMAND=7, // Remote management command
        /*0x09*/ RADIO_MESSAGE=9, // Radio message
        /*0x0A*/ RADIO_ERP2=0x0A, // ERP2 protocol radio telegram
        /*0x0B*/ CONFIG_COMMAND=0x0B // ESP3 configuration
}
ESP3_PACKET;

typedef enum RetuenCodes {
	OK=0,
	RET_OK=0,
	RET_ERROR=1,
        NOT_SUPPORTED=2,
        RET_NOT_SUPPORTED=2,
        WRONG_PARAM=3,
	INVALID_PARAMETER=3,
        RET_WRONG_PARAM=3,
        RET_OPERATION_DENIED=4,
        RET_LOCK_SET=5,
        RET_BUFFER_TOO_SMALL=6,
        BUFFER_TOO_SMALL=6,
        RET_NO_FREE_BUFFER=7,
        STATUS_BASE=64,
        NEW_RX_DATA,
        NO_RX_DATA,
        OUT_OF_RANGE,
        INVALID_CRC,
	INVALID_RESPONSE,
	INVALID_DATA,
	INVALID_STATUS,
	NO_FILE,
	TIMEOUT
}
RETURN_CODE, ESP_STATUS;

ESP_STATUS CO_WriteSleep(IN INT Period); //sleep msec
ESP_STATUS CO_WriteReset(VOID); //no params
ESP_STATUS CO_ReadVersion(OUT BYTE *VersionStr);
ESP_STATUS CO_WriteFilterAdd(IN BYTE *Id); //add filter id
ESP_STATUS CO_WriteFilterDel(IN BYTE *Id); //delete filter id
ESP_STATUS CO_WriteFilterDelAll(VOID); //no params
ESP_STATUS CO_WriteFilterEnable(IN BOOL On); //enable or disable
ESP_STATUS CO_ReadFilter(OUT INT *count, OUT BYTE *Ids);
ESP_STATUS CO_WriteMode(IN INT Mode); //1:ERP1, 2:ERP2
ESP_STATUS CFG_WriteESP3Mode(IN INT Mode); //1:ERP1, 2:ERP2
ESP_STATUS CFG_ReadESP3Mode(OUT INT *Mode); //1:ERP1, 2:ERP2
ESP_STATUS GetResponse(OUT BYTE *Buffer);
void SetCommand(IN ESP3_CMD Cmd, INOUT BYTE *Buffer, IN BYTE *Param);

//
extern BYTE Crc8CheckEx(BYTE *data, size_t offset, size_t count);
extern BYTE Crc8Check(BYTE *data, size_t count);

extern VOID SendCommand(BYTE *cmdBuffer);
extern ESP_STATUS GetResponse(OUT BYTE *Buffer);
//
void ESP_Debug(IN INT Level);

#endif
