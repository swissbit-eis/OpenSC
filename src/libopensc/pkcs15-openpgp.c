/*
 * PKCS15 emulation layer for OpenPGP card.
 * To see how this works, run p15dump on your OpenPGP card.
 *
 * Copyright (C) 2003, Olaf Kirch <okir@suse.de>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "common/compat_strlcpy.h"
#include "internal.h"
#include "pkcs15.h"
#include "log.h"
#include "card-openpgp.h"

static int sc_pkcs15emu_openpgp_add_data(sc_pkcs15_card_t *);


#define	PGP_USER_PIN_FLAGS	(SC_PKCS15_PIN_FLAG_CASE_SENSITIVE \
				| SC_PKCS15_PIN_FLAG_INITIALIZED \
				| SC_PKCS15_PIN_FLAG_LOCAL)
#define PGP_ADMIN_PIN_FLAGS	(PGP_USER_PIN_FLAGS \
				| SC_PKCS15_PIN_FLAG_UNBLOCK_DISABLED \
				| SC_PKCS15_PIN_FLAG_SO_PIN)

#define PGP_NUM_PRIVDO       4
#define PGP_MAX_NUM_CERTS    3

typedef struct _pgp_pin_cfg {
	const char	*label;
	int		reference;
	unsigned int	flags;
	int		min_length;
	int		do_index;
} pgp_pin_cfg_t;

/* OpenPGP cards v1:
 * "Signature PIN2 & "Encryption PIN" are two different PINs - not sync'ed by hardware
 */
static const pgp_pin_cfg_t	pin_cfg_v1[3] = {
	{ "Encryption PIN", 0x02, PGP_USER_PIN_FLAGS,  6, 1 },	// used for PSO:DEC, INT-AUT, {GET,PUT} DATA
	{ "Signature PIN",  0x01, PGP_USER_PIN_FLAGS,  6, 0 },	// used for PSO:CDS
	{ "Admin PIN",      0x03, PGP_ADMIN_PIN_FLAGS, 8, 2 }
};
/* OpenPGP cards v2:
 * "User PIN (sig)" & "User PIN" are the same PIN, but use different references depending on action
 */
static const pgp_pin_cfg_t	pin_cfg_v2[3] = {
	{ "User PIN",       0x02, PGP_USER_PIN_FLAGS,  6, 0 },	// used for PSO:DEC, INT-AUT, {GET,PUT} DATA
	{ "User PIN (sig)", 0x01, PGP_USER_PIN_FLAGS,  6, 0 },	// used for PSO:CDS
	{ "Admin PIN",      0x03, PGP_ADMIN_PIN_FLAGS, 8, 2 }
};

static struct sc_object_id curve25519_oid = {{1, 3, 6, 1, 4, 1, 3029, 1, 5, 1, -1}};

#define PGP_SIG_PRKEY_USAGE	(SC_PKCS15_PRKEY_USAGE_SIGN \
				| SC_PKCS15_PRKEY_USAGE_SIGNRECOVER \
				| SC_PKCS15_PRKEY_USAGE_NONREPUDIATION)
#define	PGP_ENC_PRKEY_USAGE	(SC_PKCS15_PRKEY_USAGE_DECRYPT \
				| SC_PKCS15_PRKEY_USAGE_UNWRAP)
#define PGP_AUTH_PRKEY_USAGE	(SC_PKCS15_PRKEY_USAGE_NONREPUDIATION)

#define	PGP_SIG_PUBKEY_USAGE	(SC_PKCS15_PRKEY_USAGE_VERIFY \
				| SC_PKCS15_PRKEY_USAGE_VERIFYRECOVER)
#define	PGP_ENC_PUBKEY_USAGE	(SC_PKCS15_PRKEY_USAGE_ENCRYPT \
				| SC_PKCS15_PRKEY_USAGE_WRAP)
#define	PGP_AUTH_PUBKEY_USAGE	(SC_PKCS15_PRKEY_USAGE_VERIFY)

typedef	struct _pgp_key_cfg {
	const char	*label;
	const char	*pubkey_path;
	int		prkey_pin;
	int		prkey_usage;
	int		pubkey_usage;
} pgp_key_cfg_t;

