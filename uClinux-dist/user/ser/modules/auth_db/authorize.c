/*
 * $Id: authorize.c,v 1.21.2.3 2005/09/08 20:14:48 janakj Exp $
 *
 * Digest Authentication - Database support
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * history:
 * ---------
 * 2003-02-28 scratchpad compatibility abandoned
 * 2003-01-27 next baby-step to removing ZT - PRESERVE_ZT (jiri)
 * 2004-06-06 updated to the new DB api, added auth_db_{init,bind,close,ver}
 *             (andrei)
 */


#include <string.h>
#include "../../ut.h"
#include "../../str.h"
#include "../../db/db.h"
#include "../../dprint.h"
#include "../../parser/digest/digest.h"
#include "../../parser/hf.h"
#include "../../parser/parser_f.h"
#include "../../usr_avp.h"
#include "../../mem/mem.h"
#include "authdb_mod.h"
#include "rfc2617.h"


#define MESSAGE_500 "Server Internal Error"

static db_con_t* db_handle=0; /* database connection handle */
static db_func_t  auth_dbf;

static inline int get_ha1(struct username* _username, str* _domain,
    char* _table, char* _ha1, db_res_t** res)
{
	db_key_t keys[2];
	db_val_t vals[2];
	db_key_t *col;
	str result;
	int n, nc;

	col = pkg_malloc(sizeof(*col) * (avps_int_n + avps_str_n + 1));
	if (col == NULL) {
		LOG(L_ERR, "get_ha1(): Error while allocating memory\n");
		return -1;
	}

	keys[0] = user_column.s;
	keys[1] = domain_column.s;
	col[0] = (_username->domain.len && !calc_ha1) ? (pass_column_2.s) : (pass_column.s);
	for (n = 0; n < avps_int_n; n++)
		col[1 + n] = avps_int[n].s;
	for (n = 0; n < avps_str_n; n++)
		col[1 + avps_int_n + n] = avps_str[n].s;

	VAL_TYPE(vals) = VAL_TYPE(vals + 1) = DB_STR;
	VAL_NULL(vals) = VAL_NULL(vals + 1) = 0;

	VAL_STR(vals).s = _username->user.s;
	VAL_STR(vals).len = _username->user.len;

	VAL_STR(vals + 1).s = _domain->s;
	VAL_STR(vals + 1).len = _domain->len;

	n = (use_domain ? 2 : 1);
	nc = 1 + avps_int_n + avps_str_n;
	if (auth_dbf.use_table(db_handle, _table) < 0) {
		LOG(L_ERR, "get_ha1(): Error in use_table\n");
		pkg_free(col);
		return -1;
	}

	if (auth_dbf.query(db_handle, keys, 0, vals, col, n, nc, 0, res) < 0) {
		LOG(L_ERR, "get_ha1(): Error while querying database\n");
		pkg_free(col);
		return -1;
	}
	pkg_free(col);

	if ((RES_ROW_N(*res) == 0) || VAL_NULL(ROW_VALUES(RES_ROWS(*res)))) {
		DBG("get_ha1(): no result for user \'%.*s@%.*s\'\n",
		    _username->user.len, ZSW(_username->user.s), (use_domain ? (_domain->len) : 0), ZSW(_domain->s));
		return 1;
	}

        result.s = (char*)ROW_VALUES(RES_ROWS(*res))[0].val.string_val;
	result.len = strlen(result.s);

	if (calc_ha1) {
		     /* Only plaintext passwords are stored in database,
		      * we have to calculate HA1 */
		calc_HA1(HA_MD5, &_username->whole, _domain, &result, 0, 0, _ha1);
		DBG("HA1 string calculated: %s\n", _ha1);
	} else {
		memcpy(_ha1, result.s, result.len);
		_ha1[result.len] = '\0';
	}

	return 0;
}

/*
 * Calculate the response and compare with the given response string
 * Authorization is successful if this two strings are same
 */
static inline int check_response(dig_cred_t* _cred, str* _method, char* _ha1)
{
	HASHHEX resp, hent;

	     /*
	      * First, we have to verify that the response received has
	      * the same length as responses created by us
	      */
	if (_cred->response.len != 32) {
		DBG("check_response(): Receive response len != 32\n");
		return 1;
	}

	     /*
	      * Now, calculate our response from parameters received
	      * from the user agent
	      */
	calc_response(_ha1, &(_cred->nonce), 
		      &(_cred->nc), &(_cred->cnonce), 
		      &(_cred->qop.qop_str), _cred->qop.qop_parsed == QOP_AUTHINT,
		      _method, &(_cred->uri), hent, resp);
	
	DBG("check_response(): Our result = \'%s\'\n", resp);
	
	     /*
	      * And simply compare the strings, the user is
	      * authorized if they match
	      */
	if (!memcmp(resp, _cred->response.s, 32)) {
		DBG("check_response(): Authorization is OK\n");
		return 0;
	} else {
		DBG("check_response(): Authorization failed\n");
		return 2;
	}
}


/*
 * Authorize digest credentials
 */
