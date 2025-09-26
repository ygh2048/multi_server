/**
  ******************************************************************************
  * @file    main.c
  * @author  WIZnet Software Team
  * @version V1.0
  * @date    2015-02-14
  * @brief   W5500多连接TCPServer
  ******************************************************************************
  * @attention
  *
  * 硬件平台: 基于STM32 F407开发板  
  * 论坛    : http://www.firebbs.cn
  * 淘宝    : https://fire-stm32.taobao.com
  *
  ******************************************************************************
  */
 
#include <string.h> 
#include "stm32f4xx.h"
#include "./usart/bsp_usart_debug.h"

#include "delay.h"
#include "spi.h"
#include "socket.h"	


/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
#define MAX_CLIENTS 3  // 最大客户端数量
#define SOCKET_BASE 0  // 起始socket编号
#define DATA_BUF_SIZE 2048
#define LINK_CHECK_INTERVAL 600 // 连接状态检查间隔(ms)

/* Private macro -------------------------------------------------------------*/
// 为每个客户端分配独立的缓冲区
uint8_t gDATABUF[MAX_CLIENTS][DATA_BUF_SIZE];

// 跟踪每个socket的上一次状态
uint8_t prev_socket_status[MAX_CLIENTS] = {0};

uint32_t last_link_check_time = 0;
uint8_t last_phy_link_status = 0;

// <<< 新增：系统滴答计数器（记录启动后毫秒数）>>>
volatile uint32_t g_SystemTick = 0;

// 网络配置信息
wiz_NetInfo gWIZNETINFO = { 
    .mac = {0x00, 0x08, 0xdc, 0x11, 0x11, 0x11},
    .ip = {192, 168, 103, 202},
    .sn = {255, 255, 255, 0},
    .gw = {192, 168, 103, 1},
    .dns = {8, 8, 8, 8},
    .dhcp = NETINFO_STATIC 
};

/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/
void platform_init(void);
void network_init(void);
uint8_t check_phy_link_status(void);
void handle_link_status_change(uint8_t current_status);
// <<< 新增：获取系统滴答（等效HAL_GetTick()）>>>
uint32_t SysTick_GetTick(void);
// <<< 新增：SysTick中断服务函数声明>>>
void SysTick_Handler(void);

