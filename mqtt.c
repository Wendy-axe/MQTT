/**
 * @file mqtt.c
 * @brief MQTT 3.1.1协议客户端实现
 * 
 * 本文件实现了MQTT 3.1.1协议的14种控制报文，包括：
 * - CONNECT/CONNACK：连接建立与确认
 * - PUBLISH/PUBACK/PUBREC/PUBREL/PUBCOMP：消息发布与确认
 * - SUBSCRIBE/SUBACK：订阅与确认
 * - UNSUBSCRIBE/UNSUBACK：取消订阅与确认
 * - PINGREQ/PINGRESP：心跳请求与响应
 * - DISCONNECT：断开连接
 * 
 * 采用变长编码(VB)表示剩余长度，支持QoS 0/1/2三种服务质量等级
 */
#include "mqtt.h"
#include "string.h"
#include "stdio.h"

MQTT_CB mqtt;  // MQTT控制块全局变量

/**
 * @brief MQTT客户端初始化函数
 * 
 * 初始化MQTT控制块，设置默认的ClientID、Username、Password、WillTopic和WillData
 * 初始化报文标识符(MessageID)为1
 */
void MQTT_Init(void)
{
    memset(&mqtt, 0, sizeof(mqtt));  // 清空MQTT控制块
    sprintf(mqtt.ClientID,"USER001");  // 默认客户端ID
    sprintf(mqtt.Username,"USER001");  // 默认用户名
    sprintf(mqtt.Password,"USER001");  // 默认密码
    sprintf(mqtt.WillTopic,"WILL001"); // 默认遗嘱主题
    sprintf(mqtt.WillData,"WILL001");  // 默认遗嘱消息
    mqtt.MessageID = 1; // 报文标识符初始值为1（非零）
}

/**
 * @brief 构建MQTT CONNECT报文（无遗嘱）
 * 
 * 构建标准的MQTT连接请求报文，包含ClientID、Username和Password
 * 不包含遗嘱(Will)信息
 * 
 * @param keepalive 心跳保活时间（秒），0表示禁用
 * 
 * CONNECT报文结构：
 * 固定头部：0x10 (CONNECT) + 剩余长度(变长编码)
 * 可变头部：协议名("MQTT") + 协议级别(4) + 连接标志(0xC2) + 保活时间
 * 载荷：ClientID + Username + Password
 * 
 * 连接标志0xC2 = 0b11000010：
 * - bit7=1: Username标志
 * - bit6=1: Password标志
 * - bit5=0: Will标志（无遗嘱）
 * - bit1=1: Clean Session
 */
void MQTT_Connect(unsigned int keepalive)
{
	mqtt.Fixed_len = 1;  // 固定头部初始长度（控制报文类型）
	mqtt.Var_len = 10;  // 可变头部长度：协议名(6) + 协议级别(1) + 标志(1) + 保活(2)
	// 载荷长度：ClientID(2+len) + Username(2+len) + Password(2+len)
	mqtt.Payload_len = 2 + strlen(mqtt.ClientID) + 2 + strlen(mqtt.Username) + 2 + strlen(mqtt.Password);
	mqtt.Remain_len = mqtt.Var_len + mqtt.Payload_len;   // 剩余长度 = 可变头部 + 载荷
	
	mqtt.buff[0] = 0x10;  // 控制报文类型：CONNECT (0x10)
	
	// 变长编码写入剩余长度（最多4字节）
	do{
		if(mqtt.Remain_len/128 == 0){     // 不需要进位（值<128）
			mqtt.buff[mqtt.Fixed_len] = mqtt.Remain_len; 
		}else{                               // 需要进位，最高位置1
			mqtt.buff[mqtt.Fixed_len] = (mqtt.Remain_len%128)|0x80;  
		} 
		mqtt.Fixed_len++;
		mqtt.Remain_len = mqtt.Remain_len/128;  // 右移7位
	}while(mqtt.Remain_len);	

	// 可变头部：协议名 "MQTT" (0x00 0x04 'M' 'Q' 'T' 'T')
	mqtt.buff[mqtt.Fixed_len] = 0x00;      // 协议名长度高字节
	mqtt.buff[mqtt.Fixed_len+1] = 0x04;    // 协议名长度低字节
	mqtt.buff[mqtt.Fixed_len+2] = 0x4D;    // 'M'
	mqtt.buff[mqtt.Fixed_len+3] = 0x51;    // 'Q'
	mqtt.buff[mqtt.Fixed_len+4] = 0x54;    // 'T'
	mqtt.buff[mqtt.Fixed_len+5] = 0x54;    // 'T'
	mqtt.buff[mqtt.Fixed_len+6] = 0x04;    // 协议级别：MQTT 3.1.1
	mqtt.buff[mqtt.Fixed_len+7] = 0xC2;    // 连接标志：Username+Password+CleanSession
	mqtt.buff[mqtt.Fixed_len+8] = keepalive/256;  // 保活时间高字节
	mqtt.buff[mqtt.Fixed_len+9] = keepalive%256;  // 保活时间低字节
	
	// 载荷：ClientID
	mqtt.buff[mqtt.Fixed_len+10] = strlen(mqtt.ClientID)/256;          // 长度高字节
	mqtt.buff[mqtt.Fixed_len+11] = strlen(mqtt.ClientID)%256;          // 长度低字节
	memcpy(&mqtt.buff[mqtt.Fixed_len+12],mqtt.ClientID,strlen(mqtt.ClientID));
	
	// 载荷：Username
	mqtt.buff[mqtt.Fixed_len+12+strlen(mqtt.ClientID)] = strlen(mqtt.Username)/256;
	mqtt.buff[mqtt.Fixed_len+13+strlen(mqtt.ClientID)] = strlen(mqtt.Username)%256;
	memcpy(&mqtt.buff[mqtt.Fixed_len+14+strlen(mqtt.ClientID)],mqtt.Username,strlen(mqtt.Username));	
	
	// 载荷：Password
	mqtt.buff[mqtt.Fixed_len+14+strlen(mqtt.ClientID)+strlen(mqtt.Username)] = strlen(mqtt.Password)/256;
	mqtt.buff[mqtt.Fixed_len+15+strlen(mqtt.ClientID)+strlen(mqtt.Username)] = strlen(mqtt.Password)%256;
	memcpy(&mqtt.buff[mqtt.Fixed_len+16+strlen(mqtt.ClientID)+strlen(mqtt.Username)],mqtt.Password,strlen(mqtt.Password));	
	
	mqtt.len = mqtt.Fixed_len + mqtt.Var_len + mqtt.Payload_len;  // 计算总长度
}  

