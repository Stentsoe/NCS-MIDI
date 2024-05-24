#include <zephyr/types.h>
#include <zephyr/device.h>
#include <soc.h>

#include <dk_buttons_and_leds.h>
#include <midi/midi.h>
#include <midi/midi_iso.h>
#include <midi/midi_parser.h>
#include <midi/midi_sysex.h>
#include <midi/midi_ump.h>
#include <midi/midi_ci.h>
#include <zephyr/drivers/spi.h>
#include <hal/nrf_gpio.h>
#include "test_gpio.h"

#include <zephyr/logging/log.h>

#define LOG_MODULE_NAME midi
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

#define RUN_STATUS_LED DK_LED1
#define CON_STATUS_LED DK_LED2
#define RUN_LED_BLINK_INTERVAL 1000

const struct device *serial_midi_in_dev;
const struct device *serial_midi_out_dev;
const struct device *iso_midi_in_dev;
const struct device *spi_dev;

static K_FIFO_DEFINE(fifo_event_execution);

#define UMP_NAME "nrf midi receiver"
#define FUNCTION_BLOCK_NAME "receiver function block"

#define SPI_NODE	DT_NODELABEL(spi0)

DEFINE_MIDI_PARSER_SERIAL(serial_parser);

DEFINE_MIDI_UMP_ENDPOINT(ump_endpoint, UMP_NAME, CONFIG_PRODUCT_ID, 0x07,
	0x08, 0x09, 0x00, 0x01, 0x01, MIDI_PROTOCOL_1_0, 0x01, 
	(MIDI_UMP_CI_CATEGORY_SUPPORTED_PROFILE_CONFIGURATION | 
 	MIDI_UMP_CI_CATEGORY_SUPPORTED_PROPERTY_EXCHANGE), false, false, 0xFF); 

DEFINE_MIDI_UMP_FUNCTION_BLOCK(function_block, FUNCTION_BLOCK_NAME,
			MIDI_UMP_FUNCTION_BLOCK_BIDIRECTIONAL, 
			MIDI_UMP_FUNCTION_BLOCK_MIDI_1_0_31250_BPS, 
			MIDI_UMP_FUNCTION_BLOCK_RECEIVER_SENDER, 
			0x00, 0x01, 0x02, 
			0x01);

static int spi_send(const struct device *dev, void *data, size_t len)
{
	int err;

	struct spi_cs_control cs  = (struct spi_cs_control){
		.gpio = GPIO_DT_SPEC_GET(SPI_NODE, cs_gpios),
		.delay = 0u,
	};

	struct spi_config config = (struct spi_config){
		.frequency = 125000,
		.operation = SPI_OP_MODE_MASTER | SPI_WORD_SET(8),
		.slave = 0,
		.cs = cs,
	};
	

	struct spi_buf tx_buf = {
		.buf = data,
		.len = len,
	};

	struct spi_buf_set tx_bufs = {
		.buffers = &tx_buf,
		.count = 1,
	};

	err = spi_write(dev, &config, &tx_bufs);
	if (err)
	{
		("spi_write; err: %d", err);
	}
	
	return err;
}

static int handle_midi_ci(midi_msg_t *msg)
{
	int err;
	midi_ump_function_block_t *remote_function_block;	
	midi_ci_msg_header_t * header = midi_ci_parse_msg_header(msg);

	switch (header->universal_sysex_header.sub_id_2)
	{
	case MIDI_CI_MANAGEMENT_DISCOVERY:
		midi_msg_t * reply_msg;

		if(midi_ump_get_remote_function_block_by_muid(&function_block, 
					MIDI_DATA_ARRAY_4_BYTE_TO_INTEGER(header->source_muid))) {
			LOG_DBG("Remote function block already exists");
			break;
		}

		remote_function_block = midi_ci_parse_function_block_from_msg(msg);
		err = midi_ump_add_remote_function_block(&function_block, remote_function_block);
		LOG_INF("Remote function block added");
		
		reply_msg = midi_ci_discovery_reply_msg_create_alloc(&function_block, remote_function_block);
		spi_send(spi_dev, reply_msg->data, reply_msg->len);
		midi_msg_unref(reply_msg);
		break;
	case MIDI_CI_MANAGEMENT_DISCOVERY_REPLY:

		break;
	
	default:
		break;
	}
	return err;
}

static int handle_sysex(midi_msg_t *msg)
{
	midi_sysex_universal_msg_header_t *sysex_header;
	sysex_header = midi_sysex_universal_msg_header_parse(msg);
	switch (sysex_header->sub_id_1)
	{
	case MIDI_SYSEX_SUB_ID_1_MIDI_CI:
		handle_midi_ci(msg);
		break;
	
	default:
		break;
	}
	return 0;
}