typedef struct cdata_st {
	const char *label;
	int	    authority;
	const char *path;
	const char *id;
	int         obj_flags;
} cdata;

static const pgp_key_cfg_t key_cfg[3] = {
	{ "Signature key",      "B601", 1, PGP_SIG_PRKEY_USAGE,  PGP_SIG_PUBKEY_USAGE  },
	{ "Encryption key",     "B801", 2, PGP_ENC_PRKEY_USAGE,  PGP_ENC_PUBKEY_USAGE  },
	{ "Authentication key", "A401", 2, PGP_AUTH_PRKEY_USAGE | PGP_ENC_PRKEY_USAGE, PGP_AUTH_PUBKEY_USAGE | PGP_ENC_PUBKEY_USAGE }
};

static const cdata certs[PGP_MAX_NUM_CERTS] = {
	{"AUT certificate", 0, "3F007F21", "3", SC_PKCS15_CO_FLAG_MODIFIABLE},
	{"DEC certificate", 0, "3F007F21", "2", SC_PKCS15_CO_FLAG_MODIFIABLE},
	{"SIG certificate", 0, "3F007F21", "1", SC_PKCS15_CO_FLAG_MODIFIABLE}
};

typedef struct _pgp_manuf_map {
	unsigned short		id;
	const char	*name;
} pgp_manuf_map_t;

static const pgp_manuf_map_t manuf_map[] = {
	{ 0x0001, "PPC Card Systems"		},
	{ 0x0002, "Prism"			},
	{ 0x0003, "OpenFortress"		},
	{ 0x0004, "Wewid AB"			},
	{ 0x0005, "ZeitControl"			},
	{ 0x0006, "Yubico"			},
	{ 0x0007, "OpenKMS"			},
	{ 0x0008, "LogoEmail"			},
	{ 0x0009, "Fidesmo"			},
	{ 0x000A, "Dangerous Things"		},
	{ 0x000B, "Feitian Technologies"	},
	{ 0x002A, "Magrathea"			},
	{ 0x0042, "GnuPG e.V."			},
	{ 0x1337, "Warsaw Hackerspace"		},
	{ 0x2342, "warpzone"			},
	{ 0x4354, "Confidential Technologies"	},
	{ 0x5443, "TIF-IT e.V."			},
	{ 0x63AF, "Trustica"			},
	{ 0xBA53, "c-base e.V."			},
	{ 0xBD0E, "Paranoidlabs"		},
	{ 0xF517, "FSIJ"			},
	{ 0xF5EC, "F-Secure"			},
	{ 0x0000, "test card"			},
	{ 0xffff, "test card"			},
	{ 0, NULL }
};

/*
 * This function pretty much follows what find_tlv in the GNUpg
 * code does.
 */
static int
read_file(sc_card_t *card, const char *path_name, void *buf, size_t len)
{
	sc_path_t	path;
	sc_file_t	*file;
	int		r;

	sc_format_path(path_name, &path);
	if ((r = sc_select_file(card, &path, &file)) < 0)
		return r;

	if (file->size < len)
		len = file->size;

	sc_file_free(file);

	return sc_read_binary(card, 0, (u8 *) buf, len, 0);
}

