///ys 只要我这里拦截到数据包是UDP且端口大于3K的，那就ARP它，ARP会持继2分钟
#include <libnet.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <net/ethernet.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <mysql/mysql.h>
#include "packet_info.h"
#define UPDATE_FLOW_MAC "UPDATE `flow_statistics` set `Mac`='%x:%x:%x:%x:%x:%x' where `User_IP`='%s';"


u_char mac[6]; ///ys 好像没用了
char mac_2_ip[32];///ys MAC 对应 的IP
char db_ip[24];///ys 被欺骗的目标IP
int ip1,ip2,ip3,ip4;///ys 以IP1.IP2.IP3.IP4这样的型式来拿IP字符串每个字段的值
struct arp_user user_list[255];///ys 需要ARP的用户
extern char my_device[64];///ys 监听网卡
//extern u_int8_t arp_mac[6];
extern  unsigned char arp_mac2[6];///ys 假网关mac
unsigned int arp_mac_wrong[6]= {0x11,0x11,0x11,0x11,0x11,0x11}; ///ys 假网关mac
extern char arp_source[32];///ys 假网关IP
char ex_ip[24];///ys VIPIP1，不会被拦截的IP，叫VIPIP
char ex_ip1[24];///ys VIPIP2
char ex_ip2[24];///ys VIPIP3
extern char  net_addr [8];///YS 子网
extern char server_ip[32];
extern char localip[32];
extern MYSQL mysql;
/**********************
该函数用于把需求ARP的目标IP加入到我们的APR_USRR_LIST里面，参数1"const u_char *my_packet",是我们通过LIBPCAP的抓到的原始的包
**********************/
int get_arp_user_list_from_db(MYSQL *sock)
{
    #define usre_flow_list "select User_IP,Mac from flow_statistics where Block=1"
    MYSQL_ROW row;
    MYSQL_RES *res;
    if(mysql_query(sock,usre_flow_list))
    {
        fprintf(stderr,"Query failed (%s)\n",mysql_error(sock));
        exit(1);
    }

    if (!(res=mysql_store_result(sock)))
    {
        fprintf(stderr,"Couldn't get result from %s", mysql_error(sock));
        exit(1);
    }
    while ((row = mysql_fetch_row(res)))///YS 读数据库查找到的结果
    {
        printf("aaaaa  >>>  %s   %s \n",row[0],row[1]);
        sscanf(row[0],"%d.%d.%d.%d",&ip1,&ip2,&ip3,&ip4);///YS 获取他的MAC
        //if(strlen(user_list[ip4].ip)<3||user_list[ip4].count>33 && user_list[ip4].flag==0)
        if(user_list[ip4].flag==0)///YS 如果还没发过ARP到这个IP的话,就把这个IP加入到我们的ARP_USR_LIST里,并且发送次数为1,已经加入的标志为1
        {
            printf("insert arp_usr_list   %s    %d\n",row[0],ip4);
            user_list[ip4].count=1;
            user_list[ip4].flag=1;
            user_list[ip4].from=4;
            sscanf(row[1],"%x:%x:%x:%x:%x:%x",
                   & user_list[ip4].mac[0],& user_list[ip4].mac[1],& user_list[ip4].mac[2],& user_list[ip4].mac[3]
                   ,& user_list[ip4].mac[4],& user_list[ip4].mac[5]);
        }
        sprintf(user_list[ip4].ip,"%s",row[0]);
    }
    mysql_free_result(res);
    return 0;
}




/**********************
该函数用于把需求ARP的目标IP加入到我们的APR_USRR_LIST里面，参数1"const u_char *my_packet",是我们通过LIBPCAP的抓到的原始的包
**********************/
int creat_arp_user_list(struct arp_user *arp_user_tmp)
{
    int j;
    char mac_sql[1024]={'\0'};
    sscanf(arp_user_tmp->ip,"%d.%d.%d.%d",&ip1,&ip2,&ip3,&ip4);///YS 获取他的MAC
    if(user_list[ip4].count==0)///YS 如果还没发过ARP到这个IP的话,就把这个IP加入到我们的ARP_USR_LIST里,并且发送次数为1,已经加入的标志为1
    {
        printf("insert arp_usr_list   %s  \n",arp_user_tmp->ip);
        user_list[ip4].count=1;
        user_list[ip4].flag=1;

        sprintf(user_list[ip4].ip,"%s",arp_user_tmp->ip);
        for(j=0; j<6; j++)
        {
            user_list[ip4].mac[j]=arp_user_tmp->mac[j];
            printf("%x", user_list[ip4].mac[j]);
        }
        snprintf(mac_sql,sizeof(mac_sql),UPDATE_FLOW_MAC,arp_user_tmp->mac[0],arp_user_tmp->mac[1],arp_user_tmp->mac[2],arp_user_tmp->mac[3],arp_user_tmp->mac[4],arp_user_tmp->mac[5],arp_user_tmp->ip);
        sql_put_mac_to_flow_table(mac_sql,&mysql);
        printf("\n%s\n",mac_sql);
    }
    else
    {
        printf("count !=0  %d\n",user_list[ip4].count);
    }
    return 0;
}