midi_msg_t * midi_ump_to_1_0(midi_msg_t *ump_msg)
{
	midi_status_t *status;
	midi_msg_t *msg;

	if (ump_msg->format != MIDI_FORMAT_2_0_UMP)
	{
		return NULL;
	}

	status = (midi_status_t*)&ump_msg->data[1];
	
	switch (status->opcode)
	{
	case MIDI_OP_NOTE_OFF:
	case MIDI_OP_NOTE_ON:
	case MIDI_OP_POLYPHONIC_AFTERTOUCH:
	case MIDI_OP_CONTROL_CHANGE:
	case MIDI_OP_PITCH_BEND:
		msg = midi_msg_init_alloc(NULL, 3, MIDI_FORMAT_1_0_PARSED_DELTA_US, NULL);
		memcpy(msg->data, ump_msg->data + 1, 3);
		break;
	case MIDI_OP_PROGRAM_CHANGE:
	case MIDI_OP_CHANNEL_PRESSURE:
		msg = midi_msg_init_alloc(NULL, 2, MIDI_FORMAT_1_0_PARSED_DELTA_US, NULL);
		memcpy(msg->data, ump_msg->data + 1, 2);
		break;
	default:
		msg = NULL;
		break;
	}
	msg->timestamp = ump_msg->timestamp;
	return msg;
}

static int midi_iso_received(const struct device *dev,
			  midi_msg_t *msg,
			  void *user_data)
{
	int err;
	uint8_t spi_ack_msg[4];
	midi_msg_t *midi_1_0_msg;

	LOG_DBG("MIDI iso received!");
	if (msg) {
		
		LOG_HEXDUMP_DBG(msg->data, msg->len, "Received message:");
	
		if (msg->format == MIDI_FORMAT_1_0_PARSED_DELTA_US)
		{
			switch (msg->data[0])
			{
			case MIDI_SYSEX_START:
				handle_sysex(msg);
				break;
			
			default:
				break;
			}
		} 

		if (msg->format == MIDI_FORMAT_2_0_UMP)
		{
			if (msg->timestamp != 0xFF)
			{
				midi_msg_ref(msg);
				k_fifo_put(&fifo_event_execution, msg);
				midi_1_0_msg = midi_ump_to_1_0(msg);
				midi_send(serial_midi_out_dev, midi_1_0_msg);

				
			} else {
				LOG_INF("Received duplicate message2!!!. Discarding.");
			}
			
			if(msg->ack_channel != 0xFF)
			{
				spi_ack_msg[0] = 0xFF;
				spi_ack_msg[1] = function_block.muid & 0xFF;
				spi_ack_msg[2] = msg->num;
				spi_ack_msg[3] = msg->ack_channel;
				LOG_DBG("Sending ack message. muid %x, num: %x, channel: %x", spi_ack_msg[1], spi_ack_msg[2], spi_ack_msg[3]);
				spi_send(spi_dev, spi_ack_msg, 4);
			}
		}
	}

	midi_msg_unref_alt(msg);

	return 0;
}

static int midi_serial_sent(const struct device *dev,
			  midi_msg_t *msg,
			  void *user_data)
{
	LOG_DBG("MIDI SERIAL sent!");
	midi_msg_unref(msg);

	return 0;
}

static int midi_serial_received(const struct device *dev,
			  midi_msg_t *msg,
			  void *user_data)
{
	midi_msg_unref(msg);

	return 0;
}

const struct device * get_port(const char* name, 
			midi_transfer cb, void *user_data)
{
	int err;
	const struct device * dev;

	dev = device_get_binding(name);
	if (!dev) {
		LOG_ERR("Can not get device: %s", name);
	}

	err = midi_callback_set(dev, cb, user_data);
	if (err != 0) {
		LOG_ERR("Can not set callbacks for device: %s", name);
	}

	return dev;
}

void main(void)
{
	int blink_status = 0;
	int err;

	spi_dev = DEVICE_DT_GET(SPI_NODE);
	if (!device_is_ready(spi_dev)) {
		LOG_INF("device not ready.");
	}

	midi_ump_add_function_block(&ump_endpoint, &function_block);
	midi_iso_set_function_block(&function_block);

	/*SERIAL PORTS*/
	serial_midi_in_dev = get_port("SERIAL_MIDI_IN", 
			midi_serial_received, NULL);
	
	serial_midi_out_dev = get_port("SERIAL_MIDI_OUT", 
			midi_serial_sent, NULL);

	/*ISO PORTS*/
	iso_midi_in_dev = get_port("ISO_MIDI_RECEIVER", 
			midi_iso_received, NULL);

	LOG_INF("ports gotten");

	err = dk_leds_init();
	if (err) {
		LOG_ERR("Cannot init LEDs (err: %d)", err);
	}
	
	test_gpio_init(13);
	test_gpio_init(6);

	midi_iso_scan(iso_midi_in_dev);

	for (;;) {
		dk_set_led(RUN_STATUS_LED, (++blink_status) % 2);
		k_sleep(K_MSEC(RUN_LED_BLINK_INTERVAL));
	}
}

static void event_execution_thread_fn(void *p1, void *p2, void *p3);

K_THREAD_DEFINE(event_execution_thread, 512, event_execution_thread_fn, 
	NULL, NULL, NULL, K_PRIO_PREEMPT(2), 0, 0);

static void event_execution_thread_fn(void *p1, void *p2, void *p3)
{
	midi_msg_t *msg;
	while (1)
	{
		msg = k_fifo_get(&fifo_event_execution, K_FOREVER);
		if (msg)
		{
			k_sleep(K_USEC(msg->timestamp));
			test_gpio_toggle(13);
			midi_msg_unref_alt(msg);
		}
	}
}