static int
sc_pkcs15emu_openpgp_init(sc_pkcs15_card_t *p15card)
{
	sc_card_t	*card = p15card->card;
	sc_context_t	*ctx = card->ctx;
	char		string[256];
	u8		c4data[10];
	u8		c5data[100];
	int		r, i;
	const pgp_pin_cfg_t *pin_cfg = (card->type == SC_CARD_TYPE_OPENPGP_V1)
	                               ? pin_cfg_v1 : pin_cfg_v2;
	sc_path_t path;
	sc_file_t *file = NULL;

	LOG_FUNC_CALLED(ctx);

	set_string(&p15card->tokeninfo->label, "OpenPGP card");
	set_string(&p15card->tokeninfo->manufacturer_id, "OpenPGP project");

	/* card->serialnr = 2 byte manufacturer_id + 4 byte serial_number */
	if (card->serialnr.len > 0) {
		unsigned short manuf_id = bebytes2ushort(card->serialnr.value);
		int j;

		sc_bin_to_hex(card->serialnr.value, card->serialnr.len, string, sizeof(string), 0);
		set_string(&p15card->tokeninfo->serial_number, string);

		for (j = 0; manuf_map[j].name != NULL; j++) {
			if (manuf_id == manuf_map[j].id) {
				set_string(&p15card->tokeninfo->manufacturer_id, manuf_map[j].name);
				break;
			}
		}
	}

	p15card->tokeninfo->flags = SC_PKCS15_TOKEN_PRN_GENERATION | SC_PKCS15_TOKEN_EID_COMPLIANT;

	/* Extract preferred language */
	r = read_file(card, "0065:5f2d", string, sizeof(string)-1);
	if (r < 0)
		goto failed;
	string[r] = '\0';
	set_string(&p15card->tokeninfo->preferred_language, string);

	/* Get CHV status bytes from DO 006E/0073/00C4:
	 *  00:		1 == user consent for signature PIN
	 *		(i.e. PIN still valid for next PSO:CDS command)
	 *  01-03:	max length of pins 1-3
	 *  04-07:	tries left for pins 1-3
	 */
	sc_log(ctx, "Reading PW status bytes");
	if ((r = read_file(card, "006E:0073:00C4", c4data, sizeof(c4data))) < 0)
		goto failed;
	if (r != 7) {
		sc_log(ctx, 
			"CHV status bytes have unexpected length (expected 7, got %d)\n", r);
		r = SC_ERROR_OBJECT_NOT_VALID;
		goto failed;
	}

	/* Add PIN codes */
	for (i = 0; i < 3; i++) {
		sc_pkcs15_auth_info_t pin_info;
		sc_pkcs15_object_t   pin_obj;

		memset(&pin_info, 0, sizeof(pin_info));
		memset(&pin_obj,  0, sizeof(pin_obj));

		pin_info.auth_type = SC_PKCS15_PIN_AUTH_TYPE_PIN;
		pin_info.auth_id.len      = 1;
		pin_info.auth_id.value[0] = pin_cfg[i].reference;
		pin_info.attrs.pin.reference     = pin_cfg[i].reference;
		pin_info.attrs.pin.flags         = pin_cfg[i].flags;
		pin_info.attrs.pin.type          = SC_PKCS15_PIN_TYPE_UTF8;
		pin_info.attrs.pin.min_length    = pin_cfg[i].min_length;
		pin_info.attrs.pin.stored_length = c4data[1 + pin_cfg[i].do_index];
		pin_info.attrs.pin.max_length    = c4data[1 + pin_cfg[i].do_index];
		pin_info.attrs.pin.pad_char      = '\0';
		pin_info.tries_left = c4data[4 + pin_cfg[i].do_index];
		pin_info.logged_in = SC_PIN_STATE_UNKNOWN;

		sc_format_path("3F00", &pin_info.path);

		strlcpy(pin_obj.label, pin_cfg[i].label, sizeof(pin_obj.label));
		pin_obj.flags = SC_PKCS15_CO_FLAG_MODIFIABLE | SC_PKCS15_CO_FLAG_PRIVATE;
		if (i < 2) {
			pin_obj.auth_id.len = 1;
			pin_obj.auth_id.value[0] = 3;
		}

		r = sc_pkcs15emu_add_pin_obj(p15card, &pin_obj, &pin_info);
		if (r < 0) {
			r = SC_ERROR_INTERNAL;
			goto failed;
		}
	}

	/* Get private key finger prints from DO 006E/0073/00C5:
	 *  00-19:	finger print for SIG key
	 *  20-39:	finger print for ENC key
	 *  40-59:	finger print for AUT key
	 */
	sc_log(ctx, "Reading Fingerprints");
	if ((r = read_file(card, "006E:0073:00C5", c5data, sizeof(c5data))) < 0)
		goto failed;
	if (r < 60) {
		sc_log(ctx,
			"finger print bytes have unexpected length (expected 60, got %d)\n", r);
		r = SC_ERROR_OBJECT_NOT_VALID;
		goto failed;
	}

	sc_log(ctx, "Adding private keys");
	/* XXX: check if "halfkeys" can be stored with gpg2. If not, add key pairs in one loop */
	for (i = 0; i < 3; i++) {
		sc_pkcs15_prkey_info_t prkey_info;
		sc_pkcs15_object_t     prkey_obj;
		u8 cxdata[12];
		int cxdata_len = sizeof(cxdata);
		char path_template[] = "006E:0073:00Cx";
		int j;

		memset(&prkey_info, 0, sizeof(prkey_info));
		memset(&prkey_obj,  0, sizeof(prkey_obj));
		memset(&cxdata, 0, sizeof(cxdata));

		path_template[13] = '1' + i; /* The needed tags are C1 C2 and C3 */
		if ((cxdata_len = read_file(card, path_template, cxdata, sizeof(cxdata))) < 1)
			goto failed;

		/* check validity using finger prints */
		for (j = 19; j >= 0; j--) {
			if (c5data[20 * i + j] != '\0')
				break;
		}

		/* only add valid keys, i.e. those with a legal algorithm identifier & finger print */
		if (j >= 0 && cxdata[0] != 0) {
			struct sc_object_id oid;
			struct sc_algorithm_info * algorithm_info; /* no need to free */

			algorithm_info = NULL;
			prkey_info.id.len         = 1;
			prkey_info.id.value[0]    = i + 1;
			prkey_info.usage          = key_cfg[i].prkey_usage;
			prkey_info.native         = 1;
			prkey_info.key_reference  = i;

			strlcpy(prkey_obj.label, key_cfg[i].label, sizeof(prkey_obj.label));
			prkey_obj.flags = SC_PKCS15_CO_FLAG_PRIVATE | SC_PKCS15_CO_FLAG_MODIFIABLE;
			prkey_obj.auth_id.len      = 1;
			prkey_obj.auth_id.value[0] = key_cfg[i].prkey_pin;

			/* need to get size from algorithms using oid */
			if (cxdata[0] == SC_OPENPGP_KEYALGO_ECDH ||
				cxdata[0] == SC_OPENPGP_KEYALGO_ECDSA ||
				cxdata[0] == SC_OPENPGP_KEYALGO_EDDSA) {
				/* Last byte could be Import-Format of private key, let's ignore it,
				 * as it is not part of OID */
				if (cxdata[cxdata_len-1] == SC_OPENPGP_KEYFORMAT_EC_STD ||
				    cxdata[cxdata_len-1] == SC_OPENPGP_KEYFORMAT_EC_STDPUB)
					cxdata_len--;
				r = sc_asn1_decode_object_id(&cxdata[1], cxdata_len-1, &oid);
				if (r != SC_SUCCESS) {
					sc_log(ctx, "Failed to parse OID for elliptic curve algorithm");
				}
			}

			switch (cxdata[0]) {
			case SC_OPENPGP_KEYALGO_ECDH:
				if (sc_compare_oid(&oid, &curve25519_oid)) {
					if ((algorithm_info = sc_card_find_xeddsa_alg(card, 0, &oid)))
						prkey_info.field_length = algorithm_info->key_length;
					else {
						sc_log(ctx, "algorithm not found");
						continue;
					}
					break;
				}
				/* Fall through */
			case SC_OPENPGP_KEYALGO_ECDSA:
				if((algorithm_info = sc_card_find_ec_alg(card, 0, &oid)))
					prkey_info.field_length = algorithm_info->key_length;
				else {
					sc_log(ctx, "algorithm not found");
					continue;
				}
				break;
			case SC_OPENPGP_KEYALGO_EDDSA:
				if ((algorithm_info = sc_card_find_eddsa_alg(card, 0, &oid)))
					prkey_info.field_length = algorithm_info->key_length;
				else {
					sc_log(ctx, "algorithm not found");
					continue;
				}
				break;
			}

			switch (cxdata[0]) {
			case SC_OPENPGP_KEYALGO_EDDSA:
				/* Filter out invalid usage: EdDSA does not support anything but sign */
				prkey_info.usage &= PGP_SIG_PRKEY_USAGE;
				r = sc_pkcs15emu_add_eddsa_prkey(p15card, &prkey_obj, &prkey_info);
				break;

			case SC_OPENPGP_KEYALGO_ECDH:
				/* This can result in either ECDSA key or EC_MONTGOMERY
				 * so we need to check OID */
				if (sc_compare_oid(&oid, &curve25519_oid)) {
					/* This can do only DERIVE */
					prkey_info.usage = SC_PKCS15_PRKEY_USAGE_DERIVE;
					r = sc_pkcs15emu_add_xeddsa_prkey(p15card, &prkey_obj, &prkey_info);
					break;
				}
				prkey_info.usage |= SC_PKCS15_PRKEY_USAGE_DERIVE;
				prkey_info.usage &= ~PGP_ENC_PRKEY_USAGE;
				r = sc_pkcs15emu_add_ec_prkey(p15card, &prkey_obj, &prkey_info);
				break;

			case SC_OPENPGP_KEYALGO_ECDSA:
				prkey_info.usage = SC_PKCS15_PRKEY_USAGE_SIGN;
				r = sc_pkcs15emu_add_ec_prkey(p15card, &prkey_obj, &prkey_info);
				break;

			case SC_OPENPGP_KEYALGO_RSA:
				if (cxdata_len >= 3) {
					prkey_info.modulus_length = bebytes2ushort(cxdata + 1);
					r = sc_pkcs15emu_add_rsa_prkey(p15card, &prkey_obj, &prkey_info);
					break;
				}
				/* Fallthrough */
			default:
				sc_log(ctx, "Invalid algorithm identifier %x (length = %d)",
					cxdata[0], r);
			}

			if (r < 0) {
				r = SC_ERROR_INTERNAL;
				goto failed;
			}
		}
	}

	sc_log(ctx, "Adding public keys");
	/* Add public keys */
	for (i = 0; i < 3; i++) {
		sc_pkcs15_pubkey_info_t pubkey_info;
		sc_pkcs15_object_t      pubkey_obj;
		u8 cxdata[12];
		int cxdata_len = sizeof(cxdata);
		char path_template[] = "006E:0073:00Cx";
		int j;

		memset(&pubkey_info, 0, sizeof(pubkey_info));
		memset(&pubkey_obj,  0, sizeof(pubkey_obj));
		memset(&cxdata, 0, sizeof(cxdata));

		path_template[13] = '1' + i; /* The needed tags are C1 C2 and C3 */
		if ((cxdata_len = read_file(card, path_template, cxdata, sizeof(cxdata))) < 1)
			goto failed;

		/* check validity using finger prints */
		for (j = 19; j >= 0; j--) {
			if (c5data[20 * i + j] != '\0')
				break;
		}

		/* only add valid keys, i.e. those with a legal algorithm identifier & finger print */
		if (j >= 0 && cxdata[0] != 0) {
			struct sc_object_id oid;
			struct sc_algorithm_info * algorithm_info; /* no need to free */

			algorithm_info = NULL;
			pubkey_info.id.len         = 1;
			pubkey_info.id.value[0]    = i + 1;
			pubkey_info.usage          = key_cfg[i].pubkey_usage;
			sc_format_path(key_cfg[i].pubkey_path, &pubkey_info.path);

			strlcpy(pubkey_obj.label, key_cfg[i].label, sizeof(pubkey_obj.label));
			pubkey_obj.flags = SC_PKCS15_CO_FLAG_MODIFIABLE;

			if (cxdata[0] == SC_OPENPGP_KEYALGO_ECDH ||
				cxdata[0] == SC_OPENPGP_KEYALGO_ECDSA ||
				cxdata[0] == SC_OPENPGP_KEYALGO_EDDSA) {
				/* Last byte could be Import-Format of private key, let's ignore it,
				 * as it is not part of OID */
				if (cxdata[cxdata_len-1] == SC_OPENPGP_KEYFORMAT_EC_STD ||
				    cxdata[cxdata_len-1] == SC_OPENPGP_KEYFORMAT_EC_STDPUB)
					cxdata_len--;
				r = sc_asn1_decode_object_id(&cxdata[1], cxdata_len-1, &oid);
				if (r != SC_SUCCESS) {
					sc_log(ctx, "Failed to parse OID for elliptic curve algorithm");
				}
			}

			switch (cxdata[0]) {
			case SC_OPENPGP_KEYALGO_ECDH:
				if (sc_compare_oid(&oid, &curve25519_oid)) {
					if ((algorithm_info = sc_card_find_xeddsa_alg(card, 0, &oid)))
						pubkey_info.field_length = algorithm_info->key_length;
					else {
						sc_log(ctx, "algorithm not found");
						continue;
					}
					break;
				}
				/* Fall through */
			case SC_OPENPGP_KEYALGO_ECDSA:
				if((algorithm_info = sc_card_find_ec_alg(card, 0, &oid)))
					pubkey_info.field_length = algorithm_info->key_length;
				else {
					sc_log(ctx, "algorithm not found");
					continue;
				}
				break;
			case SC_OPENPGP_KEYALGO_EDDSA:
				if ((algorithm_info = sc_card_find_eddsa_alg(card, 0, &oid)))
					pubkey_info.field_length = algorithm_info->key_length;
				else {
					sc_log(ctx, "algorithm not found");
					continue;
				}
				break;
			}

			switch (cxdata[0]) {
			case SC_OPENPGP_KEYALGO_EDDSA:
				/* assuming Ed25519 as it is the only supported now */
				/* Filter out invalid usage: ED does not support anything but sign */
				pubkey_info.usage &= PGP_SIG_PUBKEY_USAGE;
				r = sc_pkcs15emu_add_eddsa_pubkey(p15card, &pubkey_obj, &pubkey_info);
				break;
			case SC_OPENPGP_KEYALGO_ECDH:
				/* This can result in either ECDSA key or EC_MONTGOMERY
				 * so we need to check OID */
				if (sc_compare_oid(&oid, &curve25519_oid)) {
					/* XXX What can this key do? */
					pubkey_info.usage = SC_PKCS15_PRKEY_USAGE_DERIVE;
					r = sc_pkcs15emu_add_xeddsa_pubkey(p15card, &pubkey_obj, &pubkey_info);
					break;
				}
				pubkey_info.usage = SC_PKCS15_PRKEY_USAGE_DERIVE;
				r = sc_pkcs15emu_add_ec_pubkey(p15card, &pubkey_obj, &pubkey_info);
				break;
			case SC_OPENPGP_KEYALGO_ECDSA:
				pubkey_info.usage = PGP_SIG_PUBKEY_USAGE;
				r = sc_pkcs15emu_add_ec_pubkey(p15card, &pubkey_obj, &pubkey_info);
				break;
			case SC_OPENPGP_KEYALGO_RSA:
				if (cxdata_len >= 3) {
					pubkey_info.modulus_length = bebytes2ushort(cxdata + 1);
					r = sc_pkcs15emu_add_rsa_pubkey(p15card, &pubkey_obj, &pubkey_info);
					break;
				}
				/* Fall through */
			default:
				sc_log(ctx, "Invalid algorithm identifier %x (length = %d)",
					cxdata[0], r);
			}

			if (r < 0) {
				r = SC_ERROR_INTERNAL;
				goto failed;
			}
		}
	}

	/* Check if certificate DO 7F21 holds data */
	sc_format_path("7F21", &path);
	r = sc_select_file(card, &path, &file);
	if (r < 0)
		goto failed;

	for(u8 i=0; i<PGP_MAX_NUM_CERTS; i++) {
		struct sc_pkcs15_cert_info cert_info;
		struct sc_pkcs15_object    cert_obj;
		u8* buffer = malloc(MAX_OPENPGP_DO_SIZE);
		int resp_len = 0;

		if (buffer == NULL)
			goto failed;

		memset(&cert_info, 0, sizeof(cert_info));
		memset(&cert_obj,  0, sizeof(cert_obj));

		/* only try to SELECT DATA for OpenPGP >= v3 */
		if (card->type >= SC_CARD_TYPE_OPENPGP_V3) {
			r = sc_card_ctl(card, SC_CARDCTL_OPENPGP_SELECT_DATA, &i);
			if (r < 0) {
				free(buffer);
				LOG_TEST_RET(card->ctx, r, "Failed OpenPGP - select data");
			}
		}
		sc_format_path(certs[i].path, &cert_info.path);

		/* Certificate ID. We use the same ID as the authentication key */
		sc_pkcs15_format_id(certs[i].id, &cert_info.id);

		resp_len = sc_get_data(card, 0x7F21, buffer, MAX_OPENPGP_DO_SIZE);

		/* Response length => free buffer and continue with next id */
		if (resp_len == 0) {
			free(buffer);
			continue;
		}

		/* Catch error during sc_get_data */
		if (resp_len < 0) {
			free(buffer);
			goto failed;
		}

		/* Assemble certificate info struct, based on `certs` array */
		cert_info.value.len = resp_len;
		cert_info.value.value = buffer;
		cert_info.authority = certs[i].authority;
		cert_obj.flags = certs[i].obj_flags;

		/* Object label */
		strlcpy(cert_obj.label, certs[i].label, sizeof(cert_obj.label));

		r = sc_pkcs15emu_add_x509_cert(p15card, &cert_obj, &cert_info);
		if (r < 0) {
			free(buffer);
			goto failed;
		}

		/* only iterate, for OpenPGP >= v3, thus break on < v3 */
		if (card->type < SC_CARD_TYPE_OPENPGP_V3)
			break;
	}

	/* Add PKCS#15 DATA objects from other OpenPGP card DOs. The return
	 * value is ignored, so this will not cause initialization to fail.
	 */
	sc_pkcs15emu_openpgp_add_data(p15card);

failed:
	if (r < 0) {
		sc_log(card->ctx,
				"Failed to initialize OpenPGP emulation: %s\n",
				sc_strerror(r));
		sc_pkcs15_card_clear(p15card);
	}
	sc_file_free(file);
	LOG_FUNC_RETURN(ctx, r);
}

