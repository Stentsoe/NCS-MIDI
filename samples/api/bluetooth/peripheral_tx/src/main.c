#include <zephyr/kernel.h>
#include <zephyr/types.h>
#include <zephyr/device.h>
#include <soc.h>

#include <zephyr/drivers/clock_control.h>
#include <zephyr/drivers/clock_control/nrf_clock_control.h>
#include <zephyr/random/rand32.h>

#include <hal/nrf_timer.h>
#include <nrfx_timer.h>

#include <dk_buttons_and_leds.h>
#include <midi/midi.h>
#include <esb.h>
#include <zephyr/drivers/spi.h>
#include <hal/nrf_gpio.h>

#include <zephyr/logging/log.h>

#define LOG_MODULE_NAME ack_tx
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

#define RUN_STATUS_LED DK_LED1
#define RUN_LED_BLINK_INTERVAL 5000

#define SPI_NODE	DT_NODELABEL(spi0)

#define _RADIO_SHORTS_COMMON                                               \
	(RADIO_SHORTS_READY_START_Msk | RADIO_SHORTS_END_DISABLE_Msk |         \
	 RADIO_SHORTS_ADDRESS_RSSISTART_Msk |                                  \
	 RADIO_SHORTS_DISABLED_RSSISTOP_Msk)

static const struct device *gpio_dev = DEVICE_DT_GET(DT_NODELABEL(gpio0));

static K_FIFO_DEFINE(fifo_tx_open_random_delay_data);
static K_FIFO_DEFINE(fifo_tx_open_data_0);
static K_FIFO_DEFINE(fifo_tx_open_data_1);
static K_FIFO_DEFINE(fifo_tx_open_data_2);
static K_FIFO_DEFINE(fifo_tx_ack);


static void spi_thread_fn(void *p1, void *p2, void *p3);
static void esb_delayed_tx_thread_fn(void *p1, void *p2, void *p3);
static void esb_tx_thread_fn(void *p1, void *p2, void *p3);

K_THREAD_DEFINE(spi_thread, 512, spi_thread_fn, 
		NULL, NULL, NULL, K_PRIO_COOP(1), 0, 0);

K_THREAD_DEFINE(esb_delayed_tx_thread, 512, esb_delayed_tx_thread_fn, 
	NULL, NULL, NULL, K_PRIO_COOP(1), 0, 0);

K_THREAD_DEFINE(esb_tx_thread, 512, esb_tx_thread_fn, 
	NULL, NULL, NULL, K_PRIO_COOP(2), 0, 0);

K_SEM_DEFINE(esb_delayed_tx_sem, 0, 1);
K_SEM_DEFINE(esb_tx_sem, 0, 1);

K_SEM_DEFINE(init_sem, 0, 3);

struct k_fifo * tx_open_fifos[3];

static struct gpio_callback gpio_cb;

static nrfx_timer_t time_slice_timer = NRFX_TIMER_INSTANCE(3);

struct payload_data {
	void* fifo_reserved;
	struct esb_payload payload;
};

uint8_t current_slice;
uint8_t current_phase;

const struct device *spi_dev;

uint32_t get_time(uint32_t duration)
{
	uint32_t current = nrfx_timer_capture(&time_slice_timer, NRF_TIMER_CC_CHANNEL0);
	return current + nrfx_timer_us_to_ticks(&time_slice_timer, duration);
}

static void time_slice_timer_handler(nrf_timer_event_t event_type, void *context)
{
	if (event_type == NRF_TIMER_EVENT_COMPARE1) 
	{
		nrf_timer_event_clear(time_slice_timer.p_reg, NRF_TIMER_EVENT_COMPARE1);
		k_sem_give(&esb_tx_sem);
	}

	if (event_type == NRF_TIMER_EVENT_COMPARE2) 
	{
		nrf_timer_event_clear(time_slice_timer.p_reg, NRF_TIMER_EVENT_COMPARE2);
		k_sem_give(&esb_tx_sem);
	}
}

