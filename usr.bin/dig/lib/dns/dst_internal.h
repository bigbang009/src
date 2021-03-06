/*
 * Portions Copyright (C) Internet Systems Consortium, Inc. ("ISC")
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC AND NETWORK ASSOCIATES DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE
 * FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * See the COPYRIGHT file distributed with this work for additional
 * information regarding copyright ownership.
 *
 * Portions Copyright (C) Network Associates, Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC AND NETWORK ASSOCIATES DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE
 * FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: dst_internal.h,v 1.6 2020/02/18 18:11:27 florian Exp $ */

#ifndef DST_DST_INTERNAL_H
#define DST_DST_INTERNAL_H 1

#include <isc/buffer.h>
#include <isc/region.h>
#include <isc/types.h>
#include <isc/refcount.h>
#include <isc/sha1.h>
#include <isc/sha2.h>
#include <isc/hmacsha.h>

#include <dns/time.h>
#include <dst/dst.h>

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/objects.h>
#include <openssl/rsa.h>

/***
 *** Types
 ***/

typedef struct dst_func dst_func_t;

typedef struct dst_hmacsha1_key   dst_hmacsha1_key_t;
typedef struct dst_hmacsha224_key dst_hmacsha224_key_t;
typedef struct dst_hmacsha256_key dst_hmacsha256_key_t;
typedef struct dst_hmacsha384_key dst_hmacsha384_key_t;
typedef struct dst_hmacsha512_key dst_hmacsha512_key_t;

/*%
 * Indicate whether a DST context will be used for signing
 * or for verification
 */
typedef enum { DO_SIGN, DO_VERIFY } dst_use_t;

/*% DST Key Structure */
struct dst_key {
	isc_refcount_t	refs;
	dns_name_t *	key_name;	/*%< name of the key */
	unsigned int	key_size;	/*%< size of the key in bits */
	unsigned int	key_proto;	/*%< protocols this key is used for */
	unsigned int	key_alg;	/*%< algorithm of the key */
	uint32_t	key_flags;	/*%< flags of the public key */
	uint16_t	key_id;		/*%< identifier of the key */
	uint16_t	key_rid;	/*%< identifier of the key when
					     revoked */
	uint16_t	key_bits;	/*%< hmac digest bits */
	dns_rdataclass_t key_class;	/*%< class of the key record */
	dns_ttl_t	key_ttl;	/*%< default/initial dnskey ttl */
	char		*engine;	/*%< engine name (HSM) */
	char		*label;		/*%< engine label (HSM) */
	union {
		void *generic;
		EVP_PKEY *pkey;
		dst_hmacsha1_key_t *hmacsha1;
		dst_hmacsha224_key_t *hmacsha224;
		dst_hmacsha256_key_t *hmacsha256;
		dst_hmacsha384_key_t *hmacsha384;
		dst_hmacsha512_key_t *hmacsha512;

	} keydata;			/*%< pointer to key in crypto pkg fmt */

	time_t	times[DST_MAX_TIMES + 1];    /*%< timing metadata */
	isc_boolean_t	timeset[DST_MAX_TIMES + 1];  /*%< data set? */
	time_t	nums[DST_MAX_NUMERIC + 1];   /*%< numeric metadata */
	isc_boolean_t	numset[DST_MAX_NUMERIC + 1]; /*%< data set? */
	isc_boolean_t 	inactive;      /*%< private key not present as it is
					    inactive */
	isc_boolean_t 	external;      /*%< external key */

	int		fmt_major;     /*%< private key format, major version */
	int		fmt_minor;     /*%< private key format, minor version */

	dst_func_t *    func;	       /*%< crypto package specific functions */
	isc_buffer_t   *key_tkeytoken; /*%< TKEY token data */
};

struct dst_context {
	dst_use_t use;
	dst_key_t *key;
	isc_logcategory_t *category;
	union {
		void *generic;
		isc_sha1_t *sha1ctx;
		isc_sha256_t *sha256ctx;
		isc_sha512_t *sha512ctx;
		isc_hmacsha1_t *hmacsha1ctx;
		isc_hmacsha224_t *hmacsha224ctx;
		isc_hmacsha256_t *hmacsha256ctx;
		isc_hmacsha384_t *hmacsha384ctx;
		isc_hmacsha512_t *hmacsha512ctx;
		EVP_MD_CTX *evp_md_ctx;
	} ctxdata;
};

struct dst_func {
	/*
	 * Context functions
	 */
	isc_result_t (*createctx)(dst_key_t *key, dst_context_t *dctx);
	isc_result_t (*createctx2)(dst_key_t *key, int maxbits,
				   dst_context_t *dctx);
	void (*destroyctx)(dst_context_t *dctx);
	isc_result_t (*adddata)(dst_context_t *dctx, const isc_region_t *data);

	/*
	 * Key operations
	 */
	isc_result_t (*sign)(dst_context_t *dctx, isc_buffer_t *sig);
	isc_result_t (*verify)(dst_context_t *dctx, const isc_region_t *sig);
	isc_result_t (*verify2)(dst_context_t *dctx, int maxbits,
				const isc_region_t *sig);
	isc_result_t (*computesecret)(const dst_key_t *pub,
				      const dst_key_t *priv,
				      isc_buffer_t *secret);
	isc_boolean_t (*compare)(const dst_key_t *key1, const dst_key_t *key2);
	isc_boolean_t (*paramcompare)(const dst_key_t *key1,
				      const dst_key_t *key2);
	isc_result_t (*generate)(dst_key_t *key, int parms,
				 void (*callback)(int));
	isc_boolean_t (*isprivate)(const dst_key_t *key);
	void (*destroy)(dst_key_t *key);

	/* conversion functions */
	isc_result_t (*todns)(const dst_key_t *key, isc_buffer_t *data);
	isc_result_t (*fromdns)(dst_key_t *key, isc_buffer_t *data);
	isc_result_t (*tofile)(const dst_key_t *key, const char *directory);
	isc_result_t (*parse)(dst_key_t *key,
			      isc_lex_t *lexer,
			      dst_key_t *pub);

	/* cleanup */
	void (*cleanup)(void);

	isc_result_t (*fromlabel)(dst_key_t *key, const char *engine,
				  const char *label, const char *pin);
	isc_result_t (*dump)(dst_key_t *key, char **buffer,
			     int *length);
	isc_result_t (*restore)(dst_key_t *key, const char *keystr);
};

/*%
 * Initializers
 */
isc_result_t dst__openssl_init(void);

isc_result_t dst__hmacsha1_init(struct dst_func **funcp);
isc_result_t dst__hmacsha224_init(struct dst_func **funcp);
isc_result_t dst__hmacsha256_init(struct dst_func **funcp);
isc_result_t dst__hmacsha384_init(struct dst_func **funcp);
isc_result_t dst__hmacsha512_init(struct dst_func **funcp);
isc_result_t dst__opensslrsa_init(struct dst_func **funcp,
				  unsigned char algorithm);
isc_result_t dst__opensslecdsa_init(struct dst_func **funcp);

/*%
 * Destructors
 */
void dst__openssl_destroy(void);

/*%
 * Memory allocators using the DST memory pool.
 */
void * dst__mem_alloc(size_t size);
void   dst__mem_free(void *ptr);
void * dst__mem_realloc(void *ptr, size_t size);

#endif /* DST_DST_INTERNAL_H */
/*! \file */