static int
sc_pkcs15emu_openpgp_add_data(sc_pkcs15_card_t *p15card)
{
	sc_context_t *ctx = p15card->card->ctx;
	int i, r;

	LOG_FUNC_CALLED(ctx);
	/* Optional private use DOs 0101 to 0104 */
	for (i = 1; i <= PGP_NUM_PRIVDO; i++) {
		sc_pkcs15_data_info_t dat_info;
		sc_pkcs15_object_t dat_obj;
		char name[8];
		char path[9];
		u8 content[254];
		memset(&dat_info, 0, sizeof(dat_info));
		memset(&dat_obj, 0, sizeof(dat_obj));

		snprintf(name, 8, "PrivDO%d", i);
		snprintf(path, 9, "3F00010%d", i);

		/* Check if the DO can be read and is not empty. Otherwise we
		 * won't expose a PKCS#15 DATA object.
		 */
		r = read_file(p15card->card, path, content, sizeof(content));
		if (r <= 0 ) {
			sc_log(ctx, "Cannot read DO 010%d or there is no data in it", i);
			/* Skip */
			continue;
		}
		sc_format_path(path, &dat_info.path);
		strlcpy(dat_obj.label, name, sizeof(dat_obj.label));
		strlcpy(dat_info.app_label, name, sizeof(dat_info.app_label));

		/* Add DATA object to slot protected by PIN2 (PW1 with Ref 0x82) */
		dat_obj.flags = SC_PKCS15_CO_FLAG_PRIVATE | SC_PKCS15_CO_FLAG_MODIFIABLE;
		dat_obj.auth_id.len = 1;
		if (i == 1 || i == 3)
			dat_obj.auth_id.value[0] = 2;
		else
			dat_obj.auth_id.value[0] = 3;

		sc_log(ctx, "Add %s data object", name);
		r = sc_pkcs15emu_add_data_object(p15card, &dat_obj, &dat_info);
		LOG_TEST_RET(ctx, r, "Could not add data object to framework");
	}
	LOG_FUNC_RETURN(ctx, SC_SUCCESS);
}

static int openpgp_detect_card(sc_pkcs15_card_t *p15card)
{
	if (p15card->card->type == SC_CARD_TYPE_OPENPGP_BASE
			|| p15card->card->type == SC_CARD_TYPE_OPENPGP_V1
			|| p15card->card->type == SC_CARD_TYPE_OPENPGP_V2
			|| p15card->card->type == SC_CARD_TYPE_OPENPGP_GNUK
			|| p15card->card->type == SC_CARD_TYPE_OPENPGP_V3)
		return SC_SUCCESS;
	else
		return SC_ERROR_WRONG_CARD;
}

int sc_pkcs15emu_openpgp_init_ex(sc_pkcs15_card_t *p15card, struct sc_aid *aid)
{
	if (openpgp_detect_card(p15card))
		return SC_ERROR_WRONG_CARD;
	return sc_pkcs15emu_openpgp_init(p15card);
}