void esb_handler(struct esb_evt const *event)
{
	int err = 0;
	LOG_INF("esb_handler");
	struct esb_payload rx_payload;
	switch (event->evt_id)
	{
	case ESB_EVENT_TX_SUCCESS:
		LOG_INF("TX SUCCESS EVENT");
		if (err) {
			LOG_ERR("ESB pop tx failed, err %d", err);
		}
		break;
	case ESB_EVENT_TX_FAILED:
		LOG_INF("TX FAILED EVENT");
		break;
	case ESB_EVENT_RX_RECEIVED:
		if (esb_read_rx_payload(&rx_payload) == 0) {
			LOG_HEXDUMP_INF(rx_payload.data, rx_payload.length, "RX:");
		} else {
			LOG_ERR("Failed to read payload");
		}
		break;
	
	default:
		break;
	}
	return;
}

static void gpio_cb_func(const struct device *dev, struct gpio_callback *gpio_callback,
			 uint32_t pins)
{
	nrfx_timer_clear(&time_slice_timer);
	current_slice = 0;
	current_phase = 0;
	k_sem_give(&esb_tx_sem);
}

void random_delay_esb_send(struct payload_data *data)
{
	LOG_INF("random_delay_esb_send %p %d", data, data->payload.length);
	k_fifo_put(&fifo_tx_open_random_delay_data, data);
}

void ack_esb_send(struct payload_data *data)
{
	int err;
	data->payload.length = 3;
	// err = gpio_pin_toggle(gpio_dev, 6);
	// if (err) {
	// 	LOG_ERR("GPIO_0 toggle error: %d", err);
	// }
	k_fifo_put(&fifo_tx_ack, data);
}

int esb_tx_open(uint8_t slice)
{
	int err;
	struct payload_data *data;
	data = k_fifo_get(tx_open_fifos[slice], K_NO_WAIT);
	if (data) {
		LOG_HEXDUMP_DBG(data->payload.data, data->payload.length, "Tx open:");
		err = esb_write_payload(&data->payload);
		if (err) {
			LOG_ERR("Payload write failed, err %d", err);
			return err;
		}
		k_free(data);
	}
	return 0;
}

uint8_t dropout_counter = 0;

int esb_tx_ack(uint8_t slice)
{
	int err;
	struct payload_data *data;

	data = k_fifo_peek_head(&fifo_tx_ack);
	if(data)
	{
		if (data->payload.data[3] == slice) {
			data = k_fifo_get(&fifo_tx_ack, K_NO_WAIT);
			if (data) {
				#if CONFIG_MIDI_TEST_MODE
				if (((++dropout_counter) 
							% CONFIG_MIDI_TEST_ACK_DROPOUT_INTERVAL) != 0) {

					err = esb_write_payload(&data->payload);
					if (err) {
						LOG_ERR("Payload write failed, err %d", err);
						return err;
					}
				} else {
					LOG_INF("DROPPED ACK");
					err = gpio_pin_toggle(gpio_dev, 13);
					if (err) {
						LOG_ERR("GPIO_0 toggle error: %d", err);
					}
				}

				#else /* CONFIG_MIDI_TEST_MODE */
				err = esb_write_payload(&data->payload);
				if (err) {
					LOG_ERR("Payload write failed, err %d", err);
					return err;
				}
				#endif /* CONFIG_MIDI_TEST_MODE */

				k_free(data);
			}
		}
	}
	return 0;
}

void tx_phase_timer(uint32_t phase_time, uint32_t slice_time, 
						uint8_t num_slices, uint8_t num_phases)
{
	if (current_slice == 0)
	{
		if (current_phase < num_phases - 1)
		{
			nrfx_timer_compare(&time_slice_timer, NRF_TIMER_CC_CHANNEL1, 
								get_time(phase_time), true);
		}
		nrfx_timer_compare(&time_slice_timer, NRF_TIMER_CC_CHANNEL2, 
							get_time(slice_time), true);
		current_slice++;
	} else if (current_slice == num_slices - 1)
	{
		if (current_phase < num_phases - 1)
		{
			current_phase ++;
		} else
		{
			current_phase = 0;
		}
		current_slice = 0;
	} else
	{
		nrfx_timer_compare(&time_slice_timer, NRF_TIMER_CC_CHANNEL2, get_time(slice_time), true);
		current_slice++;
	}
}

