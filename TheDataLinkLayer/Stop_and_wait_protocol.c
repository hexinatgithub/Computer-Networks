/*	Stop-and-wait protocol also provides for a one-directional flow of data from sender to receiver.
	The communication channel is once again assumed to be error free.However, this time the receiver has only
	a finit buffer capacity and a finite processing speed, so the protocol must explicitly prevent the sender from flooding the
	receiver with data faster than it can be handled.
*/

typedef enum {frame_arrival} event_type;

#include "protocol.h"

void sender(void) {
	frame s;
	packet buffer;
	event_type event;

	while(true) {
		from_network_layer(&buffer);
		s.info = buffer;
		to_physical_layer(&s);
		wait_for_event(&event);
	}
}

void receiver(void) {
	frame r, s;
	event_type event;

	while(true) {
		wait_for_event(&event);
		from_physical_layer(&r);
		to_network_layer(&r.info);
		to_physical_layer(&s);
	}
}