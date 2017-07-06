// from modwlan

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "py/mpconfig.h"
#include "py/obj.h"
#include "py/objstr.h"
#include "py/runtime.h"
#include "py/stream.h"
#include "py/mphal.h"
#include "py/mperrno.h"

#include "esp_heap_alloc_caps.h"
#include "sdkconfig.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "esp_event_loop.h"

//#include "timeutils.h"
#include "netutils.h"
#include "modnetwork.h"
#include "modusocket.h"
#include "modwlan.h"
#include "pybioctl.h"
//#include "pybrtc.h"
#include "serverstask.h"
#include "mpexception.h"
#include "antenna.h"
#include "modussl.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"



// from raw_esp

#include "user_interface.h"

#include "ets_sys.h"
#include "osapi.h"
#include "driver/uart.h"
#include "mem.h"
#include "missing_declarations.h"



#include "lwip/ip_addr.h"
#include "lwip/raw.h"
#include "lwip/udp.h"
#include "netif/wlan_lwip_if.h"

#include "comm.h"
#include "misc.h"



static struct raw_pcb *raw_pcb_tcp = NULL;
static struct raw_pcb *raw_pcb_udp = NULL;

static volatile netif_input_fn netif_input_orig = NULL;
static volatile netif_output_fn netif_output_orig = NULL;
static volatile netif_linkoutput_fn netif_linkoutput_orig = NULL;



// needed for object initialization
//
STATIC mp_obj_t rawlan_init_helper() {

	struct station_config config;
	struct ip_info ip;

	wifi_set_phy_mode(PHY_MODE_11N);
	wifi_set_sleep_type(LIGHT_SLEEP_T);

	raw_pcb_tcp = raw_new(6);
	raw_pcb_udp = raw_new(17);
	if (!raw_pcb_tcp || !raw_pcb_udp) {
		COMM_DBG("Failed to init raw sockets");
	} else {
		// todo: check errors
		raw_bind(raw_pcb_tcp, IP_ADDR_ANY);
		raw_bind(raw_pcb_udp, IP_ADDR_ANY);
		raw_recv(raw_pcb_tcp, raw_receiver, NULL);
		raw_recv(raw_pcb_udp, raw_receiver, NULL);
	}
}




//static int wlan_gethostbyname(const char *name, mp_uint_t len, uint8_t *out_ip, mp_uint_t family);
//static int wlan_socket_socket(mod_network_socket_obj_t *s, int *_errno);


STATIC const mp_arg_t rawlan_init_args[] = {
    { MP_QSTR_id,                             MP_ARG_INT,  {.u_int = 0} },
    { MP_QSTR_mode,         MP_ARG_KW_ONLY  | MP_ARG_INT,  {.u_int = WIFI_MODE_STA} },
    { MP_QSTR_ssid,         MP_ARG_KW_ONLY  | MP_ARG_OBJ,  {.u_obj = MP_OBJ_NULL} },
    { MP_QSTR_auth,         MP_ARG_KW_ONLY  | MP_ARG_OBJ,  {.u_obj = mp_const_none} },
    { MP_QSTR_channel,      MP_ARG_KW_ONLY  | MP_ARG_INT,  {.u_int = 1} },
    { MP_QSTR_antenna,      MP_ARG_KW_ONLY  | MP_ARG_INT,  {.u_int = ANTENNA_TYPE_INTERNAL} },
    { MP_QSTR_power_save,   MP_ARG_KW_ONLY  | MP_ARG_BOOL, {.u_bool = false} },
};



/*
void ICACHE_FLASH_ATTR
user_init(void)
{
	uint32_t ps=999;
	os_delay_us(50*1000);   // delay 50ms before init uart

	uart_init(BIT_RATE_115200, BIT_RATE_115200);
	comm_init(packet_from_host);

	comm_send_begin(MSG_BOOT);
	comm_send_end();


	COMM_INFO("SDK version: %s", system_get_sdk_version());
	COMM_INFO("FW version: %s", FW_VERSION);
	COMM_INFO("Heap size: %d", system_get_free_heap_size());
	COMM_INFO("Alignment: %d", __BIGGEST_ALIGNMENT__);

	init_wlan();
}
*/

// Methods


STATIC mp_obj_t rawlan_make_new(const mp_obj_type_t *type, mp_uint_t n_args, mp_uint_t n_kw, const mp_obj_t *all_args) {
    // parse args
    mp_map_t kw_args;
    mp_map_init_fixed_table(&kw_args, n_kw, all_args + n_args);
    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(wlan_init_args)];
    mp_arg_parse_all(n_args, all_args, &kw_args, MP_ARRAY_SIZE(args), wlan_init_args, args);

    // setup the object
    wlan_obj_t *self = &wlan_obj;

    // give it to the sleep module
    //pyb_sleep_set_wlan_obj(self); // FIXME

    if (n_kw > 0) {
        // check the peripheral id
        if (args[0].u_int != 0) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_resource_not_avaliable));
        }
        // start the peripheral
        //rawlan_init_helper(self, &args[1]);
    }

    return (mp_obj_t)self;
}