int clocks_init(void)
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

int midi_esb_init(void)
{
	int err;

	uint8_t base_addr_0[4] = {0xE7, 0xE7};
	uint8_t base_addr_1[4] = {0xC2, 0xC2};
	uint8_t addr_prefix[8] = {0xE7, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8};

	uint8_t addr_len = 2;

	struct esb_config config = ESB_DEFAULT_CONFIG;

	config.bitrate = ESB_BITRATE_2MBPS;	 
	config.use_fast_ramp_up = true;
	config.selective_auto_ack = true;
	config.event_handler = esb_handler;
	config.tx_mode = ESB_TXMODE_AUTO;
	config.payload_length = 64;
	config.retransmit_count = 0;
	config.retransmit_delay = 80;

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

static int sys_timer_init(void)
{
	nrfx_err_t nrfx_err;
	const nrfx_timer_config_t config = {
		.frequency = NRFX_MHZ_TO_HZ(1),
		.mode = NRF_TIMER_MODE_TIMER,
		.bit_width = NRF_TIMER_BIT_WIDTH_32,
	};

	nrfx_err = nrfx_timer_init(&time_slice_timer, &config, time_slice_timer_handler);
	if (nrfx_err != NRFX_SUCCESS) {
		LOG_ERR("Failed to initialize nrfx timer (err %d)", nrfx_err);
		return -EFAULT;
	}

	IRQ_CONNECT(NRFX_CONCAT_3(TIMER, 3, _IRQn), 3,
		    NRFX_CONCAT_3(nrfx_timer_, 3, _irq_handler), NULL, 0);

	return 0;
}

static int buffer_init(void)
{
	tx_open_fifos[0] = &fifo_tx_open_data_0; 
	tx_open_fifos[1] = &fifo_tx_open_data_1;
	tx_open_fifos[2] = &fifo_tx_open_data_2;

	k_fifo_init(&fifo_tx_open_random_delay_data);
	k_fifo_init(&fifo_tx_ack);

	for (size_t i = 0; i < 3; i++)
	{
		k_fifo_init(tx_open_fifos[i]);
	}

	return 0;
}

static int gpio_init(void)
{
	int err;

	if (!device_is_ready(gpio_dev)) {
		LOG_ERR("GPIO controller not ready");
		return -ENODEV;
	}
	err = gpio_pin_configure(gpio_dev, 12,
				 GPIO_INPUT | GPIO_PULL_UP | GPIO_ACTIVE_LOW);
	if (err) {
		LOG_ERR("GPIO_0 config error: %d", err);
		return err;
	}

	err = gpio_pin_configure(gpio_dev, 13,
				GPIO_OUTPUT | GPIO_ACTIVE_LOW);
	if (err) {
		LOG_ERR("GPIO_0 config error: %d", err);
		return err;
	}

	err = gpio_pin_configure(gpio_dev, 6,
				GPIO_OUTPUT | GPIO_ACTIVE_LOW);
	if (err) {
		LOG_ERR("GPIO_0 config error: %d", err);
		return err;
	}

	err = gpio_pin_configure(gpio_dev, 14,
				GPIO_OUTPUT | GPIO_ACTIVE_LOW);
	if (err) {
		LOG_ERR("GPIO_0 config error: %d", err);
		return err;
	}

	gpio_init_callback(&gpio_cb, gpio_cb_func, BIT(12));
	err = gpio_add_callback(gpio_dev, &gpio_cb);
	if (err) {
		LOG_ERR("GPIO_0 add callback error: %d", err);
		return err;
	}
	err = gpio_pin_interrupt_configure(gpio_dev, 12, GPIO_INT_EDGE_BOTH);
	if (err) {
		LOG_ERR("GPIO_0 enable callback error: %d", err);
		return err;
	}

	LOG_INF("GPIO init done");

	return 0;
}

static void esb_delayed_tx_thread_fn(void *p1, void *p2, void *p3)
{
	uint32_t delay;
	struct payload_data *data;
	uint8_t slice;

	k_sem_take(&init_sem, K_FOREVER);
	LOG_INF("esb_delayed_tx_thread_fn");

	while (1)
	{
		data = k_fifo_get(&fifo_tx_open_random_delay_data, K_FOREVER);
		if (data) 
		{
			
			slice = sys_rand32_get() % 3;
			delay = sys_rand32_get() % 2000;
			LOG_HEXDUMP_DBG(data->payload.data, data->payload.length, "Tx delay:");
			k_sleep(K_MSEC(delay));
			k_fifo_put(tx_open_fifos[slice], data);
		}
	}
}

static void esb_tx_thread_fn(void *p1, void *p2, void *p3)
{
	int err;

	k_sem_take(&init_sem, K_FOREVER);
	LOG_INF("esb_tx_thread_fn");

	while (1)
	{
		k_sem_take(&esb_tx_sem, K_FOREVER);
		switch (current_phase)
		{
		case 0:
			nrfx_timer_compare(&time_slice_timer, NRF_TIMER_CC_CHANNEL2, get_time(800), true);
			current_phase++;
			break;
		case 1:
			tx_phase_timer(1210, 575, 2, 4);
			break;
		case 2:
			tx_phase_timer(550, 80, 5, 4);
			esb_tx_ack(current_slice);
			break;
		case 3:
			tx_phase_timer(1050, 320, 3, 4);
			esb_tx_open(current_slice);
			break;
		
		default:
			break;
		}	
	}
}

static void spi_thread_fn(void *p1, void *p2, void *p3)
{
	uint8_t len;
	
	struct spi_config config = (struct spi_config) {
		.frequency = 125000,
		.operation = SPI_OP_MODE_SLAVE | SPI_WORD_SET(8),
		.slave = 0,
		.cs = NULL,
	};

	k_sem_take(&init_sem, K_FOREVER);

	LOG_INF("spi_thread_fn");

	while (1)
	{
		struct payload_data *data;

		data = k_malloc(sizeof(struct payload_data));
		data->payload.pipe = 0;
		data->payload.noack = true;

		struct spi_buf rx_buf = {
			.buf = data->payload.data,
			.len = 64,
		};
		
		struct spi_buf_set rx_bufs = {
			.buffers = &rx_buf,
			.count = 1,
		};
		
		len = spi_read(spi_dev, &config, &rx_bufs);
		LOG_HEXDUMP_DBG(rx_bufs.buffers->buf, len, "parsed:");
		
		data->payload.length = len;

		switch (data->payload.data[0])
		{
		case 0xF0:
			random_delay_esb_send(data);
			break;
		case 0xFF:
			ack_esb_send(data);
		default:
			break;
		}
	}	
}

void main(void)
{
	int blink_status = 0;
	int err;
	
	LOG_INF("midi ack tx sample");

	err = clocks_init();
	if (err) {
		return;
	}

	buffer_init();

	spi_dev = DEVICE_DT_GET(SPI_NODE);
	if (!device_is_ready(spi_dev)) {
		LOG_INF("device not ready.");
	}

	err = midi_esb_init();
	if (err) {
		LOG_ERR("Cannot init MIDI ACK (err: %d)", err);
	}

	err = dk_leds_init();
	if (err) {
		LOG_ERR("Cannot init LEDs (err: %d)", err);
	}

	err = sys_timer_init();
	if (err) {
		LOG_ERR("Cannot init timer (err: %d)", err);
	}

	err = gpio_init();
	if (err) {
		LOG_ERR("GPIO init failed");
	}

	k_sem_give(&init_sem);
	k_sem_give(&init_sem);
	k_sem_give(&init_sem);

	nrfx_timer_enable(&time_slice_timer);
	for (;;) {
		dk_set_led(RUN_STATUS_LED, (++blink_status) % 2);
		k_sleep(K_MSEC(RUN_LED_BLINK_INTERVAL));
		LOG_INF("running");
	}
}