static inline int authorize(struct sip_msg* _m, str* _realm, char* _table, int _hftype)
{
	char ha1[256];
	int res, i;
	struct hdr_field* h;
	auth_body_t* cred;
	auth_result_t ret;
	str domain, value;
	int_str iname, ivalue;
	db_res_t* result;
	str rpid;

	domain = *_realm;

	ret = pre_auth_func(_m, &domain, _hftype, &h);

	switch(ret) {
	case ERROR:            return 0;
	case NOT_AUTHORIZED:   return -1;
	case DO_AUTHORIZATION: break;
	case AUTHORIZED:       return 1;
	}

	cred = (auth_body_t*)h->parsed;

	res = get_ha1(&cred->digest.username, &domain, _table, ha1, &result);
        if (res < 0) {
		     /* Error while accessing the database */
		if (sl_reply(_m, (char*)500, MESSAGE_500) == -1) {
			LOG(L_ERR, "authorize(): Error while sending 500 reply\n");
		}
		return 0;
	}
	if (res > 0) {
		     /* Username not found in the database */
		auth_dbf.free_result(db_handle, result);
		return -1;
	}

	     /* Recalculate response, it must be same to authorize successfully */
        if (!check_response(&(cred->digest), &_m->first_line.u.request.method, ha1)) {
		rpid.s = NULL;
		rpid.len = 0;
		for (i = 0; i < avps_str_n; i++) {
			if (avps_str[i].len != 4
					|| VAL_NULL(&(result->rows[0].values[1 + avps_int_n + i]))
					|| memcmp(avps_str[i].s, "rpid", 4) != 0)
				continue;
			rpid.s = (char*)VAL_STRING(&(result->rows[0].values[1 + avps_int_n + i]));
			if(rpid.s!=NULL)
				rpid.len = strlen(rpid.s);
		}
		ret = post_auth_func(_m, h, &rpid);
		switch(ret) {
		case ERROR:
			auth_dbf.free_result(db_handle, result);
			return 0;
		case NOT_AUTHORIZED:
			auth_dbf.free_result(db_handle, result);
			return -1;
		case AUTHORIZED:
			for (i = 0; i < avps_int_n; i++) {
				if(VAL_NULL(&(result->rows[0].values[1 + i])))
					continue;
				iname.s = &(avps_int[i]);
				ivalue.n = VAL_INT(&(result->rows[0].values[1 + i]));
				add_avp(AVP_NAME_STR, iname, ivalue);
				DBG("authorize(): set integer AVP \'%.*s = %d\'\n",
				    iname.s->len, ZSW(iname.s->s), ivalue.n);
			}
			for (i = 0; i < avps_str_n; i++) {
				value.s = (char*)VAL_STRING(&(result->rows[0].values[1 + avps_int_n + i]));
				if(VAL_NULL(&(result->rows[0].values[1 + avps_int_n + i]))
						|| value.s==NULL)
					continue;
				iname.s = &(avps_str[i]);
				value.len = strlen(value.s);
				ivalue.s = &value;
				add_avp(AVP_NAME_STR | AVP_VAL_STR, iname, ivalue);
				DBG("authorize(): set string AVP \'%.*s = %.*s\'\n",
				    iname.s->len, ZSW(iname.s->s), value.len, ZSW(value.s));
			}
			auth_dbf.free_result(db_handle, result);
			return 1;
		default:
			auth_dbf.free_result(db_handle, result);
			return -1;
		}
	}

	auth_dbf.free_result(db_handle, result);
	return -1;
}


/*
 * Authorize using Proxy-Authorize header field
 */
int proxy_authorize(struct sip_msg* _m, char* _realm, char* _table)
{
	     /* realm parameter is converted to str* in str_fixup */
	return authorize(_m, (str*)_realm, _table, HDR_PROXYAUTH);
}


/*
 * Authorize using WWW-Authorize header field
 */
int www_authorize(struct sip_msg* _m, char* _realm, char* _table)
{
	return authorize(_m, (str*)_realm, _table, HDR_AUTHORIZATION);
}



int auth_db_init(char* db_url)
{
	if (auth_dbf.init==0){
		LOG(L_CRIT, "BUG: auth_db_bind: null dbf\n");
		goto error;
	}
	db_handle=auth_dbf.init(db_url);
	if (db_handle==0){
		LOG(L_ERR, "ERROR: auth_db_bind: unable to connect to the database\n");
		goto error;
	}
	return 0;
error:
	return -1;
}


int auth_db_bind(char* db_url)
{
	if (bind_dbmod(db_url, &auth_dbf)<0){
		LOG(L_ERR, "ERROR: auth_db_bind: unable to bind to the database"
				" module\n");
		return -1;
	}
	return 0;
}


void auth_db_close()
{
	if (db_handle && auth_dbf.close){
		auth_dbf.close(db_handle);
		db_handle=0;
	}
}


int auth_db_ver(char* db_url, str* name)
{
	db_con_t* dbh;
	int ver;

	if (auth_dbf.init==0){
		LOG(L_CRIT, "BUG: auth_db_ver: unbound database\n");
		return -1;
	}
	dbh=auth_dbf.init(db_url);
	if (dbh==0){
		LOG(L_ERR, "ERROR: auth_db_ver: unable to open database connection\n");
		return -1;
	}
	ver=table_version(&auth_dbf, dbh, name);
	auth_dbf.close(dbh);
	return ver;
}
