/*
 * par_low.c - low_level routines to access amiga parallel port
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

#include "par_low.h"
#include <avr/interrupt.h>
#include <util/delay_basic.h>

void par_low_init(void)
{
  // /STROBE (IN)
  PAR_STROBE_DDR &= ~PAR_STROBE_MASK;
  PAR_STROBE_PORT |= PAR_STROBE_MASK;
  
  // SELECT (IN)
  PAR_SELECT_DDR &= ~PAR_SELECT_MASK;
  PAR_SELECT_PORT |= PAR_SELECT_MASK;
  
  // BUSY (OUT) (0)
  PAR_BUSY_DDR |= PAR_BUSY_MASK;
  PAR_BUSY_PORT &= ~PAR_BUSY_MASK;
  
  // POUT (IN)
  PAR_POUT_DDR &= ~PAR_POUT_MASK;
  PAR_POUT_PORT |= PAR_POUT_MASK;
  
  // /ACK (OUT) (1)
  PAR_ACK_DDR |= PAR_ACK_MASK;
  PAR_ACK_PORT |= PAR_ACK_MASK;

  par_low_data_set_input();
}

// data bus

#ifdef HAVE_arduino
void par_low_data_set_output(void)
{
  PAR_DATA_LO_DDR |= PAR_DATA_LO_MASK;
  PAR_DATA_HI_DDR |= PAR_DATA_HI_MASK;
}

void par_low_data_set_input(void)
{
  PAR_DATA_LO_DDR &= ~PAR_DATA_LO_MASK;
  PAR_DATA_HI_DDR &= ~PAR_DATA_HI_MASK;
}
#else
#ifdef HAVE_avrnetio
void par_low_data_set_output(void)
{
  PAR_DATA_DDR = 0xff;
}

void par_low_data_set_input(void)
{
  PAR_DATA_DDR = 0;
}
#endif
#endif

void par_low_pulse_ack(u08 delay)
{
  par_low_set_ack_lo();
  _delay_loop_1(delay);
  par_low_set_ack_hi();
}