/**
 * @brief 构建MQTT CONNECT报文（带遗嘱）
 * 
 * 构建包含遗嘱(Will)信息的MQTT连接请求报文
 * 遗嘱消息会在客户端异常断开连接时由服务器自动发布
 * 
 * @param keepalive   心跳保活时间（秒）
 * @param willretain  遗嘱消息是否保留(0/1)
 * @param willqs      遗嘱消息的QoS级别(0/1/2)
 * @param willclean   Clean Session标志(0/1)
 * 
 * 连接标志格式(0xC4为基础)：
 * - bit7=1: Username标志
 * - bit6=1: Password标志
 * - bit5: Will Retain
 * - bit4-3: Will QoS
 * - bit2=1: Will标志（有遗嘱）
 * - bit1: Clean Session
 */
void MQTT_ConnectWILL(unsigned int keepalive,unsigned char willretain,unsigned char willqs,unsigned char willclean)
{
	mqtt.Fixed_len = 1;  // 固定头部初始长度
	mqtt.Var_len = 10;  // 可变头部长度
	// 载荷长度：ClientID + WillTopic + WillData + Username + Password
	mqtt.Payload_len = 2 + strlen(mqtt.ClientID) + 2 + strlen(mqtt.Username) + 2 + strlen(mqtt.Password) 
			+ 2 + strlen(mqtt.WillTopic) + 2 + strlen(mqtt.WillData);
	mqtt.Remain_len = mqtt.Var_len + mqtt.Payload_len;
	
	mqtt.buff[0] = 0x10;  // CONNECT控制报文类型
	
	// 变长编码写入剩余长度
	do{
		if(mqtt.Remain_len/128 == 0){     // 不需要进位
			mqtt.buff[mqtt.Fixed_len] = mqtt.Remain_len;
		}else{                               // 需要进位
			mqtt.buff[mqtt.Fixed_len] = (mqtt.Remain_len%128)|0x80;
		}
		mqtt.Fixed_len++;
		mqtt.Remain_len = mqtt.Remain_len/128;
	}while(mqtt.Remain_len);	

	// 可变头部：协议名 "MQTT"
	mqtt.buff[mqtt.Fixed_len] = 0x00;
	mqtt.buff[mqtt.Fixed_len+1] = 0x04;
	mqtt.buff[mqtt.Fixed_len+2] = 0x4D;  // 'M'
	mqtt.buff[mqtt.Fixed_len+3] = 0x51;  // 'Q'
	mqtt.buff[mqtt.Fixed_len+4] = 0x54;  // 'T'
	mqtt.buff[mqtt.Fixed_len+5] = 0x54;  // 'T'
	mqtt.buff[mqtt.Fixed_len+6] = 0x04;  // 协议级别
	// 连接标志：0xC4(含Will标志) + WillRetain + WillQoS + CleanSession
	mqtt.buff[mqtt.Fixed_len+7] = 0xC4 | (willretain<<5) | (willqs<<3) | (willclean<<1);
	mqtt.buff[mqtt.Fixed_len+8] = keepalive/256;
	mqtt.buff[mqtt.Fixed_len+9] = keepalive%256;
	
	// 载荷：ClientID
	mqtt.buff[mqtt.Fixed_len+10] = strlen(mqtt.ClientID)/256;
	mqtt.buff[mqtt.Fixed_len+11] = strlen(mqtt.ClientID)%256;
	memcpy(&mqtt.buff[mqtt.Fixed_len+12],mqtt.ClientID,strlen(mqtt.ClientID));

	// 载荷：WillTopic
	mqtt.buff[mqtt.Fixed_len+12+strlen(mqtt.ClientID)] = strlen(mqtt.WillTopic)/256;
	mqtt.buff[mqtt.Fixed_len+13+strlen(mqtt.ClientID)] = strlen(mqtt.WillTopic)%256;
	memcpy(&mqtt.buff[mqtt.Fixed_len+14+strlen(mqtt.ClientID)],mqtt.WillTopic,strlen(mqtt.WillTopic));

	// 载荷：WillData
	mqtt.buff[mqtt.Fixed_len+14+strlen(mqtt.ClientID)+strlen(mqtt.WillTopic)] = strlen(mqtt.WillData)/256;
	mqtt.buff[mqtt.Fixed_len+15+strlen(mqtt.ClientID)+strlen(mqtt.WillTopic)] = strlen(mqtt.WillData)%256;
	memcpy(&mqtt.buff[mqtt.Fixed_len+16+strlen(mqtt.ClientID)+strlen(mqtt.WillTopic)],mqtt.WillData,strlen(mqtt.WillData));	
	
	// 载荷：Username
	mqtt.buff[mqtt.Fixed_len+16+strlen(mqtt.ClientID)+strlen(mqtt.WillTopic)+strlen(mqtt.WillData)] = strlen(mqtt.Username)/256;
	mqtt.buff[mqtt.Fixed_len+17+strlen(mqtt.ClientID)+strlen(mqtt.WillTopic)+strlen(mqtt.WillData)] = strlen(mqtt.Username)%256;
	memcpy(&mqtt.buff[mqtt.Fixed_len+18+strlen(mqtt.ClientID)+strlen(mqtt.WillTopic)+strlen(mqtt.WillData)],mqtt.Username,strlen(mqtt.Username));	
    
	// 载荷：Password
	mqtt.buff[mqtt.Fixed_len+18+strlen(mqtt.ClientID)+strlen(mqtt.WillTopic)+strlen(mqtt.WillData)+strlen(mqtt.Username)] = strlen(mqtt.Password)/256;
	mqtt.buff[mqtt.Fixed_len+19+strlen(mqtt.ClientID)+strlen(mqtt.WillTopic)+strlen(mqtt.WillData)+strlen(mqtt.Username)] = strlen(mqtt.Password)%256;
	memcpy(&mqtt.buff[mqtt.Fixed_len+20+strlen(mqtt.ClientID)+strlen(mqtt.WillTopic)+strlen(mqtt.WillData)+strlen(mqtt.Username)],mqtt.Password,strlen(mqtt.Password));	

	mqtt.len = mqtt.Fixed_len + mqtt.Var_len + mqtt.Payload_len;
}  