static u8_t ICACHE_FLASH_ATTR
raw_receiver(void *arg, struct raw_pcb *pcb, struct pbuf *p0, ip_addr_t *addr)
{
	struct ip_hdr hdr;
	struct pbuf *p;

	COMM_DBG("WLan IP packet of size %d", p0->tot_len);
	if (global_forwarding_mode != FORWARDING_MODE_IP)
		return 0;

	if (pbuf_copy_partial(p0, &hdr, sizeof(hdr), 0) != sizeof(hdr)) {
		COMM_WARN("WLan packet of size %d has incomplete header",
			  p0->tot_len);
		return 0;
	}

	if (!forward_ip_broadcasts && (hdr.dest.addr == 0xffffffff)) {
		COMM_DBG("Passing broadcast to internal stack");
		return 0;
	}

	if ((hdr._proto == IP_PROTO_UDP)) {
		int offset = 4 * IPH_HL(&hdr);
		struct udp_hdr udp_h;
		if (!pbuf_copy_partial(p0, &udp_h, sizeof(udp_h), offset)) {
			COMM_WARN("Can't copy UDP header from WLan packet");
			return 0;
		}
		uint16_t src = ntohs(udp_h.src);
		uint16_t dst = ntohs(udp_h.dest);
		if ((src == 67) || (dst == 67) || (src == 68) || (dst == 68)) {
			COMM_DBG("Got DHCP packet, passing it to internal stack");
			return 0;
		}
	}

	comm_send_begin(MSG_IP_PACKET);
	for(p = p0; p; p = p->next) {
		comm_send_data(p->payload, p->len);
	}
	comm_send_end();

	pbuf_free(p0);
	return 1;
	/* return 0; // not processed */
}





STATIC mp_obj_t rawlan_init(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(rawlan_init_args) - 1];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(args), &rawlan_init_args[1], args);
    return rawlan_init_helper();
}


STATIC MP_DEFINE_CONST_FUN_OBJ_KW(rawlan_init_obj, 1, rawlan_init);
/*
mp_obj_t rawlan_deinit(mp_obj_t self_in) {

    if (servers_are_enabled()) {
       wlan_servers_stop();
    }

    if (wlan_obj.started) {
        esp_wifi_stop();
        wlan_obj.started = false;
    }
    return mp_const_none;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_1(rawlan_deinit_obj, rawlan_deinit);
*/

// Adding methods to locals_dict_table

STATIC const mp_map_elem_t wlan_locals_dict_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR_init),                (mp_obj_t)&rawlan_init_obj },
	    // class constants
    { MP_OBJ_NEW_QSTR(MP_QSTR_STA),                 MP_OBJ_NEW_SMALL_INT(WIFI_MODE_STA) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_AP),                  MP_OBJ_NEW_SMALL_INT(WIFI_MODE_AP) },
};

// Definition of Class

STATIC MP_DEFINE_CONST_DICT(rawlan_locals_dict, rawlan_locals_dict_table);

const mod_network_nic_type_t mod_network_nic_type_rawlan = {
    .base = {
        { &mp_type_type },
        .name = MP_QSTR_RAWLAN,
        .make_new = rawlan_make_new,
        .locals_dict = (mp_obj_t)&rawlan_locals_dict,
    },

    .n_gethostbyname = rawlan_gethostbyname,
    .n_socket = rawlan_socket_socket,
};


// Python Bindings

static int rawlan_gethostbyname(const char *name, mp_uint_t len, uint8_t *out_ip, mp_uint_t family) {
    uint32_t ip;
    struct hostent *h = gethostbyname(name);
    if (h == NULL) {
        // CPython: socket.herror
        return -errno;
    }
    ip = *(uint32_t*)*h->h_addr_list;
    out_ip[0] = ip;
    out_ip[1] = ip >> 8;
    out_ip[2] = ip >> 16;
    out_ip[3] = ip >> 24;
    return 0;
}


static int rawlan_socket_socket(mod_network_socket_obj_t *s, int *_errno) {
    int32_t sd = socket(s->sock_base.u_param.domain, s->sock_base.u_param.type, s->sock_base.u_param.proto);
    if (sd < 0) {
        *_errno = errno;
        return -1;
    }


