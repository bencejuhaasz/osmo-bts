#ifndef _PCU_IF_H
#define _PCU_IF_H

#include <osmo-bts/pcuif_proto.h>

extern int pcu_direct;

#define PCUIF_HDR_SIZE ( sizeof(struct gsm_pcu_if) - sizeof(((struct gsm_pcu_if *)0)->u) )

int pcu_tx_info_ind(void);
int pcu_tx_si(const struct gsm_bts *bts, enum osmo_sysinfo_type si_type, bool enable);
int pcu_tx_app_info_req(struct gsm_bts *bts, uint8_t app_type, uint8_t len, const uint8_t *app_data);
int pcu_tx_rts_req(struct gsm_bts_trx_ts *ts, uint8_t is_ptcch, uint32_t fn,
	uint16_t arfcn, uint8_t block_nr);
int pcu_tx_data_ind(struct gsm_bts_trx_ts *ts, uint8_t is_ptcch, uint32_t fn,
	uint16_t arfcn, uint8_t block_nr, uint8_t *data, uint8_t len,
		    int8_t rssi, uint16_t ber10k, int16_t bto, int16_t lqual);
int pcu_tx_rach_ind(uint8_t bts_nr, uint8_t trx_nr, uint8_t ts_nr,
		    int16_t qta, uint16_t ra, uint32_t fn, uint8_t is_11bit,
		    enum ph_burst_type burst_type, uint8_t sapi);
int pcu_tx_time_ind(uint32_t fn);
int pcu_tx_interf_ind(uint8_t bts_nr, uint8_t trx_nr, uint32_t fn,
		      const uint8_t *pdch_interf);
int pcu_tx_pag_req(const uint8_t *identity_lv, uint8_t chan_needed);
int pcu_tx_pch_data_cnf(uint32_t fn, uint8_t *data, uint8_t len);
int pcu_tx_susp_req(struct gsm_lchan *lchan, uint32_t tlli, const uint8_t *ra_id, uint8_t cause);
int pcu_sock_send(struct gsm_network *net, struct msgb *msg);

int pcu_sock_init(const char *path);
void pcu_sock_exit(void);

bool pcu_connected(void);

#endif /* _PCU_IF_H */
