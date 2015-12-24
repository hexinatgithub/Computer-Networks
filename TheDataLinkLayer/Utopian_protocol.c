/* 	Utopia provides for data transmission in one direction only, from sender to receiver. 
	The communication channel is assumed to be error free and the receiver is assumed to be
	able process all the input infinitely quickly.
	Consequently, the sender just sits in a loop pumping data out onto the line as fast as it can.
*/

typedef enum {frame_arrival} event_type;

#include "protocol.h"

void sender(void) {
	frame s;
	packet buffer;
	while(true) {
		from_network_layer(&buffer);
		s.info = buffer;
		to_physical_layer(&s);
	}
}

void receiver(void) {
	frame r;
	event_type event;

	while(true) {
		wait_for_event(&event);
		from_physical_layer(&r);
		to_network_layer(&r.info);
	}
}