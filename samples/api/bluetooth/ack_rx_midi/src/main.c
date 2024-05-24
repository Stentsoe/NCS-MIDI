#include <zephyr/types.h>
#include <zephyr/device.h>
#include <soc.h>

#include <zephyr/drivers/clock_control.h>
#include <zephyr/drivers/clock_control/nrf_clock_control.h>

#include <dk_buttons_and_leds.h>
#include <midi/midi.h>
#include <esb.h>
#include <zephyr/drivers/spi.h>

#include <test_gpio.h>

#include <zephyr/logging/log.h>

#define LOG_MODULE_NAME ack_rx
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

#define RUN_STATUS_LED DK_LED1
#define RUN_LED_BLINK_INTERVAL 5000
#define STACK_SIZE 512

static K_FIFO_DEFINE(esb_spi_fifo);

#define SPI_NODE	DT_NODELABEL(spi0)

struct payload_data {
	void* fifo_reserved;
	struct esb_payload payload;
};

const struct device *spi_dev;

static int spi_send(const struct device *dev, void *data, size_t len)
{
	int err;

	struct spi_cs_control cs  = (struct spi_cs_control){
		.gpio = GPIO_DT_SPEC_GET(SPI_NODE, cs_gpios),
		.delay = 0u,
	};

	struct spi_config config = (struct spi_config){
		.frequency = 125000*2,
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
	LOG_HEXDUMP_INF(tx_bufs.buffers->buf, len, "write:");
	err = spi_write(dev, &config, &tx_bufs);
	LOG_INF("spi_write; err: %d", err);
	return err;
}

void esb_handler(struct esb_evt const *event)
{
	struct payload_data *data;
	// test_gpio_toggle(13);
	switch (event->evt_id)
	{
	case ESB_EVENT_TX_SUCCESS:
		LOG_DBG("TX SUCCESS EVENT");
		break;
	case ESB_EVENT_TX_FAILED:
		LOG_DBG("TX FAILED EVENT");
		break;
	case ESB_EVENT_RX_RECEIVED:
		data = k_malloc(sizeof(struct payload_data));
		if (esb_read_rx_payload(&data->payload) == 0) {
			test_gpio_toggle(13);
			// LOG_HEXDUMP_INF(data->payload.data, data->payload.length, "RX:");
			k_fifo_put(&esb_spi_fifo, data);
		} else {
			LOG_ERR("Failed to read payload");
		}

		break;
	
	default:
		break;
	}
	return;
}


int midi_ack_initialize(void)
{
	int err;

	uint8_t base_addr_0[4] = {0xE7, 0xE7};
	uint8_t base_addr_1[4] = {0xC2, 0xC2};
	uint8_t addr_prefix[8] = {0xE7, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8};
	uint8_t addr_len = 2;

	struct esb_config config = ESB_DEFAULT_CONFIG;
	config.bitrate = ESB_BITRATE_2MBPS;	 
	config.event_handler = esb_handler;
	config.mode = ESB_MODE_PRX;
	config.selective_auto_ack = true;
	config.payload_length = 64;
	config.retransmit_count = 0;

	err = esb_init(&config);
	if (err) {
		return err;
	}

	esb_set_address_length(addr_len);

	err = esb_set_base_address_0(base_addr_0);
	if (err) {
		return err;
	}

	err = esb_set_base_address_1(base_addr_1);
	if (err) {
		return err;
	}

	err = esb_set_prefixes(addr_prefix, ARRAY_SIZE(addr_prefix));
	if (err) {
		return err;
	}

	return err;
}

int clocks_start(void)
{
	int err;
	int res;
	struct onoff_manager *clk_mgr;
	struct onoff_client clk_cli;

	clk_mgr = z_nrf_clock_control_get_onoff(CLOCK_CONTROL_NRF_SUBSYS_HF);
	if (!clk_mgr) {
		LOG_ERR("Unable to get the Clock manager");
		return -ENXIO;
	}

	sys_notify_init_spinwait(&clk_cli.notify);

	err = onoff_request(clk_mgr, &clk_cli);
	if (err < 0) {
		LOG_ERR("Clock request failed: %d", err);
		return err;
	}

	do {
		err = sys_notify_fetch_result(&clk_cli.notify, &res);
		if (!err && res) {
			LOG_ERR("Clock could not be started: %d", res);
			return res;
		}
	} while (err);

	LOG_INF("HF clock started");
	return 0;
}

void main(void)
{
	int blink_status = 0;
	int err;
	struct payload_data *data;
	
	LOG_INF("midi ack tx sample");

	spi_dev = DEVICE_DT_GET(SPI_NODE);
	if (!device_is_ready(spi_dev)) {
		LOG_INF("device not ready.");
	}

	k_fifo_init(&esb_spi_fifo);

	err = clocks_start();
	if (err) {
		return;
	}

	err = midi_ack_initialize();
	if (err) {
		LOG_ERR("Cannot init MIDI ACK (err: %d)", err);
	}

	err = dk_leds_init();
	if (err) {
		LOG_ERR("Cannot init LEDs (err: %d)", err);
	}

	test_gpio_init(13);
	test_gpio_init(6);

	err = esb_start_rx();
	if (err) {
		LOG_ERR("ESB start rx failed, err %d", err);
	}

	for (;;) {
		dk_set_led(RUN_STATUS_LED, (++blink_status) % 2);
		data = k_fifo_get(&esb_spi_fifo, K_MSEC(RUN_LED_BLINK_INTERVAL));
		if (data) {
			LOG_HEXDUMP_INF(data->payload.data, data->payload.length, "Tx loop:");
			spi_send(spi_dev, data->payload.data, data->payload.length);
			k_free(data);
		}
		LOG_INF("running");
	}
}