/**
 * @brief 解析MQTT CONNACK报文
 * 
 * 解析服务器返回的连接确认报文，提取连接返回码
 * 
 * @param rxdata      接收到的数据缓冲区
 * @param rxdata_len  数据长度
 * 
 * @return 连接返回码(0-255)，-1表示解析失败
 * 
 * CONNACK报文结构：
 * 固定头部：0x20 (CONNACK) + 0x02 (剩余长度)
 * 可变头部：1字节确认标志 + 1字节返回码
 * 
 * 返回码说明：
 * 0x00: 连接成功
 * 0x01: 不支持的协议版本
 * 0x02: 客户端标识符无效
 * 0x03: 服务器不可用
 * 0x04: 用户名或密码错误
 * 0x05: 未授权
 */
char MQTT_ConnectACK(unsigned char *rxdata,unsigned int rxdata_len)
{
    // 验证CONNACK报文：长度应为4字节，控制报文类型为0x20
    if((rxdata_len == 4)&&(rxdata[0] == 0x20)){
        // 验证通过，返回连接返回码(rxdata[3])
    }else{
        return -1;  // 非CONNACK报文
    }
    return rxdata[3];    // 返回连接返回码
}

/**
 * @brief 构建MQTT DISCONNECT报文
 * 
 * 构建断开连接请求报文，客户端优雅断开与服务器的连接
 * 
 * DISCONNECT报文结构：
 * 固定头部：0xE0 (DISCONNECT) + 0x00 (剩余长度)
 * 无可变头部和载荷
 */
void MQTT_DISCONNECT(void)
{
    mqtt.buff[0] = 0xE0;  // DISCONNECT控制报文类型
    mqtt.buff[1] = 0x00;  // 剩余长度为0
    mqtt.len = 2;         // 总长度2字节
}


/**
 * @brief 构建MQTT SUBSCRIBE报文
 * 
 * 构建订阅请求报文，请求订阅指定主题
 * 
 * @param topic 订阅的主题过滤器
 * @param Qs    请求的QoS级别(0/1/2)
 * 
 * SUBSCRIBE报文结构：
 * 固定头部：0x82 (SUBSCRIBE + QoS1) + 剩余长度(变长编码)
 * 可变头部：2字节Message ID
 * 载荷：主题过滤器 + QoS级别
 */
