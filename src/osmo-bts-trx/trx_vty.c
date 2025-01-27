/* VTY interface for osmo-bts-trx */

/* (C) 2013 by Andreas Eversberg <jolly@eversberg.eu>
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
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <ctype.h>
#include <inttypes.h>

#include <arpa/inet.h>

#include <osmocom/core/msgb.h>
#include <osmocom/core/talloc.h>
#include <osmocom/core/select.h>
#include <osmocom/core/bits.h>
#include <osmocom/core/socket.h>

#include <osmocom/vty/vty.h>
#include <osmocom/vty/command.h>
#include <osmocom/vty/misc.h>

#include <osmo-bts/gsm_data.h>
#include <osmo-bts/logging.h>
#include <osmo-bts/vty.h>
#include <osmo-bts/scheduler.h>
#include <osmo-bts/bts.h>

#include "l1_if.h"
#include "trx_if.h"
#include "loops.h"

#define X(x) (1 << x)

#define OSMOTRX_STR	"OsmoTRX Transceiver configuration\n"

DEFUN(show_transceiver, show_transceiver_cmd, "show transceiver",
	SHOW_STR "Display information about transceivers\n")
{
	struct gsm_bts_trx *trx;
	struct trx_l1h *l1h;
	unsigned int tn;

	llist_for_each_entry(trx, &g_bts->trx_list, list) {
		struct phy_instance *pinst = trx_phy_instance(trx);
		struct phy_link *plink = pinst->phy_link;
		char *sname = osmo_sock_get_name(NULL, plink->u.osmotrx.trx_ofd_clk.fd);
		l1h = pinst->u.osmotrx.hdl;
		vty_out(vty, "TRX %d %s%s", trx->nr, sname, VTY_NEWLINE);
		talloc_free(sname);
		vty_out(vty, " %s%s",
			trx_if_powered(l1h) ? "poweron":"poweroff",
			VTY_NEWLINE);
		vty_out(vty, "phy link state: %s%s",
			phy_link_state_name(phy_link_state_get(plink)), VTY_NEWLINE);
		if (l1h->config.arfcn_valid)
			vty_out(vty, " arfcn  : %d%s%s",
				(l1h->config.arfcn & ~ARFCN_PCS),
				(l1h->config.arfcn & ARFCN_PCS) ? " (PCS)" : "",
				VTY_NEWLINE);
		else
			vty_out(vty, " arfcn  : undefined%s", VTY_NEWLINE);
		if (l1h->config.tsc_valid)
			vty_out(vty, " tsc    : %d%s", l1h->config.tsc,
				VTY_NEWLINE);
		else
			vty_out(vty, " tsc    : undefined%s", VTY_NEWLINE);
		if (l1h->config.bsic_valid)
			vty_out(vty, " bsic   : %d%s", l1h->config.bsic,
				VTY_NEWLINE);
		else
			vty_out(vty, " bsic   : undefined%s", VTY_NEWLINE);

		for (tn = 0; tn < ARRAY_SIZE(trx->ts); tn++) {
			const struct gsm_bts_trx_ts *ts = &trx->ts[tn];
			const struct l1sched_ts *l1ts = ts->priv;
			const struct trx_sched_multiframe *mf;

			mf = &trx_sched_multiframes[l1ts->mf_index];

			vty_out(vty, "  timeslot #%u (%s)%s",
				tn, mf->name, VTY_NEWLINE);
			vty_out(vty, "    pending DL prims    : %u%s",
				llist_count(&l1ts->dl_prims), VTY_NEWLINE);
			vty_out(vty, "    interference        : %ddBm%s",
				l1ts->chan_state[TRXC_IDLE].meas.interf_avg,
				VTY_NEWLINE);
		}
	}

	return CMD_SUCCESS;
}


static void show_phy_inst_single(struct vty *vty, struct phy_instance *pinst)
{
	uint8_t tn;
	struct trx_l1h *l1h = pinst->u.osmotrx.hdl;
	struct gsm_bts_trx *trx = pinst->trx;

	vty_out(vty, "PHY Instance '%s': bound to %s%s",
		phy_instance_name(pinst),
		gsm_trx_name(trx),
		VTY_NEWLINE);

	if (trx != NULL) {
		const int actual = get_p_actual_mdBm(trx, trx->power_params.p_total_tgt_mdBm);
		const int max = get_p_max_out_mdBm(trx);
		vty_out(vty, " tx-attenuation : %d dB%s",
			(max - actual) / 1000, VTY_NEWLINE);
	}

	if (l1h->config.rxgain_valid)
		vty_out(vty, " rx-gain        : %d dB%s",
			l1h->config.rxgain, VTY_NEWLINE);
	else
		vty_out(vty, " rx-gain        : undefined%s", VTY_NEWLINE);
	if (l1h->config.maxdly_valid)
		vty_out(vty, " maxdly : %d%s", l1h->config.maxdly,
			VTY_NEWLINE);
	else
		vty_out(vty, " maxdly : undefined%s", VTY_NEWLINE);
	if (l1h->config.maxdlynb_valid)
		vty_out(vty, " maxdlynb : %d%s", l1h->config.maxdlynb,
			VTY_NEWLINE);
	else
		vty_out(vty, " maxdlynb : undefined%s", VTY_NEWLINE);
	for (tn = 0; tn < TRX_NR_TS; tn++) {
		if (!((1 << tn) & l1h->config.slotmask)) {
			vty_out(vty, " slot #%d: unsupported%s", tn,
				VTY_NEWLINE);
			continue;
		} else if (!l1h->config.setslot_valid[tn]) {
			vty_out(vty, " slot #%d: undefined%s", tn,
				VTY_NEWLINE);
			continue;
		}

		vty_out(vty, " slot #%d: type %d", tn,
			l1h->config.setslot[tn].slottype);
		if (l1h->config.setslot[tn].tsc_valid)
			vty_out(vty, " TSC-s%dc%d",
				l1h->config.setslot[tn].tsc_set,
				l1h->config.setslot[tn].tsc_val);
		vty_out(vty, "%s", VTY_NEWLINE);
	}
}

static void show_phy_single(struct vty *vty, struct phy_link *plink)
{
	struct phy_instance *pinst;

	vty_out(vty, "PHY %u%s", plink->num, VTY_NEWLINE);

	llist_for_each_entry(pinst, &plink->instances, list)
		show_phy_inst_single(vty, pinst);
}

DEFUN(show_phy, show_phy_cmd, "show phy",
	SHOW_STR  "Display information about the available PHYs")
{
	int i;

	for (i = 0; i < 255; i++) {
		struct phy_link *plink = phy_link_by_num(i);
		if (!plink)
			break;
		show_phy_single(vty, plink);
	}

	return CMD_SUCCESS;
}

DEFUN_USRATTR(cfg_trx_nominal_power, cfg_trx_nominal_power_cmd,
	      X(BTS_VTY_TRX_POWERCYCLE),
	      "nominal-tx-power <-10-100>",
	      "Manually set (force) the nominal transmit output power in dBm\n"
	      "Nominal transmit output power level in dBm\n")
{
	struct gsm_bts_trx *trx = vty->index;
	struct phy_instance *pinst = trx_phy_instance(trx);
	struct trx_l1h *l1h = pinst->u.osmotrx.hdl;
	int val = atoi(argv[0]);

	l1if_trx_set_nominal_power(trx, val);
	l1h->config.nominal_power_set_by_vty = true;

	return CMD_SUCCESS;
}

DEFUN_USRATTR(cfg_trx_no_nominal_power, cfg_trx_no_nominal_power_cmd,
	      X(BTS_VTY_TRX_POWERCYCLE),
	      "no nominal-tx-power",
	      NO_STR
	      "Manually set (force) the nominal transmit output power; ask the TRX instead (default)\n")
{
	struct gsm_bts_trx *trx = vty->index;
	struct phy_instance *pinst = trx_phy_instance(trx);
	struct trx_l1h *l1h = pinst->u.osmotrx.hdl;

	l1h->config.nominal_power_set_by_vty = false;

	return CMD_SUCCESS;
}

DEFUN_DEPRECATED(cfg_phy_ms_power_loop, cfg_phy_ms_power_loop_cmd,
	"osmotrx ms-power-loop <-127-127>", OSMOTRX_STR
	"Enable MS power control loop\nTarget RSSI value (transceiver specific, "
	"should be 6dB or more above noise floor)\n")
{
	vty_out(vty, "'%s' is deprecated, MS Power Control is now managed by BSC%s",
		self->string, VTY_NEWLINE);

	uint8_t rxlev = dbm2rxlev(atoi(argv[0]));
	g_bts->ms_dpc_params.rxlev_meas.lower_thresh = rxlev;
	g_bts->ms_dpc_params.rxlev_meas.upper_thresh = rxlev;

	return CMD_SUCCESS;
}

DEFUN_DEPRECATED(cfg_phy_no_ms_power_loop, cfg_phy_no_ms_power_loop_cmd,
	"no osmotrx ms-power-loop",
	NO_STR OSMOTRX_STR "Disable MS power control loop\n")
{
	vty_out(vty, "'%s' is deprecated, MS Power Control is now managed by BSC%s",
		self->string, VTY_NEWLINE);

	return CMD_SUCCESS;
}

DEFUN_DEPRECATED(cfg_phy_timing_advance_loop, cfg_phy_timing_advance_loop_cmd,
	"osmotrx timing-advance-loop", OSMOTRX_STR
	"Enable timing advance control loop\n")
{
	vty_out(vty, "'%s' is deprecated, Timing Advance loop is now active by default%s",
		self->string, VTY_NEWLINE);

	return CMD_SUCCESS;
}
DEFUN_DEPRECATED(cfg_phy_no_timing_advance_loop, cfg_phy_no_timing_advance_loop_cmd,
	"no osmotrx timing-advance-loop",
	NO_STR OSMOTRX_STR "Disable timing advance control loop\n")
{
	vty_out(vty, "'%s' is deprecated, Timing Advance loop is now active by default%s",
		self->string, VTY_NEWLINE);

	return CMD_SUCCESS;
}

DEFUN_ATTR(cfg_phyinst_maxdly, cfg_phyinst_maxdly_cmd,
	   "osmotrx maxdly <0-31>",
	   OSMOTRX_STR
	   "Set the maximum acceptable delay of an Access Burst (in GSM symbols)."
	   " Access Burst is the first burst a mobile transmits in order to establish"
	   " a connection and it is used to estimate Timing Advance (TA) which is"
	   " then applied to Normal Bursts to compensate for signal delay due to"
	   " distance. So changing this setting effectively changes maximum range of"
	   " the cell, because if we receive an Access Burst with a delay higher than"
	   " this value, it will be ignored and connection is dropped.\n"
	   "GSM symbols (approx. 1.1km per symbol)\n",
	   CMD_ATTR_IMMEDIATE)
{
	struct phy_instance *pinst = vty->index;
	struct trx_l1h *l1h = pinst->u.osmotrx.hdl;

	l1h->config.maxdly = atoi(argv[0]);
	l1h->config.maxdly_valid = 1;
	l1h->config.maxdly_sent = false;
	l1if_provision_transceiver_trx(l1h);

	return CMD_SUCCESS;
}

DEFUN_ATTR(cfg_phyinst_maxdlynb, cfg_phyinst_maxdlynb_cmd,
	   "osmotrx maxdlynb <0-31>",
	   OSMOTRX_STR
	   "Set the maximum acceptable delay of a Normal Burst (in GSM symbols)."
	   " USE FOR TESTING ONLY, DON'T CHANGE IN PRODUCTION USE!"
	   " During normal operation, Normal Bursts delay are controlled by a Timing"
	   " Advance control loop and thus Normal Bursts arrive to a BTS with no more"
	   " than a couple GSM symbols, which is already taken into account in osmo-trx."
	   " So changing this setting will have no effect in production installations"
	   " except increasing osmo-trx CPU load. This setting is only useful when"
	   " testing with a transmitter which can't precisely synchronize to the BTS"
	   " downlink signal, like e.g. R&S CMD57.\n"
	   "GSM symbols (approx. 1.1km per symbol)\n",
	   CMD_ATTR_IMMEDIATE)
{
	struct phy_instance *pinst = vty->index;
	struct trx_l1h *l1h = pinst->u.osmotrx.hdl;

	l1h->config.maxdlynb = atoi(argv[0]);
	l1h->config.maxdlynb_valid = 1;
	l1h->config.maxdlynb_sent = false;
	l1if_provision_transceiver_trx(l1h);

	return CMD_SUCCESS;
}

DEFUN(cfg_phyinst_slotmask, cfg_phyinst_slotmask_cmd,
	"slotmask (1|0) (1|0) (1|0) (1|0) (1|0) (1|0) (1|0) (1|0)",
	"Set the supported slots\n"
	"TS0 supported\nTS0 unsupported\nTS1 supported\nTS1 unsupported\n"
	"TS2 supported\nTS2 unsupported\nTS3 supported\nTS3 unsupported\n"
	"TS4 supported\nTS4 unsupported\nTS5 supported\nTS5 unsupported\n"
	"TS6 supported\nTS6 unsupported\nTS7 supported\nTS7 unsupported\n")
{
	struct phy_instance *pinst = vty->index;
	struct trx_l1h *l1h = pinst->u.osmotrx.hdl;
	uint8_t tn;

	l1h->config.slotmask = 0;
	for (tn = 0; tn < TRX_NR_TS; tn++)
		if (argv[tn][0] == '1')
			l1h->config.slotmask |= (1 << tn);

	return CMD_SUCCESS;
}

DEFUN_DEPRECATED(cfg_phyinst_power_on, cfg_phyinst_power_on_cmd,
	"osmotrx power (on|off)",
	OSMOTRX_STR
	"Change TRX state\n"
	"Turn it ON or OFF\n")
{
	struct phy_instance *pinst = vty->index;
	struct trx_l1h *l1h = pinst->u.osmotrx.hdl;

	vty_out (vty, "'osmotrx power' is deprecated, use OML's standard "
		      "Administrative State instead to control each TRX "
		      "('rf_locked' VTY cmd in osmo-bsc)%s", VTY_NEWLINE);

	if (strcmp(argv[0], "on"))
		vty_out(vty, "OFF: %d%s", trx_if_cmd_poweroff(l1h, NULL), VTY_NEWLINE);
	else {
		vty_out(vty, "ON: %d%s", trx_if_cmd_poweron(l1h, NULL), VTY_NEWLINE);
	}

	return CMD_SUCCESS;
}

DEFUN_ATTR(cfg_phy_fn_advance, cfg_phy_fn_advance_cmd,
	   "osmotrx fn-advance <0-30>",
	   OSMOTRX_STR
	   "Set the number of frames to be transmitted to transceiver in advance "
	   "of current FN\n"
	   "Advance in frames\n",
	   CMD_ATTR_IMMEDIATE)
{
	struct phy_link *plink = vty->index;

	plink->u.osmotrx.clock_advance = atoi(argv[0]);

	return CMD_SUCCESS;
}

DEFUN_ATTR(cfg_phy_rts_advance, cfg_phy_rts_advance_cmd,
	   "osmotrx rts-advance <0-30>",
	   OSMOTRX_STR
	   "Set the number of frames to be requested (PCU) in advance of current "
	   "FN. Do not change this, unless you have a good reason!\n"
	   "Advance in frames\n",
	   CMD_ATTR_IMMEDIATE)
{
	struct phy_link *plink = vty->index;

	plink->u.osmotrx.rts_advance = atoi(argv[0]);

	return CMD_SUCCESS;
}

DEFUN_USRATTR(cfg_phyinst_rxgain, cfg_phyinst_rxgain_cmd,
	      X(BTS_VTY_TRX_POWERCYCLE),
	      "osmotrx rx-gain <0-50>",
	      OSMOTRX_STR
	      "Set the receiver gain in dB\n"
	      "Gain in dB\n")
{
	struct phy_instance *pinst = vty->index;
	struct trx_l1h *l1h = pinst->u.osmotrx.hdl;

	l1h->config.rxgain = atoi(argv[0]);
	l1h->config.rxgain_valid = 1;
	l1h->config.rxgain_sent = false;

	return CMD_SUCCESS;
}

DEFUN(cfg_phyinst_tx_atten, cfg_phyinst_tx_atten_cmd,
	"osmotrx tx-attenuation (oml|<0-50>)",
	OSMOTRX_STR
	"Set the transmitter attenuation\n"
	"Use NM_ATT_RF_MAXPOWR_R (max power reduction) from BSC via OML (default)\n"
	"Fixed attenuation in dB, overriding OML (default)\n")
{
	struct phy_instance *pinst = vty->index;
	struct trx_l1h *l1h = pinst->u.osmotrx.hdl;

	if (strcmp(argv[0], "oml") == 0)
		l1h->config.forced_max_power_red = -1;
	else
		l1h->config.forced_max_power_red = atoi(argv[0]);

	return CMD_SUCCESS;
}

DEFUN_USRATTR(cfg_phyinst_no_rxgain, cfg_phyinst_no_rxgain_cmd,
	      X(BTS_VTY_TRX_POWERCYCLE),
	      "no osmotrx rx-gain",
	      NO_STR OSMOTRX_STR "Unset the receiver gain in dB\n")
{
	struct phy_instance *pinst = vty->index;
	struct trx_l1h *l1h = pinst->u.osmotrx.hdl;

	l1h->config.rxgain_valid = 0;

	return CMD_SUCCESS;
}

DEFUN_USRATTR(cfg_phyinst_no_maxdly, cfg_phyinst_no_maxdly_cmd,
	      X(BTS_VTY_TRX_POWERCYCLE),
	      "no osmotrx maxdly",
	      NO_STR OSMOTRX_STR
	      "Unset the maximum delay of GSM symbols\n")
{
	struct phy_instance *pinst = vty->index;
	struct trx_l1h *l1h = pinst->u.osmotrx.hdl;

	l1h->config.maxdly_valid = 0;

	return CMD_SUCCESS;
}

DEFUN_USRATTR(cfg_phyinst_no_maxdlynb, cfg_phyinst_no_maxdlynb_cmd,
	      X(BTS_VTY_TRX_POWERCYCLE),
	      "no osmotrx maxdlynb",
	      NO_STR OSMOTRX_STR
	      "Unset the maximum delay of GSM symbols\n")
{
	struct phy_instance *pinst = vty->index;
	struct trx_l1h *l1h = pinst->u.osmotrx.hdl;

	l1h->config.maxdlynb_valid = 0;

	return CMD_SUCCESS;
}

DEFUN(cfg_phy_transc_ip, cfg_phy_transc_ip_cmd,
	"osmotrx ip HOST",
	OSMOTRX_STR
	"Set local and remote IP address\n"
	"IP address (for both OsmoBtsTrx and OsmoTRX)\n")
{
	struct phy_link *plink = vty->index;

	osmo_talloc_replace_string(plink, &plink->u.osmotrx.local_ip, argv[0]);
	osmo_talloc_replace_string(plink, &plink->u.osmotrx.remote_ip, argv[0]);

	return CMD_SUCCESS;
}

DEFUN(cfg_phy_osmotrx_ip, cfg_phy_osmotrx_ip_cmd,
	"osmotrx ip (local|remote) A.B.C.D",
	OSMOTRX_STR
	"Set IP address\n" "Local IP address (BTS)\n"
	"Remote IP address (OsmoTRX)\n" "IP address\n")
{
	struct phy_link *plink = vty->index;

	if (!strcmp(argv[0], "local"))
		osmo_talloc_replace_string(plink, &plink->u.osmotrx.local_ip, argv[1]);
	else if (!strcmp(argv[0], "remote"))
		osmo_talloc_replace_string(plink, &plink->u.osmotrx.remote_ip, argv[1]);
	else
		return CMD_WARNING;

	return CMD_SUCCESS;
}

DEFUN(cfg_phy_base_port, cfg_phy_base_port_cmd,
	"osmotrx base-port (local|remote) <0-65535>",
	OSMOTRX_STR "Set base UDP port number\n" "Local UDP port\n"
	"Remote UDP port\n" "UDP base port number\n")
{
	struct phy_link *plink = vty->index;

	if (!strcmp(argv[0], "local"))
		plink->u.osmotrx.base_port_local = atoi(argv[1]);
	else
		plink->u.osmotrx.base_port_remote = atoi(argv[1]);

	return CMD_SUCCESS;
}

DEFUN_USRATTR(cfg_phy_setbsic, cfg_phy_setbsic_cmd,
	      X(BTS_VTY_TRX_POWERCYCLE),
	      "osmotrx legacy-setbsic", OSMOTRX_STR
	      "Use SETBSIC to configure transceiver (use ONLY with OpenBTS Transceiver!)\n")
{
	struct phy_link *plink = vty->index;
	plink->u.osmotrx.use_legacy_setbsic = true;

	vty_out(vty, "%% You have enabled SETBSIC, which is not supported by OsmoTRX "
		"but only useful if you want to interface with legacy OpenBTS Transceivers%s",
		VTY_NEWLINE);

	return CMD_SUCCESS;
}

DEFUN_USRATTR(cfg_phy_no_setbsic, cfg_phy_no_setbsic_cmd,
	      X(BTS_VTY_TRX_POWERCYCLE),
	      "no osmotrx legacy-setbsic",
	      NO_STR OSMOTRX_STR "Disable Legacy SETBSIC to configure transceiver\n")
{
	struct phy_link *plink = vty->index;
	plink->u.osmotrx.use_legacy_setbsic = false;

	return CMD_SUCCESS;
}

DEFUN_USRATTR(cfg_phy_trxd_max_version, cfg_phy_trxd_max_version_cmd,
	      X(BTS_VTY_TRX_POWERCYCLE),
	      "osmotrx trxd-max-version (latest|<0-15>)", OSMOTRX_STR
	      "Set maximum TRXD format version to negotiate with TRX\n"
	      "Use latest supported TRXD format version (default)\n"
	      "Maximum TRXD format version number\n")
{
	struct phy_link *plink = vty->index;

	int max_ver;
	if (strcmp(argv[0], "latest") == 0)
		max_ver = TRX_DATA_PDU_VER;
	else
		max_ver = atoi(argv[0]);
	if (max_ver > TRX_DATA_PDU_VER) {
		vty_out(vty, "%% Format version %d is not supported, maximum supported is %d%s",
			max_ver, TRX_DATA_PDU_VER, VTY_NEWLINE);
		return CMD_WARNING;
	}
	plink->u.osmotrx.trxd_pdu_ver_max = max_ver;

	return CMD_SUCCESS;
}

void bts_model_config_write_phy(struct vty *vty, const struct phy_link *plink)
{
	if (plink->u.osmotrx.local_ip)
		vty_out(vty, " osmotrx ip local %s%s",
			plink->u.osmotrx.local_ip, VTY_NEWLINE);
	if (plink->u.osmotrx.remote_ip)
		vty_out(vty, " osmotrx ip remote %s%s",
			plink->u.osmotrx.remote_ip, VTY_NEWLINE);

	if (plink->u.osmotrx.base_port_local)
		vty_out(vty, " osmotrx base-port local %"PRIu16"%s",
			plink->u.osmotrx.base_port_local, VTY_NEWLINE);
	if (plink->u.osmotrx.base_port_remote)
		vty_out(vty, " osmotrx base-port remote %"PRIu16"%s",
			plink->u.osmotrx.base_port_remote, VTY_NEWLINE);

	vty_out(vty, " osmotrx fn-advance %d%s",
		plink->u.osmotrx.clock_advance, VTY_NEWLINE);
	vty_out(vty, " osmotrx rts-advance %d%s",
		plink->u.osmotrx.rts_advance, VTY_NEWLINE);

	if (plink->u.osmotrx.use_legacy_setbsic)
		vty_out(vty, " osmotrx legacy-setbsic%s", VTY_NEWLINE);

	if (plink->u.osmotrx.trxd_pdu_ver_max != TRX_DATA_PDU_VER)
		vty_out(vty, " osmotrx trxd-max-version %d%s", plink->u.osmotrx.trxd_pdu_ver_max, VTY_NEWLINE);
}

void bts_model_config_write_phy_inst(struct vty *vty, const struct phy_instance *pinst)
{
	struct trx_l1h *l1h = pinst->u.osmotrx.hdl;

	if (l1h->config.rxgain_valid)
		vty_out(vty, "  osmotrx rx-gain %d%s",
			l1h->config.rxgain, VTY_NEWLINE);
	if (l1h->config.forced_max_power_red == -1)
		vty_out(vty, "  osmotrx tx-attenuation oml%s", VTY_NEWLINE);
	else
		vty_out(vty, "  osmotrx tx-attenuation %d%s",
			l1h->config.forced_max_power_red, VTY_NEWLINE);
	if (l1h->config.maxdly_valid)
		vty_out(vty, "  osmotrx maxdly %d%s", l1h->config.maxdly, VTY_NEWLINE);
	if (l1h->config.maxdlynb_valid)
		vty_out(vty, "  osmotrx maxdlynb %d%s", l1h->config.maxdlynb, VTY_NEWLINE);
	if (l1h->config.slotmask != 0xff)
		vty_out(vty, "  slotmask %d %d %d %d %d %d %d %d%s",
			l1h->config.slotmask & 1,
			(l1h->config.slotmask >> 1) & 1,
			(l1h->config.slotmask >> 2) & 1,
			(l1h->config.slotmask >> 3) & 1,
			(l1h->config.slotmask >> 4) & 1,
			(l1h->config.slotmask >> 5) & 1,
			(l1h->config.slotmask >> 6) & 1,
			l1h->config.slotmask >> 7,
			VTY_NEWLINE);
}

void bts_model_config_write_bts(struct vty *vty, const struct gsm_bts *bts)
{
}

void bts_model_config_write_trx(struct vty *vty, const struct gsm_bts_trx *trx)
{
	struct phy_instance *pinst = trx_phy_instance(trx);
	struct trx_l1h *l1h = pinst->u.osmotrx.hdl;

	if (l1h->config.nominal_power_set_by_vty)
		vty_out(vty, "  nominal-tx-power %d%s", trx->nominal_power,
			VTY_NEWLINE);
}

int bts_model_vty_init(void *ctx)
{
	install_element_ve(&show_transceiver_cmd);
	install_element_ve(&show_phy_cmd);

	install_element(TRX_NODE, &cfg_trx_nominal_power_cmd);
	install_element(TRX_NODE, &cfg_trx_no_nominal_power_cmd);

	install_element(PHY_NODE, &cfg_phy_ms_power_loop_cmd);
	install_element(PHY_NODE, &cfg_phy_no_ms_power_loop_cmd);
	install_element(PHY_NODE, &cfg_phy_timing_advance_loop_cmd);
	install_element(PHY_NODE, &cfg_phy_no_timing_advance_loop_cmd);
	install_element(PHY_NODE, &cfg_phy_base_port_cmd);
	install_element(PHY_NODE, &cfg_phy_fn_advance_cmd);
	install_element(PHY_NODE, &cfg_phy_rts_advance_cmd);
	install_element(PHY_NODE, &cfg_phy_transc_ip_cmd);
	install_element(PHY_NODE, &cfg_phy_osmotrx_ip_cmd);
	install_element(PHY_NODE, &cfg_phy_setbsic_cmd);
	install_element(PHY_NODE, &cfg_phy_no_setbsic_cmd);
	install_element(PHY_NODE, &cfg_phy_trxd_max_version_cmd);

	install_element(PHY_INST_NODE, &cfg_phyinst_rxgain_cmd);
	install_element(PHY_INST_NODE, &cfg_phyinst_tx_atten_cmd);
	install_element(PHY_INST_NODE, &cfg_phyinst_no_rxgain_cmd);
	install_element(PHY_INST_NODE, &cfg_phyinst_slotmask_cmd);
	install_element(PHY_INST_NODE, &cfg_phyinst_power_on_cmd);
	install_element(PHY_INST_NODE, &cfg_phyinst_maxdly_cmd);
	install_element(PHY_INST_NODE, &cfg_phyinst_no_maxdly_cmd);
	install_element(PHY_INST_NODE, &cfg_phyinst_maxdlynb_cmd);
	install_element(PHY_INST_NODE, &cfg_phyinst_no_maxdlynb_cmd);

	return 0;
}

int bts_model_ctrl_cmds_install(struct gsm_bts *bts)
{
	return 0;
}
