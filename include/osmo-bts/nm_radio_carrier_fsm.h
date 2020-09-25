/* NM Radio Carrier FSM. Following 3GPP TS 12.21 Figure 2/GSM 12.21:
  GSM 12.21 Objects' Operational state and availability status behaviour during initialization */

/* (C) 2020 by sysmocom - s.m.f.c. GmbH <info@sysmocom.de>
 * Author: Pau Espin Pedrol <pespin@sysmocom.de>
 *
 * All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#pragma once

#include <osmocom/core/fsm.h>

enum nm_rcarrier_op_fsm_states {
	NM_RCARRIER_ST_OP_DISABLED_NOTINSTALLED,
	NM_RCARRIER_ST_OP_DISABLED_OFFLINE,
	NM_RCARRIER_ST_OP_ENABLED,
};

enum nm_rcarrier_op_fsm_events {
	NM_RCARRIER_EV_SW_ACT,
	NM_RCARRIER_EV_OPSTART_ACK,
	NM_RCARRIER_EV_OPSTART_NACK,
	NM_RCARRIER_EV_RSL_UP,
	NM_RCARRIER_EV_RSL_DOWN,
	NM_RCARRIER_EV_PHYLINK_UP,
	NM_RCARRIER_EV_PHYLINK_DOWN,
	NM_RCARRIER_EV_DISABLE,
};

extern struct osmo_fsm nm_rcarrier_fsm;
