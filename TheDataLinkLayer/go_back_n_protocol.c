#define MAX_SEQ	7
typedef enum {frame_arrival, cksum_err, timeout, network_layer_ready} event_type;
#include "protocol.h"

static boolean between(seq_nr a, seq_nr b, seq_nr c) {
	/* Return true if a <= b < c circularly;false otherwise */
	if(((a <= b) && (b < c)) || ((c < a) && (b < c)) || ((c < a) && (a <= b)))
		return true;
	else
		return false;
}

static void send_data(seq_nr frame_nr, seq_nr frame_expected, packet buffer[]) {
	/* Construct and send a data frame */
	frame s;
	s.info = buffer[frame_nr];
	s.seq = frame_nr;
	s.ack = (frame_expected + MAX_SEQ) % (MAX_SEQ + 1);
	to_physical_layer(&s);
	start_timer(frame_nr);
}

void go_back_n(void){
	seq_nr next_frame_to_send;
	seq_nr ack_expected;
	seq_nr frame_expected;
	seq_nr nbuffed;
	event_type event;
	frame s;
	packet buffer[MAX_SEQ + 1];

	next_frame_to_send = 0;
	ack_expected = 0;
	frame_expected = 0;
	nbuffed = 0;
	while(true) {
		wait_for_event(&event);
		switch(event) {
			case network_layer_ready:
				from_network_layer(&buffer[next_frame_to_send]);
				send_data(next_frame_to_send, frame_expected, buffer);
				inc(next_frame_to_send);
				nbuffed += 1;
				break;

			case frame_arrival:
				from_physical_layer(&s);
				if(s.seq == frame_expected) {
					to_newwork_layer(&s.info);
					inc(frame_expected);
				}

				while(between(ack_expected, s.ack, next_frame_to_send)){
					ack_expected += 1;
					nbuffed -= 1;
					stop_timer(s.seq);
				}
				break;

			case cksum_err:	break;

			case timeout:
				next_frame_to_send = ack_expected;
				for(int i = 1; i <= nbuffed; ++i) {
					send_data(next_frame_to_send, frame_expected, buffer);
					inc(next_frame_to_send);
				}
				break;
		}

		if(nbuffed < MAX_SEQ)
			enable_network_layer();
		else:
			disable_network_layer();
	}
}