void MQTT_SUBSCRIBE(char *topic,char Qs)
{
	mqtt.Fixed_len = 1;  // 固定头部初始长度
	mqtt.Var_len = 2;   // 可变头部长度（Message ID）
	mqtt.Payload_len = 2 + strlen(topic) + 1; // 主题长度(2) + 主题 + QoS(1)
	mqtt.Remain_len = mqtt.Var_len + mqtt.Payload_len;
	
	mqtt.buff[0] = 0x82;  // SUBSCRIBE控制报文类型(0x80) + QoS1(0x02)
	
	// 变长编码写入剩余长度
	do{
		if(mqtt.Remain_len/128 == 0){     // 不需要进位
			mqtt.buff[mqtt.Fixed_len] = mqtt.Remain_len;
		}else{                               // 需要进位
			mqtt.buff[mqtt.Fixed_len] = (mqtt.Remain_len%128)|0x80;
		}
		mqtt.Fixed_len++;
		mqtt.Remain_len = mqtt.Remain_len/128;
	}while(mqtt.Remain_len);	

	// 可变头部：Message ID
	mqtt.buff[mqtt.Fixed_len] = mqtt.MessageID / 256;   // 高字节
	mqtt.buff[mqtt.Fixed_len+1] = mqtt.MessageID % 256; // 低字节
	mqtt.MessageID++; // 报文标识符递增
	if(mqtt.MessageID == 0)  // 防止溢出回绕到0
		mqtt.MessageID = 1;
	
	// 载荷：主题过滤器
	mqtt.buff[mqtt.Fixed_len+2] = strlen(topic)/256;
	mqtt.buff[mqtt.Fixed_len+3] = strlen(topic)%256;
	memcpy(&mqtt.buff[mqtt.Fixed_len+4],topic,strlen(topic));
	
	// 载荷：请求的QoS级别
	mqtt.buff[mqtt.Fixed_len+4 + strlen(topic)] = Qs;
		
	mqtt.len = mqtt.Fixed_len + mqtt.Var_len + mqtt.Payload_len;
}  

/**
 * @brief 解析MQTT SUBACK报文
 * 
 * 解析服务器返回的订阅确认报文，提取授予的QoS级别
 * 
 * @param rxdata      接收到的数据缓冲区
 * @param rxdata_len  数据长度
 * 
 * @return 授予的QoS级别(0/1/2)，-1表示解析失败
 * 
 * SUBACK报文结构：
 * 固定头部：0x90 (SUBACK) + 0x03 (剩余长度)
 * 可变头部：2字节Message ID
 * 载荷：1字节返回码（授予的QoS级别）
 */
char MQTT_SUBACK(unsigned char *rxdata,unsigned int rxdata_len)
{
    // 验证SUBACK报文：长度应为5字节，控制报文类型为0x90
    if((rxdata_len == 5)&&(rxdata[0] == 0x90)){
        // 验证通过
    }else{
        return -1;  // 非SUBACK报文
    }
    return rxdata[4];  // 返回授予的QoS级别
}

/**
 * @brief 构建MQTT UNSUBSCRIBE报文
 * 
 * 构建取消订阅请求报文，请求取消订阅指定主题
 * 
 * @param topic 要取消订阅的主题过滤器
 * 
 * UNSUBSCRIBE报文结构：
 * 固定头部：0xA2 (UNSUBSCRIBE + QoS1) + 剩余长度(变长编码)
 * 可变头部：2字节Message ID
 * 载荷：主题过滤器
 */
void MQTT_UNSUBSCRIBE(char *topic)
{
	mqtt.Fixed_len = 1;  // 固定头部初始长度
	mqtt.Var_len = 2;   // 可变头部长度（Message ID）
	mqtt.Payload_len = 2 + strlen(topic); // 主题长度(2) + 主题
	mqtt.Remain_len = mqtt.Var_len + mqtt.Payload_len;
	
	mqtt.buff[0] = 0xA2;  // UNSUBSCRIBE控制报文类型(0xA0) + QoS1(0x02)
	
	// 变长编码写入剩余长度
	do{
		if(mqtt.Remain_len/128 == 0){     // 不需要进位
			mqtt.buff[mqtt.Fixed_len] = mqtt.Remain_len;
		}else{                               // 需要进位
			mqtt.buff[mqtt.Fixed_len] = (mqtt.Remain_len%128)|0x80;
		}
		mqtt.Fixed_len++;
		mqtt.Remain_len = mqtt.Remain_len/128;
	}while(mqtt.Remain_len);	

	// 可变头部：Message ID
	mqtt.buff[mqtt.Fixed_len] = mqtt.MessageID / 256;
	mqtt.buff[mqtt.Fixed_len+1] = mqtt.MessageID % 256;
	mqtt.MessageID++; // 报文标识符递增
	if(mqtt.MessageID == 0)
		mqtt.MessageID = 1;
	
	// 载荷：主题过滤器
	mqtt.buff[mqtt.Fixed_len+2] = strlen(topic)/256;
	mqtt.buff[mqtt.Fixed_len+3] = strlen(topic)%256;
	memcpy(&mqtt.buff[mqtt.Fixed_len+4],topic,strlen(topic));		
	mqtt.len = mqtt.Fixed_len + mqtt.Var_len + mqtt.Payload_len;
}  
/**
 * @brief 解析MQTT UNSUBACK报文
 * 
 * 解析服务器返回的取消订阅确认报文
 * 
 * @param rxdata      接收到的数据缓冲区
 * @param rxdata_len  数据长度
 * 
 * @return 0表示成功，-1表示解析失败
 * 
 * UNSUBACK报文结构：
 * 固定头部：0xB0 (UNSUBACK) + 0x02 (剩余长度)
 * 可变头部：2字节Message ID
 */
char MQTT_UNSUBACK(unsigned char *rxdata,unsigned int rxdata_len)
{
    // 验证UNSUBACK报文：长度应为4字节，控制报文类型为0xB0
    if((rxdata_len == 4)&&(rxdata[0] == 0xB0)){
        // 验证通过
    }else{
        return -1;  // 非UNSUBACK报文
    }
    return 0;  // 取消订阅成功
}

