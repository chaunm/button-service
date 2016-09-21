/*
 * button.h
 *
 *  Created on: Sep 20, 2016
 *      Author: ChauNM
 */

#ifndef BUTTON_H_
#define BUTTON_H_

#define BUTTON_STATE_PIN		16

typedef struct tagBUTTONACTOROPTION {
	char* guid;
	char* psw;
	char* host;
	WORD port;
}BUTTONACTOROPTION, *PBUTTONACTOROPTION;

#endif /* BUTTON_H_ */