/****************************************************
函数名称：main
功能描述：主函数
输入参数：无
返回值：无
****************************************************/
int main(void)
{
    uint16_t len = 0;
    uint8_t memsize[2][8] = {{2, 2, 2, 2, 2, 2, 2, 2}, {2, 2, 2, 2, 2, 2, 2, 2}};
    
    // 平台初始化（已包含SysTick初始化）
    platform_init();
    
    // 注册WIZCHIP的CRITICAL SECTION函数
    reg_wizchip_cris_cbfunc(SPI_CrisEnter, SPI_CrisExit);
    
    // 注册CS函数
#if _WIZCHIP_IO_MODE_ == _WIZCHIP_IO_MODE_SPI_VDM_
    reg_wizchip_cs_cbfunc(SPI_CS_Select, SPI_CS_Deselect);
#elif _WIZCHIP_IO_MODE_ == _WIZCHIP_IO_MODE_SPI_FDM_
    reg_wizchip_cs_cbfunc(SPI_CS_Select, SPI_CS_Deselect);
#else
    #if (_WIZCHIP_IO_MODE_ & _WIZCHIP_IO_MODE_SIP_) != _WIZCHIP_IO_MODE_SIP_
        #error "Unknown _WIZCHIP_IO_MODE_"
    #else
        reg_wizchip_cs_cbfunc(wizchip_select, wizchip_deselect);
    #endif
#endif
    
    // 注册SPI读写函数
    reg_wizchip_spi_cbfunc(SPI_ReadByte, SPI_WriteByte);
    
    // 初始化WIZCHIP
    if(ctlwizchip(CW_INIT_WIZCHIP, (void*)memsize) == -1) {
        printf("WIZCHIP Initialized fail.\r\n");
        while(1);
			
    }
    
    // 检查PHY连接状态
    last_phy_link_status = check_phy_link_status();
    printf("Initial PHY Link Status: %s\r\n", last_phy_link_status ? "Connected" : "Disconnected");
    
    // 网络初始化
    network_init();
    
    // 初始化所有socket
    for(int i = 0; i < MAX_CLIENTS; i++) {
        socket(i, Sn_MR_TCP, 5000 + i, 0x00);
        listen(i);
        prev_socket_status[i] = getSn_SR(i); // 记录初始状态
        printf("Socket %d listening on port %d\r\n", i, 5000 + i);
    }
    
    // 主循环
    while(1) {
            if(SysTick_GetTick() - last_link_check_time > LINK_CHECK_INTERVAL) {
                uint8_t current_status = check_phy_link_status();
                
                // 增加状态变化的判断条件
                if(current_status != last_phy_link_status || !current_status) {
                    handle_link_status_change(current_status);
                    last_phy_link_status = current_status;
                    
                    // 强制重新检查网络状态
                    if(!current_status) {
                        delay_ms(20);
                        current_status = check_phy_link_status();
                        if(current_status) {
                            handle_link_status_change(current_status);
                            last_phy_link_status = current_status;
                        }
                    }
                }
                last_link_check_time = SysTick_GetTick();
            }
                    
        for(int i = 0; i < MAX_CLIENTS; i++) {
            uint8_t current_status = getSn_SR(i);
            
            // 只有当状态发生变化时才打印消息
            if(current_status != prev_socket_status[i]) {
                switch(current_status) {
                    case SOCK_INIT:
                        printf("Socket %d initialized\r\n", i);
                        listen(i);
                        break;
                        
                    case SOCK_ESTABLISHED:
                        printf("Client connected to socket %d\r\n", i);
                        break;
                        
                    case SOCK_CLOSE_WAIT:
                        printf("Client disconnected from socket %d\r\n", i);
                        disconnect(i);
                        break;
                        
                    case SOCK_CLOSED:
                        printf("Socket %d closed, reinitializing...\r\n", i);
                        socket(i, Sn_MR_TCP, 5000 + i, 0x00);这
                        listen(i);
                        printf("Socket %d reinitialized and listening\r\n", i);
                        break;
                        
                    default:
                        break;
                }
                
                // 更新状态记录
                prev_socket_status[i] = current_status;
            }
            
            // 处理已建立连接的数据传输
            if(current_status == SOCK_ESTABLISHED) {
                if(getSn_IR(i) & Sn_IR_CON) {
                    setSn_IR(i, Sn_IR_CON);
                }
                
                // 检查是否有数据到达
                len = getSn_RX_RSR(i);
                if(len > 0) {
                    len = recv(i, gDATABUF[i], len);
                    if(len > 0) {
                        printf("Received from socket %d: %s\r\n", i, gDATABUF[i]);
                        send(i, gDATABUF[i], len);
                    }
                }
            }
        }
        
        // 添加小延迟以减少CPU使用率
        delay_ms(10);
    }
}

/**
  * @brief  检查PHY连接状态
  * @retval 连接状态 (1:已连接, 0:未连接)
  */
uint8_t check_phy_link_status(void)
{
    uint8_t link_status;
    uint8_t retry = 3;  // 添加重试机制
    
    while(retry--) {
        ctlwizchip(CW_GET_PHYLINK, &link_status);
        if(link_status != 0xFF) {  // 有效状态
            return link_status;
        }
        delay_ms(1);  // 短暂延时后重试
    }
    
    // 如果多次读取失败，尝试重新初始化PHY
    wizphy_reset();
    delay_ms(20);  // 等待PHY重置完成
    
    ctlwizchip(CW_GET_PHYLINK, &link_status);
    return link_status;
}

/**
  * @brief  处理连接状态变化
  * @param  current_status 当前连接状态
  * @retval None
  */