/**
 * @brief 构建MQTT PINGREQ报文
 * 
 * 构建心跳请求报文，用于保持与服务器的连接
 * 
 * PINGREQ报文结构：
 * 固定头部：0xC0 (PINGREQ) + 0x00 (剩余长度)
 * 无可变头部和载荷
 */
void MQTT_PINGREQ(void)
{
    mqtt.buff[0] = 0xC0;  // PINGREQ控制报文类型
    mqtt.buff[1] = 0x00;  // 剩余长度为0
    mqtt.len = 2;         // 总长度2字节
}

/**
 * @brief 解析MQTT PINGRESP报文
 * 
 * 解析服务器返回的心跳响应报文
 * 
 * @param rxdata      接收到的数据缓冲区
 * @param rxdata_len  数据长度
 * 
 * @return 0表示成功，-1表示解析失败
 * 
 * PINGRESP报文结构：
 * 固定头部：0xD0 (PINGRESP) + 0x00 (剩余长度)
 * 无可变头部和载荷
 */
char MQTT_PINGRESP(unsigned char *rxdata,unsigned int rxdata_len)
{
    // 验证PINGRESP报文：长度应为2字节，控制报文类型为0xD0
    if((rxdata_len == 2)&&(rxdata[0] == 0xD0)){
        // 验证通过
    }else{
        return -1;  // 非PINGRESP报文
    }
    return 0;  // 心跳响应成功
}

/**
 * @brief 构建MQTT PUBLISH报文（QoS 0）
 * 
 * 构建QoS 0级别的消息发布报文（最多一次交付）
 * QoS 0无确认机制，消息可能丢失
 * 
 * @param retain    保留标志(0/1)
 * @param topic     发布的主题
 * @param data      消息载荷数据
 * @param data_len  数据长度
 * 
 * PUBLISH报文结构(QoS 0)：
 * 固定头部：0x30 (PUBLISH) + 剩余长度(变长编码)
 * 可变头部：主题名称(2字节长度+字符串)
 * 载荷：消息数据
 */
void MQTT_PUBLISH0(char retain,char *topic,unsigned char *data,unsigned int data_len)
{
	mqtt.Fixed_len = 1;     // 固定头部初始长度
	mqtt.Var_len = 2 + strlen(topic);  // 可变头部：主题长度(2) + 主题
	mqtt.Payload_len = data_len;       // 载荷长度
	mqtt.Remain_len = mqtt.Var_len + mqtt.Payload_len;
	
	// 固定头部：PUBLISH(0x30) + retain标志
	mqtt.buff[0] = 0x30 | (retain << 0);
	
	// 变长编码写入剩余长度
	do{
		if(mqtt.Remain_len/128 == 0){     // 不需要进位
			mqtt.buff[mqtt.Fixed_len] = mqtt.Remain_len;
		}else{                               // 需要进位
			mqtt.buff[mqtt.Fixed_len] = (mqtt.Remain_len%128)|0x80;
		}
		mqtt.Fixed_len++;
		mqtt.Remain_len = mqtt.Remain_len/128;
	}while(mqtt.Remain_len);	

	// 可变头部：主题名称
	mqtt.buff[mqtt.Fixed_len] = strlen(topic)/256;
	mqtt.buff[mqtt.Fixed_len+1] = strlen(topic)%256;
	memcpy(&mqtt.buff[mqtt.Fixed_len+2],topic,strlen(topic));
	
	// 载荷：消息数据
	memcpy(&mqtt.buff[mqtt.Fixed_len+2+strlen(topic)],data,data_len);
	
	mqtt.len = mqtt.Fixed_len + mqtt.Var_len + mqtt.Payload_len;
}
/**
 * @brief 构建MQTT PUBLISH报文（QoS 1）
 * 
 * 构建QoS 1级别的消息发布报文（至少一次交付）
 * QoS 1需要PUBACK确认，保证消息至少到达一次
 * 
 * @param dup       重发标志(0/1)
 * @param retain    保留标志(0/1)
 * @param topic     发布的主题
 * @param data      消息载荷数据
 * @param data_len  数据长度
 * 
 * PUBLISH报文结构(QoS 1)：
 * 固定头部：0x32 (PUBLISH+QoS1) + dup + retain + 剩余长度(变长编码)
 * 可变头部：主题名称 + 2字节Message ID
 * 载荷：消息数据
 */
