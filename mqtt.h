#ifndef MQTT_H
#define __MQTT_H__

#define PARA_SIZE 64
#define BUFF_SIZE 256
#define TOPIC_SIZE 128
#define DATA_SIZE 128
typedef struct {
    unsigned int Fixed_len; //固定报头长度
    unsigned int Var_len; //可变报头长度
    unsigned int Payload_len; //有效载荷长度
    unsigned int Remain_len; //剩余长度
    unsigned int MessageID; //报文标识符
    unsigned int len; //报文总长度
    unsigned char buff[BUFF_SIZE]; //报文内容
    char ClientID[PARA_SIZE]; //客户端ID
    char Username[PARA_SIZE]; //用户名
    char Password[PARA_SIZE]; //密码
    char WillTopic[PARA_SIZE]; //遗嘱主题
    char WillData[PARA_SIZE]; //遗嘱消息
    char topic[TOPIC_SIZE]; //接收的主题
    unsigned char data[DATA_SIZE]; //接收的数据
}MQTT_CB;

extern MQTT_CB mqtt;
void MQTT_Init(void);
void MQTT_Connect(unsigned int keepalive);
void MQTT_ConnectWILL(unsigned int keepalive,unsigned char willretain,unsigned char willqs,unsigned char willclean);
char MQTT_ConnectACK(unsigned char *rxdata,unsigned int rxdata_len);
void MQTT_DISCONNECT(void);
void MQTT_SUBSCRIBE(char *topic,char Qs);
char MQTT_SUBACK(unsigned char *rxdata,unsigned int rxdata_len);
void MQTT_UNSUBSCRIBE(char *topic);
char MQTT_UNSUBACK(unsigned char *rxdata,unsigned int rxdata_len);
void MQTT_PINGREQ(void);
char MQTT_PINGRESP(unsigned char *rxdata,unsigned int rxdata_len);
void MQTT_PUBLISH0(char retain,char *topic,unsigned char *data,unsigned int data_len);
void MQTT_PUBLISH1(char dup,char retain,char *topic,unsigned char *data,unsigned int data_len);
void MQTT_PUBLISH2(char dup,char retain,char *topic,unsigned char *data,unsigned int data_len);
char MQTT_ProcessPUBLISH(unsigned char *rxdata, unsigned int rxdata_len, unsigned char *qs, unsigned int *messageid);
void MQTT_PUBACK(unsigned int messageid);
char MQTT_ProcessPUBACK(unsigned char *rxdata, unsigned int rxdata_len, unsigned int *messageid);
void MQTT_PUBREC(unsigned int messageid);
char MQTT_ProcessPUBREC(unsigned char *rxdata, unsigned int rxdata_len, unsigned int *messageid);
void MQTT_PUBREL(unsigned int messageid);
char MQTT_ProcessPUBREL(unsigned char *rxdata, unsigned int rxdata_len, unsigned int *messageid);
void MQTT_PUBCOMP(unsigned int messageid);
char MQTT_ProcessPUBCOMP(unsigned char *rxdata, unsigned int rxdata_len, unsigned int *messageid);
#endif
