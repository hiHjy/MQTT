#include <MQTTClientPersistence.h>
#include <MQTTReasonCodes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "MQTTClient.h"
#include <unistd.h>

#define BROKER_ADDRESS "tcp://127.0.0.1:1883"  // 公共测试服务器
#define CLIENT_ID     "imx6ull_gateway_001"         // 客户端ID（需唯一）
#define PUB_TOPIC     "imx6ull/led" // 发布主题
#define SUB_TOPIC     "control/imx6ull"             // 订阅主题
#define QOS           1                             // 服务质量
#define TIMEOUT       10000L                        // 超时时间(毫秒)
MQTTClient client;
//连接丢失回调
void connection_lost(void *context, char *cause) {
    printf("\nConnection lost!\n");
    printf("     Cause: %s\n", cause);
    printf("Reconnecting...\n");
    while (MQTTClient_connect(client, NULL) != MQTTCLIENT_SUCCESS) {
        //连接丢失后一直卡在着？
        printf("Reconnect failed, retrying in 5 seconds...\n");
        sleep(5);
    }
    printf("Reconnected successfully!\n");
}

int msg_arrived(void *context, char *topicName, int topicLen, MQTTClient_message *msg) {
    printf("Message arrived on topic: %s\n", topicName);
    // printf("Message payload: %.*s\n", msg->payloadlen, (char*)msg->payload);
    printf("Msg payload: %.*s\n", msg->payloadlen, (char*)msg->payload);

    // 这里可以添加处理控制命令的逻辑
    // 例如：if (strstr((char*)message->payload, "reboot")) { system("reboot"); }
    
    MQTTClient_freeMessage(&msg);
    MQTTClient_free(topicName);
    return 1;
}

// MQTTClient_deliveryToken token：
// 消息的唯一标识符（令牌），用于跟踪已发布的消息。

// 该令牌由 MQTTClient_publishMessage 返回，可用于确认哪条消息送达。
void delivery_complete(void *context, MQTTClient_deliveryToken token) {
    printf("Message with token %d delivery confirmed\n", token);
}

int mqtt_init() {
    /*---------------------------------------------------创建客户端句柄----------------------------------------*/
    int rc = MQTTClient_create(&client,
                                BROKER_ADDRESS,
                                CLIENT_ID,
                                MQTTCLIENT_PERSISTENCE_NONE,//持久化类型无
                                NULL);                      //持久化上下文

    if (rc != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "Failed to create client, return code %d\n", rc);
        exit(-1);
    }
    /*---------------------------------------------------设置回调------------------------------------------*/
    MQTTClient_setCallbacks(client, NULL, connection_lost, msg_arrived, delivery_complete);
    
    /*-------------------------------------------------设置连接选项------------------------------------------*/
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    conn_opts.keepAliveInterval = 60;
    conn_opts.cleansession = 1;

    /*----------------------------------------------连接到MQTT服务器---------------------------------------------------*/
    printf("Connecting to MQTT broker: %s\n", BROKER_ADDRESS);
    if ((rc = MQTTClient_connect(client, &conn_opts) != MQTTCLIENT_SUCCESS)) {
        fprintf(stderr, "Failed to connect, return code %d\n", rc);
        MQTTClient_destroy(client);
        return -1;
    }

    /*------------------------------------------------订阅主题---------------------------------------------------------------*/
    printf("Subscribing to topic: %s\n", SUB_TOPIC);
    if ((rc = MQTTClient_subscribe(client, SUB_TOPIC, QOS) != MQTTCLIENT_SUCCESS)) {
        fprintf(stderr, "Failed to subscribe, return code %d\n", rc);
        MQTTClient_disconnect(client, 1000);
        MQTTClient_destroy(&client);
        return -1;
    }

   return 0;

}

int mqtt_publish(const char *topic,const char *payload) {
    MQTTClient_message pub_msg = MQTTClient_message_initializer;
    MQTTClient_deliveryToken token;
    pub_msg.payload = (void *)payload;
    pub_msg.payloadlen = strlen(payload);
    pub_msg.qos = QOS;
    pub_msg.retained = 0;
    int rc;
    //发布消息
    if ((rc = MQTTClient_publishMessage(client, topic, &pub_msg, &token)) != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "Failed to publish message, return code %d\n", rc);
        return -1;
    }
    //等待消息发布完成 QOS>0

    rc = MQTTClient_waitForCompletion(client, token, TIMEOUT);
    if (rc != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "Failed to complete delivery token%d\n", token);
        return -1;
    }
    printf("Message published: %s\n", payload);
    return 0;

}

int main(int argc, char *argv[]) {

    if (mqtt_init() != 0) {
        fprintf(stderr, "MQTT initialization failed\n");
        return -1;
    }
    printf("MQTT connected successfully!\n");

    while (1) {

        char payload[100];
        int i = 1;
        snprintf(payload, sizeof(payload), "{\"led\":\"%d\"}", i);
        i = -i;

        int rc;
        if ((rc = mqtt_publish(SUB_TOPIC, payload) != MQTTCLIENT_SUCCESS)) {
            fprintf(stderr, "mqtt_publish error code:%d\n", rc);
            MQTTClient_disconnect(client, TIMEOUT);
            return -1;
        }
        sleep(5);

    }

    return 0;
}