void handle_link_status_change(uint8_t current_status)
{
    static uint8_t reconnect_count = 0;
    
    if(current_status) {
        printf("Network cable connected\r\n");
        reconnect_count = 0;  // 重置重连计数
        
        // 重新初始化PHY
        wizphy_reset();
        delay_ms(10);
        
        // 重新初始化网络配置
        network_init();
        
        // 重新初始化所有socket
        for(int i = 0; i < MAX_CLIENTS; i++) {
            close(i);
            delay_ms(10);  // 添加短暂延时确保关闭完成
            socket(i, Sn_MR_TCP, 5000 + i, 0x00);
            listen(i);
            prev_socket_status[i] = getSn_SR(i);
            printf("Socket %d reinitialized after cable reconnect\r\n", i);
        }
    } else {
        reconnect_count++;
        printf("Network cable disconnected (count: %d)\r\n", reconnect_count);
        
        // 关闭所有socket并重置状态
        for(int i = 0; i < MAX_CLIENTS; i++) {
            disconnect(i);
            delay_ms(5);
            close(i);
            prev_socket_status[i] = SOCK_CLOSED;
            printf("Socket %d closed due to cable disconnect\r\n", i);
        }
        
        // 执行完整的芯片重置
        if(reconnect_count > 1) {
            printf("Performing full chip reset...\r\n");
            wizchip_sw_reset();
            delay_ms(5);
            // 重新初始化WIZCHIP
            uint8_t memsize[2][8] = {{2,2,2,2,2,2,2,2}, {2,2,2,2,2,2,2,2}};
            ctlwizchip(CW_INIT_WIZCHIP, (void*)memsize);
        }
    }
}

/**
  * @brief  初始化网络信息
  * @retval None
  */
void network_init(void)
{
    uint8_t tmpstr[6];
    ctlnetwork(CN_SET_NETINFO, (void*)&gWIZNETINFO);
    ctlnetwork(CN_GET_NETINFO, (void*)&gWIZNETINFO);
    
    // 显示网络信息
    ctlwizchip(CW_GET_ID, (void*)tmpstr);
    printf("\r\n=== %s NET CONF ===\r\n", (char*)tmpstr);
    printf("MAC: %02X:%02X:%02X:%02X:%02X:%02X\r\n", 
           gWIZNETINFO.mac[0], gWIZNETINFO.mac[1], gWIZNETINFO.mac[2],
           gWIZNETINFO.mac[3], gWIZNETINFO.mac[4], gWIZNETINFO.mac[5]);
    printf("SIP: %d.%d.%d.%d\r\n", 
           gWIZNETINFO.ip[0], gWIZNETINFO.ip[1], gWIZNETINFO.ip[2], gWIZNETINFO.ip[3]);
    printf("GAR: %d.%d.%d.%d\r\n", 
           gWIZNETINFO.gw[0], gWIZNETINFO.gw[1], gWIZNETINFO.gw[2], gWIZNETINFO.gw[3]);
    printf("SUB: %d.%d.%d.%d\r\n", 
           gWIZNETINFO.sn[0], gWIZNETINFO.sn[1], gWIZNETINFO.sn[2], gWIZNETINFO.sn[3]);
    printf("DNS: %d.%d.%d.%d\r\n", 
           gWIZNETINFO.dns[0], gWIZNETINFO.dns[1], gWIZNETINFO.dns[2], gWIZNETINFO.dns[3]);
    printf("======================\r\n");
}

/**
  * @brief  平台初始化（包含SysTick初始化）
  * @retval None
  */
void platform_init(void)
{
    Debug_USART_Config();
    SPI_Configuration();
    delay_init(168);
    // <<< 新增：初始化SysTick（1ms中断一次，基于168MHz系统时钟）>>>
    // 公式：SysTick_Config(系统时钟频率 / 1000) → 1ms中断
    SysTick_Config(SystemCoreClock / 1000);
	
	
	
}



// <<< 新增：获取系统启动后的毫秒数（等效HAL_GetTick()）>>>
uint32_t SysTick_GetTick(void)
{
    return g_SystemTick;
}

/*********************************************END OF FILE**********************/

