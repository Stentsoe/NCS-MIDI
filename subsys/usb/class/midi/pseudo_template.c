struct usb_midi_emb_jack {
	uint8_t id;

	void (*received_cb)(const struct device *dev,
					       struct net_buf *buffer,
					       size_t size);
};

struct midi_ep_data {
	uint8_t num_assoc;
	union {
		struct usb_midi_emb_jack **in_jacks;
		struct usb_midi_emb_jack *out_jacks;
		struct usb_midi_group_terminal_block *gt_blocks;
	} assoc;
};



struct usb_midi_iface_data {
	bool enabled;
	uint8_t index;
	// uint8_t num_ext_jacks;
};

/* Device data structure */
struct usb_midi_dev_data {
	const struct usb_midi_ops *ops;

	const struct cs_ac_if_descriptor *desc_hdr;

	struct usb_dev_data common;

	struct net_buf_pool *pool;

	uint8_t num_ifaces;

	struct usb_midi_iface_data *iface_list;

};

struct midi_ep_data *midi_internal_ep_data[16];

static sys_slist_t usb_midi_data_devlist;


NET_BUF_POOL_FIXED_DEFINE(buf_pool_0, 5, 8, net_buf_destroy);


struct usb_midi_emb_jack *emb_in_jacks[2];
struct usb_midi_emb_jack *emb_out_jacks[2];

struct usb_midi_emb_jack emb_in_jack_0000 = { 	
    .id = 1			
};

struct usb_midi_emb_jack emb_in_jack_0001 = { 	
    .id = 3			
};

struct usb_midi_emb_jack emb_out_jack_0000 = { 	
    .id = 2			
};

struct usb_midi_emb_jack emb_out_jack_0001 = { 	
    .id = 4			
};

struct usb_midi_iface_data iface_data_0[] = {		
	{								
		.enabled = false,			
		.index = 0, 
	},
};	
													
static struct usb_midi_dev_data midi_dev_data_0 = {	
    .iface_list = iface_data_0,						
    .num_ifaces = 1, 
    .pool = &buf_pool_0, \
};


/* DESCRIPTOR */
 
static struct usb_ep_cfg_data usb_midi_ep_data_0[] = {
    {			
		.ep_cb = midi_receive_cb,	
		.ep_addr = 0x01,
	},
    {			
		.ep_cb = usb_transfer_ep_callback,	
		.ep_addr = 0x81,
	},
};

struct usb_midi_emb_jack *out_ep_in_jacks_0000[1];

struct midi_ep_data midi_internal_ep_data_0000 = {
    .num_assoc = 1, 
    .assoc.in_jacks = out_ep_in_jacks_0000,
};

struct usb_midi_emb_jack *in_ep_out_jacks_0001[1];

struct midi_ep_data midi_internal_ep_data_0001 = {
    .num_assoc = 1, 
    .assoc.out_jacks = in_ep_out_jacks_0001,
};

/* USB_DEVICE */

struct usb_midi_emb_jack in_port_in_jacks_0001[1];
struct usb_midi_port_config in_port_cfg_data_0001 = {
		.type = EXT_JACK_OUT,														
		.entity_descriptor = &midi_device_0.out_jack_0001,	
		.num_assoc = 1,		
		.assoc.in_jacks = in_port_in_jacks_0001,													
	};	
static const struct midi_api usb_midi_out_jack_api_0001 = {	
    .midi_send = usb_midi_send, 												
    .midi_callback_set = usb_midi_port_callback_set, 							
};

static int usb_midi_cfg_init(const struct device *arg)
{
    in_port_in_jacks_0001[0] = &emb_in_jacks[0];
    emb_out_jacks[0] = &emb_in_jack_0001;
	return 0;
}


Lag autoindeksering
	