void MQTT_PUBLISH1(char dup,char retain,char *topic,unsigned char *data,unsigned int data_len)
{
	mqtt.Fixed_len = 1;     // 固定头部初始长度
	mqtt.Var_len = 2 + strlen(topic) + 2;  // 主题长度(2) + 主题 + Message ID(2)
	mqtt.Payload_len = data_len;
	mqtt.Remain_len = mqtt.Var_len + mqtt.Payload_len;
	
	// 固定头部：PUBLISH(0x30) + QoS1(0x02) + dup + retain
	mqtt.buff[0] = 0x32 | (dup<<3) | (retain << 0);
	
	// 变长编码写入剩余长度
	do{
		if(mqtt.Remain_len/128 == 0){     // 不需要进位
			mqtt.buff[mqtt.Fixed_len] = mqtt.Remain_len;
		}else{                               // 需要进位
			mqtt.buff[mqtt.Fixed_len] = (mqtt.Remain_len%128)|0x80;
		}
		mqtt.Fixed_len++;
		mqtt.Remain_len = mqtt.Remain_len/128;
	}while(mqtt.Remain_len);	

	// 可变头部：主题名称
	mqtt.buff[mqtt.Fixed_len] = strlen(topic)/256;
	mqtt.buff[mqtt.Fixed_len+1] = strlen(topic)%256;
	memcpy(&mqtt.buff[mqtt.Fixed_len+2],topic,strlen(topic));

	// 可变头部：Message ID
	mqtt.buff[mqtt.Fixed_len+2+strlen(topic)] = mqtt.MessageID / 256;
	mqtt.buff[mqtt.Fixed_len+3+strlen(topic)] = mqtt.MessageID % 256;
	mqtt.MessageID++; // 报文标识符递增
	if(mqtt.MessageID == 0)
		mqtt.MessageID = 1;
	
	// 载荷：消息数据
	memcpy(&mqtt.buff[mqtt.Fixed_len+4+strlen(topic)],data,data_len);
	
	mqtt.len = mqtt.Fixed_len + mqtt.Var_len + mqtt.Payload_len;
} 
/**
 * @brief 构建MQTT PUBLISH报文（QoS 2）
 * 
 * 构建QoS 2级别的消息发布报文（恰好一次交付）
 * QoS 2需要PUBREC/PUBREL/PUBCOMP三次握手，保证消息只到达一次
 * 
 * @param dup       重发标志(0/1)
 * @param retain    保留标志(0/1)
 * @param topic     发布的主题
 * @param data      消息载荷数据
 * @param data_len  数据长度
 * 
 * PUBLISH报文结构(QoS 2)：
 * 固定头部：0x34 (PUBLISH+QoS2) + dup + retain + 剩余长度(变长编码)
 * 可变头部：主题名称 + 2字节Message ID
 * 载荷：消息数据
 */
void MQTT_PUBLISH2(char dup,char retain,char *topic,unsigned char *data,unsigned int data_len)
{
	mqtt.Fixed_len = 1;     // 固定头部初始长度
	mqtt.Var_len = 2 + strlen(topic) + 2;  // 主题长度(2) + 主题 + Message ID(2)
	mqtt.Payload_len = data_len;
	mqtt.Remain_len = mqtt.Var_len + mqtt.Payload_len;
	
	// 固定头部：PUBLISH(0x30) + QoS2(0x04) + dup + retain
	mqtt.buff[0] = 0x34 | (dup<<3) | (retain << 0);
	
	// 变长编码写入剩余长度
	do{
		if(mqtt.Remain_len/128 == 0){     // 不需要进位
			mqtt.buff[mqtt.Fixed_len] = mqtt.Remain_len;
		}else{                               // 需要进位
			mqtt.buff[mqtt.Fixed_len] = (mqtt.Remain_len%128)|0x80;
		}
		mqtt.Fixed_len++;
		mqtt.Remain_len = mqtt.Remain_len/128;
	}while(mqtt.Remain_len);	

	// 可变头部：主题名称
	mqtt.buff[mqtt.Fixed_len] = strlen(topic)/256;
	mqtt.buff[mqtt.Fixed_len+1] = strlen(topic)%256;
	memcpy(&mqtt.buff[mqtt.Fixed_len+2],topic,strlen(topic));

	// 可变头部：Message ID
	mqtt.buff[mqtt.Fixed_len+2+strlen(topic)] = mqtt.MessageID / 256;
	mqtt.buff[mqtt.Fixed_len+3+strlen(topic)] = mqtt.MessageID % 256;
	mqtt.MessageID++; // 报文标识符递增
	if(mqtt.MessageID == 0)
		mqtt.MessageID = 1;
	
	// 载荷：消息数据
	memcpy(&mqtt.buff[mqtt.Fixed_len+4+strlen(topic)],data,data_len);
	
	mqtt.len = mqtt.Fixed_len + mqtt.Var_len + mqtt.Payload_len;
}  
/**
 * @brief MQTT PUBLISH报文解析函数
 * 
 * 用于解析从服务器收到的PUBLISH报文，提取Topic名称、消息ID和消息载荷。
 * 支持QoS 0、QoS 1、QoS 2三种服务质量等级的PUBLISH报文解析。
 * 
 * @param rxdata      接收到的原始数据缓冲区
 * @param rxdata_len  接收到的数据长度
 * @param qs          输出参数：解析得到的QoS级别（0/1/2）
 * @param messageid   输出参数：消息ID（QoS 0时为0）
 * 
 * @return 0  - 解析成功
 * @return -1 - 解析失败（非PUBLISH报文）
 * 
 * @note PUBLISH报文结构：
 *       固定头部：1字节控制报文类型 + 1-4字节剩余长度（变长编码）
 *       可变头部：Topic名称（2字节长度+字符串） + [Message ID（仅QoS>0）]
 *       载荷：消息数据
 *       控制报文类型：0x30 (3 << 4)，表示PUBLISH
 *       QoS级别由固定头部低2位（bit1-bit0）表示
 */
