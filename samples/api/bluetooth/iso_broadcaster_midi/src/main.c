#include <zephyr/types.h>
#include <zephyr/device.h>
#include <soc.h>

#include <test_gpio.h>
#include <dk_buttons_and_leds.h>
#include <midi/midi.h>
#include <midi/midi_iso.h>
#include <midi/midi_parser.h>
#include <midi/midi_sysex.h>
#include <midi/midi_ump.h>
#include <midi/midi_ci.h>
#include <zephyr/drivers/spi.h>

#include <zephyr/logging/log.h>

#define LOG_MODULE_NAME midi
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

#define RUN_STATUS_LED DK_LED1
#define CON_STATUS_LED DK_LED2
#define RUN_LED_BLINK_INTERVAL 1000

#define UMP_NAME "nrf midi broadcaster"
#define FUNCTION_BLOCK_NAME "broadcaster function block"

#define SPI_NODE	DT_NODELABEL(spi0)

static void spi_thread_fn(void *p1, void *p2, void *p3);
static void rx_thread_fn(void *p1, void *p2, void *p3);

K_THREAD_DEFINE(spi_thread, 512,  spi_thread_fn, 
		NULL, NULL, NULL, K_PRIO_COOP(1), 0, 0);

K_THREAD_DEFINE(rx_thread, 512, rx_thread_fn, 
		NULL, NULL, NULL, K_PRIO_COOP(1), 0, 0);

static K_FIFO_DEFINE(esb_spi_fifo);

K_SEM_DEFINE(device_discovered_sem, 0, 1);
K_SEM_DEFINE(init_sem, 0, 2);

const struct device *serial_midi_in_dev;
const struct device *serial_midi_out_dev;
const struct device *iso_midi_out_dev;
const struct device *spi_dev;

#define MAX_NUM_MUID 2
uint8_t num_muid = 0;
uint32_t remote_muid[] = {0, 0};

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

static int midi_iso_sent(const struct device *dev,
			  midi_msg_t *msg,
			  void *user_data)
{
	int err;

	LOG_DBG("MIDI iso sent!");
	midi_msg_unref(msg);

	return 0;
}

static int midi_serial_sent(const struct device *dev,
			  midi_msg_t *msg,
			  void *user_data)
{
	LOG_DBG("MIDI serial sent!");
	midi_msg_unref(msg);

	return 0;
}

midi_msg_t *midi_1_0_to_ump(midi_msg_t *msg, uint8_t channel)
{
	midi_status_t *status;
	midi_msg_t *ump_msg;

	if (msg->format != MIDI_FORMAT_1_0_PARSED)
	{
		return NULL;
	}

	status = (midi_status_t*)msg->data;
	switch (status->opcode)
	{
	case MIDI_OP_NOTE_OFF:
	case MIDI_OP_NOTE_ON:
	case MIDI_OP_POLYPHONIC_AFTERTOUCH:
	case MIDI_OP_CONTROL_CHANGE:
	case MIDI_OP_PITCH_BEND:
		ump_msg = midi_ump_1_0_channel_voice_msg_create_alloc(0, status->opcode, 
								channel, msg->data[1], msg->data[2], &remote_muid[0]);
		break;
	case MIDI_OP_PROGRAM_CHANGE:
	case MIDI_OP_CHANNEL_PRESSURE:
		ump_msg = midi_ump_1_0_channel_voice_msg_create_alloc(0, status->opcode, 
									channel, msg->data[1], 0, &remote_muid[0]);
		break;
	default:
		ump_msg = NULL;
		break;
	}
	return ump_msg;
}


static int midi_serial_received(const struct device *dev,
			  midi_msg_t *msg,
			  void *user_data)
{
	midi_msg_t *msg_channel_voice;
	midi_msg_t *parsed_msg = midi_parse_serial(msg, &serial_parser);
	midi_msg_unref(msg);

	midi_ump_function_block_t remote_function_block;
	
	if (parsed_msg) {
		msg_channel_voice = midi_1_0_to_ump(parsed_msg, 0);
		midi_msg_unref(parsed_msg);

		if (msg_channel_voice)
		{
			midi_send(iso_midi_out_dev, msg_channel_voice);
		}
	}
	
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
		return NULL;
	}

	err = midi_callback_set(dev, cb, user_data);
	if (err != 0) {
		LOG_ERR("Can not set callbacks for device: %s", name);
	}

	return dev;
}

