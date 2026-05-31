#include "mqtt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int i,res;
unsigned char databuff[64];
unsigned char outbuff[64];
unsigned char qs;
unsigned int messageid;
int Str_to_Hex(char* idnata, unsigned char *outdata)
{
	int num;
	char* str;
	char* endptr;
	
	num = 0;
	str = idnata;
	while(*str != '\0' ){
		outdata[num] = strtol(str,&endptr,16);
		num++;
		str = endptr;		
	}
	
	return num;
}

int main(int argc, char *argv[])
{
    MQTT_Init();
    printf("ClientID: %s\n", mqtt.ClientID);
    printf("Username: %s\n", mqtt.Username);
    printf("Password: %s\n", mqtt.Password);
    
    MQTT_ConnectWILL(100,1,2,1);
    MQTT_DISCONNECT();
    MQTT_SUBSCRIBE("USER002",2);
    MQTT_UNSUBSCRIBE("USER002");
	MQTT_PUBLISH0(0,"USER001","123",3);	
	MQTT_PUBLISH1(0,0,"USER001","123",3);
    for(i=0;i<mqtt.len;i++)
        printf("%02x ",mqtt.buff[i]);
    printf("\r\n\r\n");

    gets(databuff);
    for(i=0;i<strlen(databuff);i++)
		printf("%02x ",databuff[i]);
	printf("\r\n\r\n");

    res = Str_to_Hex(databuff,outbuff);
	for(i=0;i<res;i++)
		printf("%02x ",outbuff[i]);
	printf("\r\n\r\n");	
	printf("%d\r\n",MQTT_ProcessPUBREC(outbuff,res,&messageid));
	// printf("qs: %d\r\n",qs);
	printf("messageid: %x\r\n",messageid);
	// printf("topic_len: %d\r\n",mqtt.topic[0]*256 + mqtt.topic[1]);
	// printf("topic: %s\r\n",&mqtt.topic[2]);
	// printf("data_len: %d\r\n",mqtt.data[0]*256 + mqtt.data[1]);
	// printf("data: %s\r\n",&mqtt.data[2]);

	// if(qs == 1){
	// 	MQTT_PUBACK(messageid);
	// }else if(qs == 2){
	// 	MQTT_PUBREC(messageid);

	// }
	// for(i=0;i<mqtt.len;i++)
    //     printf("%02x ",mqtt.buff[i]);
    // printf("\r\n\r\n");

	MQTT_PUBREL(messageid);
    for(i=0;i<mqtt.len;i++)
        printf("%02x ",mqtt.buff[i]);
    printf("\r\n\r\n");
    return 0;
}