char MQTT_ProcessPUBLISH(unsigned char *rxdata, unsigned int rxdata_len, unsigned char *qs, unsigned int *messageid)
{
	char i;              // 剩余长度字节数计数器
	int topic_len;       // Topic名称长度
	int data_len;        // 消息载荷长度
	
	// 验证是否为PUBLISH报文：控制报文类型应为0x30 (3 << 4)
	if((rxdata[0]&0xF0) == 0x30){
		// 解析剩余长度的可变编码（最多4字节）
		// 剩余长度采用小端序变长编码，最高位为延续位
		for(i=1; i<5; i++){
			if((rxdata[i]&0x80) == 0x00)  // 最高位为0表示这是最后一个字节
				break;
		}
		
		// 根据QoS级别（固定头部bit1-bit0）进行分支处理
		switch(rxdata[0]&0x06){
			// ============ QoS 0 处理 ============
			// QoS 0：最多一次交付，无Message ID，无需确认
			case 0x00:				
					*qs = 0;                      // 设置QoS级别为0
					*messageid = 0;               // QoS 0无Message ID
					topic_len = rxdata[1+i]*256 + rxdata[1+i+1];  // 解析Topic长度（大端序）
					// 计算载荷长度：总长度 - 固定头部 - 剩余长度字节 - Topic长度字段 - Topic内容
					data_len = rxdata_len - 1 - i - 2 - topic_len;
					memset(mqtt.topic, 0, TOPIC_SIZE);            // 清空Topic缓冲区
					memset(mqtt.data, 0, DATA_SIZE);              // 清空数据缓冲区
					memcpy(mqtt.topic, &rxdata[1+i], topic_len+2); // 复制Topic（含2字节长度）
					mqtt.data[0] = data_len/256;                  // 载荷长度高字节
					mqtt.data[1] = data_len%256;                  // 载荷长度低字节
					// 复制消息载荷到缓冲区
					memcpy(&mqtt.data[2], &rxdata[1+i+2+topic_len], data_len);					
					break; 
			
			// ============ QoS 1 处理 ============
			// QoS 1：至少一次交付，有Message ID，需要PUBACK确认
			case 0x02:				
					*qs = 1;                      // 设置QoS级别为1
					topic_len = rxdata[1+i]*256 + rxdata[1+i+1];  // 解析Topic长度
					// 解析Message ID（大端序，位于Topic之后）
					*messageid = rxdata[1+i+2+topic_len]*256 + rxdata[1+i+2+topic_len+1];
					// 计算载荷长度：多减去2字节Message ID
					data_len = rxdata_len - 1 - i - 2 - topic_len - 2;
					memset(mqtt.topic, 0, TOPIC_SIZE);
					memset(mqtt.data, 0, DATA_SIZE);
					memcpy(mqtt.topic, &rxdata[1+i], topic_len+2);
					mqtt.data[0] = data_len/256;
					mqtt.data[1] = data_len%256;
					// 复制消息载荷（跳过Message ID）
					memcpy(&mqtt.data[2], &rxdata[1+i+2+topic_len+2], data_len);					
					break; 
			
			// ============ QoS 2 处理 ============
			// QoS 2：恰好一次交付，有Message ID，需要PUBREC/PUBREL/PUBCOMP握手
			case 0x04:				
					*qs = 2;                      // 设置QoS级别为2
					topic_len = rxdata[1+i]*256 + rxdata[1+i+1];  // 解析Topic长度
					// 解析Message ID（大端序）
					*messageid = rxdata[1+i+2+topic_len]*256 + rxdata[1+i+2+topic_len+1];
					// 计算载荷长度：多减去2字节Message ID
					data_len = rxdata_len - 1 - i - 2 - topic_len - 2;
					memset(mqtt.topic, 0, TOPIC_SIZE);
					memset(mqtt.data, 0, DATA_SIZE);
					memcpy(mqtt.topic, &rxdata[1+i], topic_len+2);
					mqtt.data[0] = data_len/256;
					mqtt.data[1] = data_len%256;
					// 复制消息载荷（跳过Message ID）
					memcpy(&mqtt.data[2], &rxdata[1+i+2+topic_len+2], data_len);					
					break; 
		}
	}else{
		// 非PUBLISH报文，返回-1表示解析失败
		return -1;
	}
	
	// 解析成功，返回0
	return 0;
}
/**
 * @brief 构建MQTT PUBACK报文
 * 
 * 构建PUBACK确认报文，用于确认QoS 1的PUBLISH消息
 * 
 * @param messageid 要确认的消息ID
 * 
 * PUBACK报文结构：
 * 固定头部：0x40 (PUBACK) + 0x02 (剩余长度)
 * 可变头部：2字节Message ID
 */
void MQTT_PUBACK(unsigned int messageid)
{
    mqtt.buff[0] = 0x40;  // PUBACK控制报文类型
    mqtt.buff[1] = 0x02;  // 剩余长度
	mqtt.buff[2] = messageid/256;   // Message ID高字节
	mqtt.buff[3] = messageid%256;   // Message ID低字节
    mqtt.len = 4;         // 总长度4字节
}

/**
 * @brief 解析MQTT PUBACK报文
 * 
 * 解析收到的PUBACK确认报文，提取消息ID
 * 
 * @param rxdata      接收到的数据缓冲区
 * @param rxdata_len  数据长度
 * @param messageid   输出参数：消息ID
 * 
 * @return 0表示成功，-1表示解析失败
 */
