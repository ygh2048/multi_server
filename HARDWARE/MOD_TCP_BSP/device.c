#include "device.h"

#include "W5500.h"     /* FDM1/FDM2/FDM4/VDM, RWB_READ/WRITE, COMMON_R 等 */
#include "delay.h"  



typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned char  SOCKET;

/* 环形缓冲大小（与芯片配置一致） */
#define S_RX_SIZE  2048
#define S_TX_SIZE  2048

/* 由你提供的 SPI2 单字节收发 */
extern u8 SPI2_ReadWriteByte(u8 TxData);





