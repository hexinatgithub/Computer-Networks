#define MAX_SEQ 1
typedef enum {frame_arrival, cksum_err, timeout} event_type;
#include "protocol.h"

void protocol4(void){
	seq_nr frame_expected;
	seq_nr next_frame_to_send;
	frame r, s;
	packet buffer;
	event_type event;

	next_frame_to_send = 0;
	frame_expected = 0;
	from_network_layer(&buffer);
	s.info = buffer;
	s.seq = next_frame_to_send;
	s.ack = 1-frame_expected;
	to_physical_layer(&s);
	start_timer(s.seq);
	while(true) {
		wait_for_event(&event);

		if(event == frame_arrival){		// frame arrival correct
			from_physical_layer(&r);
			if(r.seq == frame_expected){
				to_newwork_layer(&r.info);
				inc(frame_expected);
			}
			if(r.ack == next_frame_to_send){
				stop_timer(s.seq);
				from_network_layer(&buffer);
				inc(next_frame_to_send);
			}
		}

		s.info = buffer;
		s.seq = next_frame_to_send;
		s.ack = 1-frame_expected;
		to_physical_layer(&s);
		start_timer(s.seq);
	}
}