char MQTT_ProcessPUBACK(unsigned char *rxdata, unsigned int rxdata_len, unsigned int *messageid)
{
    // 验证PUBACK报文：长度应为4字节，控制报文类型为0x40
    if((rxdata_len == 4)&&(rxdata[0] == 0x40)){
        *messageid = rxdata[2]*256 + rxdata[3];  // 解析Message ID
	
    }else{
        return -1;  // 非PUBACK报文
    }
    return 0;    // 解析成功
} 

/**
 * @brief 构建MQTT PUBREC报文
 * 
 * 构建PUBREC确认报文，是QoS 2消息发布流程的第一步确认
 * 
 * @param messageid 要确认的消息ID
 * 
 * PUBREC报文结构：
 * 固定头部：0x50 (PUBREC) + 0x02 (剩余长度)
 * 可变头部：2字节Message ID
 */
void MQTT_PUBREC(unsigned int messageid)
{
    mqtt.buff[0] = 0x50;  // PUBREC控制报文类型
    mqtt.buff[1] = 0x02;  // 剩余长度
	mqtt.buff[2] = messageid/256;   // Message ID高字节
	mqtt.buff[3] = messageid%256;   // Message ID低字节
    mqtt.len = 4;         // 总长度4字节
}

/**
 * @brief 解析MQTT PUBREC报文
 * 
 * 解析收到的PUBREC确认报文，提取消息ID
 * 
 * @param rxdata      接收到的数据缓冲区
 * @param rxdata_len  数据长度
 * @param messageid   输出参数：消息ID
 * 
 * @return 0表示成功，-1表示解析失败
 */
char MQTT_ProcessPUBREC(unsigned char *rxdata, unsigned int rxdata_len, unsigned int *messageid)
{
    // 验证PUBREC报文：长度应为4字节，控制报文类型为0x50
    if((rxdata_len == 4)&&(rxdata[0] == 0x50)){
        *messageid = rxdata[2]*256 + rxdata[3];  // 解析Message ID
	
    }else{
        return -1;  // 非PUBREC报文
    }
    return 0;    // 解析成功
} 
/**
 * @brief 构建MQTT PUBREL报文
 * 
 * 构建PUBREL释放报文，是QoS 2消息发布流程的第二步
 * 
 * @param messageid 要释放的消息ID
 * 
 * PUBREL报文结构：
 * 固定头部：0x62 (PUBREL + QoS1) + 0x02 (剩余长度)
 * 可变头部：2字节Message ID
 */
void MQTT_PUBREL(unsigned int messageid)
{
    mqtt.buff[0] = 0x62;  // PUBREL控制报文类型(0x60) + QoS1(0x02)
    mqtt.buff[1] = 0x02;  // 剩余长度
	mqtt.buff[2] = messageid/256;   // Message ID高字节
	mqtt.buff[3] = messageid%256;   // Message ID低字节
    mqtt.len = 4;         // 总长度4字节
}

/**
 * @brief 解析MQTT PUBREL报文
 * 
 * 解析收到的PUBREL释放报文，提取消息ID
 * 
 * @param rxdata      接收到的数据缓冲区
 * @param rxdata_len  数据长度
 * @param messageid   输出参数：消息ID
 * 
 * @return 0表示成功，-1表示解析失败
 */
char MQTT_ProcessPUBREL(unsigned char *rxdata, unsigned int rxdata_len, unsigned int *messageid)
{
    // 验证PUBREL报文：长度应为4字节，控制报文类型为0x62
    if((rxdata_len == 4)&&(rxdata[0] == 0x62)){
        *messageid = rxdata[2]*256 + rxdata[3];  // 解析Message ID
	
    }else{
        return -1;  // 非PUBREL报文
    }
    return 0;    // 解析成功
} 
/**
 * @brief 构建MQTT PUBCOMP报文
 * 
 * 构建PUBCOMP完成报文，是QoS 2消息发布流程的第三步（最后一步）
 * 
 * @param messageid 要完成的消息ID
 * 
 * PUBCOMP报文结构：
 * 固定头部：0x70 (PUBCOMP) + 0x02 (剩余长度)
 * 可变头部：2字节Message ID
 */
void MQTT_PUBCOMP(unsigned int messageid)
{
    mqtt.buff[0] = 0x70;  // PUBCOMP控制报文类型
    mqtt.buff[1] = 0x02;  // 剩余长度
	mqtt.buff[2] = messageid/256;   // Message ID高字节
	mqtt.buff[3] = messageid%256;   // Message ID低字节
    mqtt.len = 4;         // 总长度4字节
}

/**
 * @brief 解析MQTT PUBCOMP报文
 * 
 * 解析收到的PUBCOMP完成报文，提取消息ID
 * 
 * @param rxdata      接收到的数据缓冲区
 * @param rxdata_len  数据长度
 * @param messageid   输出参数：消息ID
 * 
 * @return 0表示成功，-1表示解析失败
 */
char MQTT_ProcessPUBCOMP(unsigned char *rxdata, unsigned int rxdata_len, unsigned int *messageid)
{
    // 验证PUBCOMP报文：长度应为4字节，控制报文类型为0x70
    if((rxdata_len == 4)&&(rxdata[0] == 0x70)){
        *messageid = rxdata[2]*256 + rxdata[3];  // 解析Message ID
	
    }else{
        return -1;  // 非PUBCOMP报文
    }
    return 0;    // 解析成功
}