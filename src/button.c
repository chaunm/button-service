/*
 * button.c
 *
 *  Created on: Sep 20, 2016
 *      Author: ChauNM
 */


#include "button.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>
#include "jansson.h"
#include "lib/wiringPi/wiringPi.h"
#include "universal.h"
#include "Actor/actor.h"


#define MAIN_LOOP_PERIOD		10000
#define RESET_CYCLE_COUNT		300
#define RESET_WIFI_CYCLE_COUNT	100

static PACTOR buttonActor = NULL;
static BOOL buttonState;
static unsigned long blackoutTimeStamp;

static void ButtonSetupGpio()
{
	wiringPiSetupSys();
	pinMode(BUTTON_STATE_PIN, INPUT);
	buttonState = digitalRead(BUTTON_STATE_PIN);
}
/*
static void PowerActorOnHiRequest(PVOID pParam)
{
	char* message = (char*) pParam;
	char **znpSplitMessage;
	if (buttonActor == NULL) return;
	json_t* responseJson = NULL;
	json_t* statusJson = NULL;
	PACTORHEADER header;
	char* responseTopic;
	char* responseMessage;
	znpSplitMessage = ActorSplitMessage(message);
	if (znpSplitMessage == NULL)
		return;
	// parse header to get origin of message
	header = ActorParseHeader(znpSplitMessage[0]);
	if (header == NULL)
	{
		ActorFreeSplitMessage(znpSplitMessage);
		return;
	}
	//make response package
	responseJson = json_object();
	statusJson = json_object();
	json_t* requestJson = json_loads(znpSplitMessage[1], JSON_DECODE_ANY, NULL);
	json_object_set(responseJson, "request", requestJson);
	json_decref(requestJson);
	json_t* resultJson = json_string("status.success");
	json_object_set(statusJson, "status", resultJson);
	json_decref(resultJson);
	json_t* blackoutJson = NULL;
	json_t* blackoutTimeStampJson = NULL;
	json_t* durationTimeJson = NULL;
	json_t* elapsedTimeJson;
	if (powerState == POWER_OFF)
	{
		blackoutJson = json_string("true");
		blackoutTimeStampJson = json_integer(blackoutTimeStamp);
		durationTimeJson = json_integer(BATTERY_DURATION);
		elapsedTimeJson = json_integer(backoutDuration);
		json_object_set(statusJson, "blackout", blackoutJson);
		json_object_set(statusJson, "backoutDuration", blackoutTimeStampJson);
		json_object_set(statusJson, "durationTime", durationTimeJson);
		json_object_set(statusJson, "elapsedTime", elapsedTimeJson);
		json_decref(blackoutJson);
		json_decref(blackoutTimeStampJson);
		json_decref(durationTimeJson);
		json_decref(elapsedTimeJson);
	}
	else
	{
		blackoutJson = json_string("false");
		json_object_set(statusJson, "blackout", blackoutJson);
		json_decref(blackoutJson);
	}
	json_object_set(responseJson, "response", statusJson);
	json_decref(statusJson);
	responseMessage = json_dumps(responseJson, JSON_INDENT(4) | JSON_REAL_PRECISION(4));
	responseTopic = ActorMakeTopicName(header->origin, "/:response");
	ActorFreeHeaderStruct(header);
	json_decref(responseJson);
	ActorFreeSplitMessage(znpSplitMessage);
	ActorSend(buttonActor, responseTopic, responseMessage, NULL, FALSE);
	free(responseMessage);
	free(responseTopic);
}
*/

static void ButtonPublishEvent(char* event)
{
	unsigned long long timeStamp = time(NULL);
	blackoutTimeStamp = timeStamp;
	if (buttonActor == NULL) return;
	json_t* eventJson = json_object();
	json_t* paramsJson = json_object();
	json_t* contentJson = json_string(event);
	json_object_set(paramsJson, "event", contentJson);
	json_object_set(eventJson, "params", paramsJson);
	char* eventMessage = json_dumps(eventJson, JSON_INDENT(4) | JSON_REAL_PRECISION(4));
	char* topicName = ActorMakeTopicName(buttonActor->guid, "/:event/button_event");
	ActorSend(buttonActor, topicName, eventMessage, NULL, FALSE);
	json_decref(contentJson);
	json_decref(paramsJson);
	json_decref(eventJson);
	free(topicName);
	free(eventMessage);
}

