/*
 * bridge.c: bridge packets from plip to eth and back
 *
 * Written by
 *  Christian Vogelgsang <chris@vogelgsang.org>
 *
 * This file is part of plipbox.
 * See README for copyright notice.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 *  02111-1307  USA.
 *
 */

#include "global.h"
   
#include "pkt_buf.h"
#include "pb_proto.h"
#include "uartutil.h"
#include "param.h"
#include "dump.h"
#include "timer.h"
#include "stats.h"
#include "util.h"
#include "bridge.h"
#include "main.h"
#include "cmd.h"
#include "pb_util.h"
#include "pio_util.h"
#include "pio.h"
#include "net/eth.h"
#include "net/net.h"

#define FLAG_ONLINE       1
#define FLAG_SEND_MAGIC   2

static u08 flags;
static u08 req_is_pending;

static void trigger_request(void)
{
  if(!req_is_pending) {
    req_is_pending = 1;
    pb_proto_request_recv();
    if(global_verbose) {
      uart_send_time_stamp_spc();
      uart_send_pstring(PSTR("REQ\r\n"));
    }
  } else {
    if(global_verbose) {
      uart_send_time_stamp_spc();
      uart_send_pstring(PSTR("req ign\r\n"));
    }
  }
}

// ----- magic packets -----

static void magic_online(const u08 *buf)
{
  uart_send_time_stamp_spc();
  uart_send_pstring(PSTR("[MAGIC] online\r\n"));
  flags |= FLAG_ONLINE;

  // validate mac address and if it does not match then reconfigure PIO
  const u08 *src_mac = eth_get_src_mac(buf);
  if(!net_compare_mac(param.mac_addr, src_mac)) {
    // update mac param and save
    net_copy_mac(src_mac, param.mac_addr);
    param_save();

    // re-configure PIO
    pio_exit();
    pio_init(param.mac_addr, PIO_INIT_BROAD_CAST);
  }
}

static void magic_offline(void)
{
  uart_send_time_stamp_spc();
  uart_send_pstring(PSTR("[MAGIC] offline\r\n"));
  flags &= ~FLAG_ONLINE;
}

static void magic_loopback(u16 size)
{
  flags |= FLAG_SEND_MAGIC;
  trigger_request();
}

static void request_magic(void)
{
  uart_send_time_stamp_spc();
  uart_send_pstring(PSTR("[MAGIC] request\r\n"));

  // build magic packet
  net_copy_bcast_mac(pkt_buf + ETH_OFF_TGT_MAC);
  net_copy_mac(param.mac_addr, pkt_buf + ETH_OFF_SRC_MAC);
  net_put_word(pkt_buf + ETH_OFF_TYPE, ETH_TYPE_MAGIC_ONLINE);

  // request receive
  flags |= FLAG_SEND_MAGIC;
  trigger_request();
}

// ----- packet callbacks -----

// the Amiga requests a new packet
static u08 fill_pkt(u08 *buf, u16 max_size, u16 *size)
{
  // need to send a magic?
  if((flags & FLAG_SEND_MAGIC) == FLAG_SEND_MAGIC) {
    flags &= ~FLAG_SEND_MAGIC;
    *size = ETH_HDR_SIZE;
  } else {
    // pending PIO packet?
    pio_util_recv_packet(size);
  }

  req_is_pending = 0;

  return PBPROTO_STATUS_OK;  
}

// handle incoming packet from Amiga
static u08 proc_pkt(const u08 *buf, u16 size)
{
  // get eth type
  u16 eth_type = eth_get_pkt_type(buf);
  switch(eth_type) {
    case ETH_TYPE_MAGIC_ONLINE:
      magic_online(buf);
      break;
    case ETH_TYPE_MAGIC_OFFLINE:
      magic_offline();
      break;
    case ETH_TYPE_MAGIC_LOOPBACK:
      magic_loopback(size);
      break;
    default:
      // send packet via pio
      pio_util_send_packet(size);
      // if a packet arrived and we are not online then request online state
      if((flags & FLAG_ONLINE)==0) {
        request_magic();
      }
      break;
  }
  return PBPROTO_STATUS_OK;
}

// ---------- loop ----------

u08 bridge_loop(void)
{
  u08 reset = 0;

  uart_send_time_stamp_spc();
  uart_send_pstring(PSTR("[BRIDGE] on\r\n"));

  pb_proto_init(fill_pkt, proc_pkt, pkt_buf, PKT_BUF_SIZE);
  pio_init(param.mac_addr, PIO_INIT_BROAD_CAST);
  stats_reset();

  // online flag
  flags = 0;
  req_is_pending = 0;

  while(run_mode == RUN_MODE_BRIDGE) {
    // handle commands
    reset = !cmd_worker();
    if(reset) {
      break;
    }

    // handle pbproto
    pb_util_handle();

    // incoming packet via PIO available?
    u08 n = pio_has_recv();
    if(n>0) {
      // if no request is pending then request it
      trigger_request();
     }
  }

  stats_dump_all();
  pio_exit();

  uart_send_time_stamp_spc();
  uart_send_pstring(PSTR("[BRIDGE] off\r\n"));

  return reset;
}
