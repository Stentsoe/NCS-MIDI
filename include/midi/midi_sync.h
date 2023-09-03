/**
 * @file midi_parser.h
 *
 * @defgroup midi_parser MIDI parser
 * @{
 * @brief Basic parser for MIDI streams.
 */
#ifndef MIDI_SYNC_H__
#define MIDI_SYNC_H__


// #include <stdbool.h>
#include <zephyr.h>
#include <zephyr/types.h>
#include <midi/midi.h>


#ifdef __cplusplus
extern "C" {
#endif

enum midi_sync_enable {
	SYNC_ON,
	SYNC_OFF
};

enum midi_sync_running_status {
	SYNC_ENFORCE_RUNNING_STATUS_ON,
	SYNC_ENFORCE_RUNNING_STATUS_OFF,
};

enum midi_sync_resolution {
	SYNC_MICROSECOND,
	SYNC_MILLISECOND
};

struct midi_sync_cfg {
	enum midi_sync_enable enable;
	enum midi_sync_running_statu running_status;
	enum midi_sync_resolution resolution;
	uint8_t timestamp_width;
};

/** @brief Struct holding the state of the MIDI parser instance. */
struct midi_sync {
	struct k_thread thread;
	midi_transfer_t in;
	midi_transfer_t out;
	struct midi_sync_cfg cfg;
};

/**
 * @brief Init midi synchronizer instance
 */
int midi_sync_init(struct midi_sync *sync, struct midi_sync_cfg cfg);


/** @} */

#ifdef __cplusplus
}
#endif

#endif /* MIDI_SYNC_H__ */