static void ButtonActorCreate(char* guid, char* psw, char* host, WORD port)
{
	buttonActor = ActorCreate(guid, psw, host, port);
	//Register callback to handle request package
	if (buttonActor == NULL)
	{
		printf("Couldn't create actor\n");
		return;
	}
	//ActorRegisterCallback(buttonActor, ":request/Hi", PowerActorOnHiRequest, CALLBACK_RETAIN);
}

static void ButtonProcess()
{
	static unsigned long buttonCount;
	if (digitalRead(BUTTON_STATE_PIN) == LOW)
	{
		if (buttonState == HIGH)
		{
			buttonState = LOW;
			buttonCount = 1;
		}
		if (buttonCount > 0)
			buttonCount++;
		if (buttonCount == RESET_CYCLE_COUNT)
		{
			buttonCount = 0;
			ButtonPublishEvent("reset");
		}
	}
	else
	{
		buttonState = HIGH;
		if ((buttonCount > RESET_WIFI_CYCLE_COUNT) && (buttonCount < RESET_CYCLE_COUNT))
			ButtonPublishEvent("reset_wifi");
		if (buttonCount >= RESET_CYCLE_COUNT)
			ButtonPublishEvent("reset");
		buttonCount = 0;
	}
}

void PowerActorStart(PBUTTONACTOROPTION option)
{
	ButtonSetupGpio();
	mosquitto_lib_init();
	printf("Create actor\n");
	ButtonActorCreate(option->guid, option->psw, option->host, option->port);
	if (buttonActor == NULL)
	{
		mosquitto_lib_cleanup();
		return;
	}
	while(1)
	{
		ActorProcessEvent(buttonActor);
		ButtonProcess();
		mosquitto_loop(buttonActor->client, 0, 1);
		usleep(10000);
	}
	mosquitto_disconnect(buttonActor->client);
	mosquitto_destroy(buttonActor->client);
	mosquitto_lib_cleanup();
}

int main(int argc, char* argv[])
{
	BUTTONACTOROPTION option;
	/* get option */
	int opt= 0;
	char *token = NULL;
	char *guid = NULL;
	char *host = NULL;
	WORD port = 0;
	printf("start qr-service \n");
	// specific the expected option
	static struct option long_options[] = {
			{"id",      required_argument,  0, 'i' },
			{"token", 	required_argument,  0, 't' },
			{"host", 	required_argument,  0, 'H' },
			{"port", 	required_argument, 	0, 'p' },
			{"help", 	no_argument, 		0, 'h' }
	};
	int long_index;
	/* Process option */
	while ((opt = getopt_long(argc, argv,":hi:t:H:p:",
			long_options, &long_index )) != -1) {
		switch (opt) {
		case 'h' :
			printf("using: LedActor --<token> --<id> --<host> --port<>\n"
					"id: guid of the actor\n"
					"token: password of the actor\n"
					"host: mqtt server address, if omitted using local host\n"
					"port: mqtt port, if omitted using port 1883\n");
			return (EXIT_SUCCESS);
			break;
		case 'i':
			guid = StrDup(optarg);
			break;
		case 't':
			token = StrDup(optarg);
			break;
		case 'H':
			host = StrDup(optarg);
			break;
		case 'p':
			port = atoi(optarg);
			break;
		case ':':
			if (optopt == 'i')
			{
				printf("invalid option(s), using --help for help\n");
				return EXIT_FAILURE;
			}
			break;
		default:
			break;
		}
	}
	if (guid == NULL)
	{
		printf("invalid option(s), using --help for help\n");
		return EXIT_FAILURE;
	}
	option.guid = guid;
	option.psw = token;
	option.host = host;
	option.port = port;
	PowerActorStart(&option);
	return EXIT_SUCCESS;
}




