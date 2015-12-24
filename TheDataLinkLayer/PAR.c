#define MAX_SEQ 1
typedef enum {frame_arrival, cksum_err, timeout} event_type;
#include "protocol.h"

void sender(void) {
	frame s, r;
	packet buffer;
	event_type event;
	seq_nr next_frame_to_send;

	from_network_layer(&buffer);
	next_frame_to_send = 0;
	s.seq = next_frame_to_send;
	s.info = buffer;
	while(true) {
		to_physical_layer(&s);
		wait_for_event(&event);
		if(event == frame_arrival){
			from_physical_layer(&r);
			if(r.ack == next_frame_to_send){
				from_network_layer(&buffer);
				s.seq = inc(next_frame_to_send);
				s.info = buffer;
			}
		}
	}
}

void receiver(void) {
	frame r, s;
	event_type event;
	seq_nr frame_expected;

	frame_expected = 0;
	while(true) {
		wait_for_event(&event);
		if(event == frame_arrival){
			from_physical_layer(&r);
			if(r.seq == frame_expected){
				to_network_layer(&r.info);
				inc(frame_expected);
			}
			s.ack = r.seq;
			to_physical_layer(&s);
		}
	}
}