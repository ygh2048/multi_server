#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "socket.h"
#include "w5100s.h"
#include "arp.h"
#include "pico/time.h"

ARPMSG pARPMSG;
ARPMSG *aAPRMSG;

/**
 *@brief  16-bit character high-8-bit low-8-bit conversion
 *@param  i:The data to be converted
 *@return The converted data
 */
static uint16_t swaps(uint16_t i)
{
    uint16_t ret = 0;
    ret = (i & 0xFF) << 8;
    ret |= ((i >> 8) & 0xFF);
    return ret;
}

/**
 *@brief  Converts a host-mode unsigned short data to big-endian TCP/IP network byte format data.
 *@param  The data to be converted
 *@return Big-endian data
 */
static uint16_t htons(uint16_t hostshort)
{
    /**< A 16-bit number in host byte order.  */
#if (SYSTEM_ENDIAN == _ENDIAN_LITTLE_)
    return swaps(hostshort);
#else
    return hostshort;
#endif
}

/**
 * @brief   send arp request
 * @param   sn: socket number
 * @param   port: local socket port
 * @param   dest_ip: ARP Destination IP address
 * @return  none
 */
void arp_request(uint8_t sn, uint16_t port, uint8_t *dest_ip)
{
    uint16_t i;
    uint8_t broadcast_addr[4] = {0xff, 0xff, 0xff, 0xff};

    for (i = 0; i < 6; i++)
    {
        pARPMSG.dst_mac[i] = 0xff; // Broadcast address in an Ethernet frame
        pARPMSG.tgt_mac[i] = 0x00;
        pARPMSG.tgt_ip[i] = dest_ip[i];
    }
    getSHAR(pARPMSG.src_mac);            // get local mac address.
    getSIPR(pARPMSG.sender_ip);          // get local ip address.
    pARPMSG.msg_type = htons(ARP_TYPE);  // ARP type
    pARPMSG.hw_type = htons(ETHER_TYPE); // Ethernet type
    pARPMSG.pro_type = htons(PRO_TYPE);  // IP
    pARPMSG.hw_size = HW_SIZE;           // 6
    pARPMSG.pro_size = PRO_SIZE;         // 4
    pARPMSG.opcode = htons(ARP_REQUEST); // request: 0x0001;  reply: 0x0002

    if (sendto(sn, (uint8_t *)&pARPMSG, sizeof(pARPMSG), broadcast_addr, port) != sizeof(pARPMSG))
    {
        printf("Fail to send arp request packet.\r\n");
    }
    else
    {
        if (pARPMSG.opcode == htons(ARP_REPLY))
        {
            printf("Who has %d.%d.%d.%d ?  Tell %d.%d.%d.%d\r\n", pARPMSG.tgt_ip[0], pARPMSG.tgt_ip[1], pARPMSG.tgt_ip[2], pARPMSG.tgt_ip[3],
                   pARPMSG.sender_ip[0], pARPMSG.sender_ip[1], pARPMSG.sender_ip[2], pARPMSG.sender_ip[3]);
        }
        else
        {
            // printf("Opcode has wrong value. check opcode!\r\n");
        }
    }
}

/**
 * @brief   ARP reply process
 * @param   sn: socket number
 * @param   buff: The cache that accepts data
 * @param   rlen: The length of the received data
 * @return  none
 */
void arp_reply(uint8_t sn, uint8_t *buff, uint16_t rlen)
{
    uint8_t destip[4];
    uint16_t destport;
    uint8_t ret_arp_reply[128];

    recvfrom(sn, (uint8_t *)buff, rlen, destip, &destport);

    if (buff[12] == ARP_TYPE_HI && buff[13] == ARP_TYPE_LO)
    {
        aAPRMSG = (ARPMSG *)buff;
        if ((aAPRMSG->opcode) == htons(ARP_REPLY))
        {
            memset(ret_arp_reply, 0x00, 128);
            sprintf((int8_t *)ret_arp_reply, "%d.%d.%d.%d is at %x.%x.%x.%x.%x.%x\r\n",
                    aAPRMSG->sender_ip[0], aAPRMSG->sender_ip[1], aAPRMSG->sender_ip[2], aAPRMSG->sender_ip[3],
                    aAPRMSG->sender_mac[0], aAPRMSG->sender_mac[1], aAPRMSG->sender_mac[2], aAPRMSG->sender_mac[3],
                    aAPRMSG->sender_mac[4], aAPRMSG->sender_mac[5]);

            printf("%d.%d.%d.%d is at %x.%x.%x.%x.%x.%x\r\n",
                   aAPRMSG->sender_ip[0], aAPRMSG->sender_ip[1], aAPRMSG->sender_ip[2], aAPRMSG->sender_ip[3],
                   aAPRMSG->sender_mac[0], aAPRMSG->sender_mac[1], aAPRMSG->sender_mac[2], aAPRMSG->sender_mac[3],
                   aAPRMSG->sender_mac[4], aAPRMSG->sender_mac[5]);
        }
        else if ((aAPRMSG->opcode) == htons(ARP_REQUEST))
        {
            printf("Who has %d.%d.%d.%d ? Tell %x.%x.%x.%x.%x.%x\r\n",
                   aAPRMSG->tgt_ip[0], aAPRMSG->tgt_ip[1], aAPRMSG->tgt_ip[2], aAPRMSG->tgt_ip[3],
                   aAPRMSG->sender_mac[0], aAPRMSG->sender_mac[1], aAPRMSG->sender_mac[2], aAPRMSG->sender_mac[3],
                   aAPRMSG->sender_mac[4], aAPRMSG->sender_mac[5]);
        }
    }
    else
    {
        printf("This message is not ARP reply: opcode is not ox02!\r\n");
    }
}

/**
 * @brief   send arp request
 * @param   sn: socket number
 * @param   buff: The cache that accepts data
 * @param   dest_ip: ARP Destination IP address
 * @return  none
 */
void do_arp(uint8_t sn, uint8_t *buf, uint8_t *dest_ip)
{
    uint16_t rlen = 0;
    uint16_t local_port = 5000;

    switch (getSn_SR(sn))
    {
    case SOCK_CLOSED:
        close(sn);
        socket(sn, Sn_MR_MACRAW, local_port, 0x00);
        while (getSn_SR(sn) != SOCK_MACRAW)
        {
            sleep_ms(100);
        }
    case SOCK_MACRAW:
        arp_request(sn, local_port, dest_ip);
        rlen = getSn_RX_RSR(sn);
        if ((rlen = getSn_RX_RSR(sn)) > 0)
        {
            arp_reply(sn, buf, rlen);
        }
        sleep_ms(1000);
        break;
    }
}