/*******************
arp欺骗包发送函数,从apr_user_list里拿IP然后去欺骗，完了后就加1，直到发了33个包后，再看看对方有没有继续发过高端口的UDP包，有的话，继续
***********************/
void arp_spoofind( )
{

	return 1;
    libnet_t *handle=NULL;        /* Libnet句柄 */
    int packet_size;
    u_int32_t dst_ip, src_ip;                /* 网路序的目的IP和源IP */
    char error[LIBNET_ERRBUF_SIZE];        /* 出错信息 */
    libnet_ptag_t arp_proto_tag, eth_proto_tag;



    int i=0;
    while(1)
    {
        ///YS 每5秒钟发一次ARP
        sleep(5);

        for(i=0; i<255; i++)
        {
            if(user_list[i].flag!=1)
            {
                continue;
            }

            printf("pass  arp>>>  %s   %d\n",user_list[i].ip,user_list[i].count);
            /* 把目的IP地址字符串转化成网络序 */
            dst_ip = libnet_name2addr4(handle, user_list[i].ip, LIBNET_RESOLVE);
            /* 把源IP地址字符串转化成网络序 */
            src_ip = libnet_name2addr4(handle, arp_source, LIBNET_RESOLVE);

            if ( dst_ip == -1 || src_ip == -1 )
            {
                printf("ip address convert error\n");
                continue;
            }

            /* 初始化Libnet,注意第一个参数和TCP初始化不同 */
            if ( (handle = libnet_init(LIBNET_LINK_ADV, my_device, error)) == NULL )
            {
                printf("158   %s\n",my_device);
                continue;
            }

            if(user_list[i].from==4)
            {

                /* 构造arp协议块 */
                arp_proto_tag = libnet_build_arp(
                                    ARPHRD_ETHER,        /* 硬件类型,1表示以太网硬件地址 */
                                    ETHERTYPE_IP,        /* 0x0800表示询问IP地址 */
                                    6,                    /* 硬件地址长度 */
                                    4,                    /* IP地址长度 */
                                    ARPOP_REPLY,        /* 操作方式:ARP请求 */
                                    arp_mac_wrong,                /* source MAC addr */
                                    (u_int8_t *)&src_ip,    /* src proto addr */
                                    user_list[i].mac,                /* dst MAC addr */
                                    (u_int8_t *)&dst_ip,    /* dst IP addr */
                                    NULL,                /* no payload */
                                    0,                    /* payload length */
                                    handle,                /* libnet tag */
                                    0                    /* Create new one */
                                );
            }
            else
            {

                /* 构造arp协议块 */
                arp_proto_tag = libnet_build_arp(
                                    ARPHRD_ETHER,        /* 硬件类型,1表示以太网硬件地址 */
                                    ETHERTYPE_IP,        /* 0x0800表示询问IP地址 */
                                    6,                    /* 硬件地址长度 */
                                    4,                    /* IP地址长度 */
                                    ARPOP_REPLY,        /* 操作方式:ARP请求 */
                                    arp_mac2,                /* source MAC addr */
                                    (u_int8_t *)&src_ip,    /* src proto addr */
                                    user_list[i].mac,                /* dst MAC addr */
                                    (u_int8_t *)&dst_ip,    /* dst IP addr */
                                    NULL,                /* no payload */
                                    0,                    /* payload length */
                                    handle,                /* libnet tag */
                                    0                    /* Create new one */
                                );
            }
            if (arp_proto_tag == -1)
            {
                printf("179\n");
                exit(-3);
            };



            if(user_list[i].from==4)
            {

                /* 构造一个以太网协议块*/
                eth_proto_tag = libnet_build_ethernet(
                                    user_list[i].mac,            /* 以太网目的地址 */
                                    arp_mac_wrong,            /* 以太网源地址 */
                                    ETHERTYPE_ARP,    /* 以太网上层协议类型，此时为ARP请求 */
                                    NULL,            /* 负载，这里为空 */
                                    0,                /* 负载大小 */
                                    handle,            /* Libnet句柄 */
                                    0                /* 协议块标记，0表示构造一个新的 */
                                );

            }
            else
            {
                /* 构造一个以太网协议块*/
                eth_proto_tag = libnet_build_ethernet(
                                    user_list[i].mac,            /* 以太网目的地址 */
                                    arp_mac2,            /* 以太网源地址 */
                                    ETHERTYPE_ARP,    /* 以太网上层协议类型，此时为ARP请求 */
                                    NULL,            /* 负载，这里为空 */
                                    0,                /* 负载大小 */
                                    handle,            /* Libnet句柄 */
                                    0                /* 协议块标记，0表示构造一个新的 */
                                );

            }
            if (eth_proto_tag == -1)
            {
                printf("build eth_header failure\n");
            }

            packet_size = libnet_write(handle);/* 发送arp欺骗广播 */
            printf("ARP SPOOFIND SENT  %x  %x  %x %x  %x\n",arp_mac2[0],arp_mac2[1],arp_mac2[2],arp_mac2[3],arp_mac2[4]);

            user_list[i].count++;
            if( user_list[i].from==4)
            {
                user_list[i].count=3;
            }
            else

                if(user_list[i].count>33 )
                {
                    user_list[i].flag=0;
                    user_list[i].count=0;
                }


        }

    }

    libnet_destroy(handle);                /* 释放句柄 */
}