static int handle_midi_ci(midi_msg_t *msg)
{
	int err;
	midi_ump_function_block_t *remote_function_block;	
	midi_ci_msg_header_t * header = midi_ci_parse_msg_header(msg);

	switch (header->universal_sysex_header.sub_id_2)
	{
	case MIDI_CI_MANAGEMENT_DISCOVERY:

		break;
	case MIDI_CI_MANAGEMENT_DISCOVERY_REPLY:
		if(midi_ump_get_remote_function_block_by_muid(&function_block, 
					MIDI_DATA_ARRAY_4_BYTE_TO_INTEGER(header->source_muid))) {
			LOG_INF("Remote function block already exists");
			break;
		}

		remote_function_block = midi_ci_parse_function_block_from_msg(msg);
		err = midi_ump_add_remote_function_block(&function_block, remote_function_block);

		
		remote_muid[num_muid] = remote_function_block->muid;
		num_muid++;

		LOG_INF("Remote function block added, %x", remote_function_block->muid);

		if (num_muid == MAX_NUM_MUID)
		{
			LOG_INF("All devices discovered!");
			k_sem_give(&device_discovered_sem);
		}
		
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

static void rx_thread_fn(void *p1, void *p2, void *p3)
{
	midi_msg_t *msg;
	int err;
	
	k_sem_take(&init_sem, K_FOREVER);

	while (1)
	{
		msg = k_fifo_get(&esb_spi_fifo, K_FOREVER);
		if (msg)
		{
			if (msg->data[0] == 0xF0)
			{
				LOG_DBG("SysEx message received!");
				msg->format = MIDI_FORMAT_1_0_PARSED;
				handle_sysex(msg);
			}

			if (msg->data[0] == 0xFF)
			{
				midi_iso_ack_msg(msg->data[1], msg->data[2]);
				LOG_INF("Ack msg received! muid: %x msg id: %x,", msg->data[1], msg->data[2]);
			}
			midi_msg_unref(msg);
		}
	}
}

static void spi_thread_fn(void *p1, void *p2, void *p3)
{
	uint8_t len;
	
	k_sem_take(&init_sem, K_FOREVER);

	struct spi_config config = (struct spi_config){
		.frequency = 125000,
		.operation = SPI_OP_MODE_SLAVE | SPI_WORD_SET(8),
		.slave = 0,
		.cs = NULL,
	};

	while (1)
	{
		midi_msg_t *msg;
		msg = midi_msg_alloc(NULL, 48);

		struct spi_buf rx_buf = {
			.buf = msg->data,
			.len = 48,
		};
		
		struct spi_buf_set rx_bufs = {
			.buffers = &rx_buf,
			.count = 1,
		};

		len = spi_read(spi_dev, &config, &rx_bufs);
		msg->len = len;

		k_fifo_put(&esb_spi_fifo, msg);
	}
}

void main(void)
{
	int blink_status = 0;
	int err;
	midi_msg_t *msg_discovery;

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
	iso_midi_out_dev = get_port("ISO_MIDI_BROADCASTER", 
			midi_iso_sent, NULL);

	LOG_INF("ports gotten");

	k_fifo_init(&esb_spi_fifo);

	err = dk_leds_init();
	if (err) {
		LOG_ERR("Cannot init LEDs (err: %d)", err);
	}

	test_gpio_init(13);
	test_gpio_init(6);

	k_sem_give(&init_sem);
	k_sem_give(&init_sem);

	msg_discovery = midi_ci_discovery_msg_create_alloc(&function_block);

	LOG_INF("MIDI iso broadcasting sample running");

	for (;;) {
		dk_set_led(RUN_STATUS_LED, (++blink_status) % 2);
		k_sleep(K_MSEC(RUN_LED_BLINK_INTERVAL));
		if (num_muid < MAX_NUM_MUID)
		{
			LOG_INF("Sending discovery message");
			msg_discovery = midi_msg_ref(msg_discovery);
			midi_send(iso_midi_out_dev, msg_discovery);
		}
	}
}

#if CONFIG_MIDI_TEST_MODE

static void test_thread_fn(void *p1, void *p2, void *p3);

K_THREAD_DEFINE(test_thread, 512, test_thread_fn, 
	NULL, NULL, NULL, K_PRIO_COOP(1), 0, 0);

static void test_thread_fn(void *p1, void *p2, void *p3)
{
	uint8_t msg_num = CONFIG_MIDI_TEST_MSG_NUM;
	midi_msg_t *test_msg[CONFIG_MIDI_TEST_MSG_NUM];
	midi_ump_1_0_channel_voice_msg_t *msg_data;
	uint8_t note_value = 0;

	for (size_t i = 0; i < CONFIG_MIDI_TEST_MSG_NUM; i++) {
		test_msg[i] = midi_ump_1_0_channel_voice_msg_create_alloc(0, 
		MIDI_OP_NOTE_ON, 0, 0x40, 0x020 + i, &remote_muid[i % MAX_NUM_MUID]);
	}

	k_sem_take(&device_discovered_sem, K_FOREVER);
	while (1)
	{	
		note_value++;
		#if CONFIG_MIDI_TEST_MSG_INTERVAL_RANDOM
		k_sleep(K_MSEC(sys_rand32_get() % CONFIG_MIDI_TEST_MSG_INTERVAL));
		#else
		k_sleep(K_MSEC(CONFIG_MIDI_TEST_MSG_INTERVAL));
		#endif /* MIDI_TEST_MSG_INTERVAL_RANDOM */

		for (int i = 0; i < msg_num; i++) {
			msg_data = (midi_ump_1_0_channel_voice_msg_t*)test_msg[i]->data;
			msg_data->channel_voice_msg.data_1 = note_value % 127;
			test_msg[i] = midi_msg_ref(test_msg[i]);
			midi_send(iso_midi_out_dev, test_msg[i]);
			
			#if CONFIG_MIDI_TEST_MSG_SPACING_RANDOM
			k_sleep(K_USEC(sys_rand32_get() % CONFIG_MIDI_TEST_MSG_SPACING));
			#else
			k_sleep(K_USEC(CONFIG_MIDI_TEST_MSG_SPACING));
			#endif /* MIDI_TEST_MSG_SPACING_RANDOM */
		}
	}
}
#endif /* CONFIG_MIDI_TEST_MODE */

