/*-
 * Copyright (c) 2017 Ribose Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*-
 * Copyright (c) 2009 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Alistair Crooks (agc@NetBSD.org)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Copyright (c) 2005-2008 Nominet UK (www.nic.uk)
 * All rights reserved.
 * Contributors: Ben Laurie, Rachel Willmer. The Contributors have asserted
 * their moral rights under the UK Copyright Design and Patents Act 1988 to
 * be recorded as the authors of this copyright work.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License.
 *
 * You may obtain a copy of the License at
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/** \file
 */
#include "config.h"
#include "crypto.h"
#include "readerwriter.h"
#include "rnpdefs.h"

/**
   \ingroup Core_Crypto
   \brief Recovers message digest from the signature
   \param out Where to write decrypted data to
   \param in Encrypted data
   \param length Length of encrypted data
   \param pubkey RSA public key
   \return size of recovered message digest
*/
int
pgp_rsa_public_decrypt(uint8_t *out,
			const uint8_t *in,
			size_t length,
			const pgp_rsa_pubkey_t *pubkey)
{
        size_t out_bytes = 0;
        size_t n_bytes = 0;
        botan_mp_t output, msg;

        botan_mp_init(&msg);
        botan_mp_from_bin(msg, in, length);

        botan_mp_init(&output);
        botan_mp_powmod(output, msg, pubkey->e->mp, pubkey->n->mp);

        // Handle any necessary zero padding
        botan_mp_num_bytes(output, &out_bytes);
        botan_mp_num_bytes(pubkey->n->mp, &n_bytes);

        if(n_bytes < out_bytes)
           return 0;

        botan_mp_to_bin(output, out + (n_bytes - out_bytes));

        return n_bytes;
}

/**
   \ingroup Core_Crypto
   \brief Signs data with RSA
   \param out Where to write signature
   \param in Data to sign
   \param length Length of data
   \param seckey RSA secret key
   \param pubkey RSA public key
   \return number of bytes decrypted
*/
int
pgp_rsa_private_encrypt(uint8_t *out,
			const uint8_t *in,
			size_t in_length,
			const pgp_rsa_seckey_t *seckey,
			const pgp_rsa_pubkey_t * pubkey)
{

   botan_privkey_t rsa_key;
   botan_pk_op_sign_t sign_op;
   botan_rng_t rng;
   size_t out_length;

   if(seckey->q == NULL)
   {
      (void) fprintf(stderr, "private key not set in pgp_rsa_private_encrypt\n");
      return 0;
   }

   botan_rng_init(&rng, NULL);

   /* p and q are reversed from normal usage in PGP */
   botan_privkey_load_rsa(&rsa_key, seckey->q->mp, seckey->p->mp, seckey->d->mp);

   if(botan_privkey_check_key(rsa_key, rng, 0) != 0)
   {
      botan_privkey_destroy(rsa_key);
      botan_rng_destroy(rng);
      return 0;
   }

   if(botan_pk_op_sign_create(&sign_op, rsa_key, "Raw", 0) != 0)
   {
      botan_privkey_destroy(rsa_key);
      botan_rng_destroy(rng);
      return 0;
   }

   botan_pk_op_sign_update(sign_op, in, in_length);
   botan_pk_op_sign_finish(sign_op, rng, out, &out_length);

   botan_pk_op_sign_destroy(sign_op);
   botan_privkey_destroy(rsa_key);
   botan_rng_destroy(rng);

   return (int)out_length;
}

/**
\ingroup Core_Crypto
\brief Decrypts RSA-encrypted data
\param out Where to write the plaintext
\param in Encrypted data
\param length Length of encrypted data
\param seckey RSA secret key
\param pubkey RSA public key
\return size of recovered plaintext
*/
int
pgp_rsa_private_decrypt(uint8_t *out,
			const uint8_t *in,
			size_t length,
			const pgp_rsa_seckey_t *seckey,
			const pgp_rsa_pubkey_t *pubkey)
{
   botan_privkey_t rsa_key;
   botan_rng_t rng;
   botan_pk_op_decrypt_t decrypt_op;
   size_t out_len = RNP_BUFSIZ; // in pgp_decrypt_decode_mpi

   botan_privkey_load_rsa(&rsa_key, seckey->q->mp, seckey->p->mp, pubkey->e->mp);

   botan_rng_init(&rng, NULL);
   if(botan_privkey_check_key(rsa_key, rng, 0) != 0)
   {
      botan_rng_destroy(rng);
      botan_privkey_destroy(rsa_key);
      return 0;
   }

   botan_pk_op_decrypt_create(&decrypt_op, rsa_key, "Raw", 0);

   if(botan_pk_op_decrypt(decrypt_op, out, &out_len, (uint8_t*)in, length) != 0)
   {
      out_len = 0;
   }

   botan_rng_destroy(rng);
   botan_privkey_destroy(rsa_key);
   botan_pk_op_decrypt_destroy(decrypt_op);
   return (int)out_len;
}

/**
   \ingroup Core_Crypto
   \brief RSA-encrypts data
   \param out Where to write the encrypted data
   \param in Plaintext
   \param length Size of plaintext
   \param pubkey RSA Public Key
*/
int
pgp_rsa_public_encrypt(uint8_t *out,
			const uint8_t *in,
			size_t length,
			const pgp_rsa_pubkey_t *pubkey)
{

   botan_pubkey_t rsa_key;
   botan_pk_op_encrypt_t enc_op;

   botan_rng_t rng;

   botan_rng_init(&rng, NULL);

   botan_pubkey_load_rsa(&rsa_key, pubkey->n->mp, pubkey->e->mp);

   if (botan_pubkey_check_key(rsa_key, rng, 1) != 0)
   {
      return -1;
   }

   botan_pk_op_encrypt_create(&enc_op, rsa_key, "Raw", 0);

   size_t out_len = RNP_BUFSIZ; // in pgp_rsa_encrypt_mpi
   if(botan_pk_op_encrypt(enc_op, rng, out, &out_len, in, length) != 0)
   {
      return -1;
   }

   botan_pk_op_encrypt_destroy(enc_op);
   botan_pubkey_destroy(rsa_key);
   botan_rng_destroy(rng);

   return (int)out_len;
}


/**
 \ingroup HighLevel_KeyGenerate
 \brief Generates an RSA keypair
 \param numbits Modulus size
 \param e Public Exponent
 \param keydata Pointer to keydata struct to hold new key
 \return 1 if key generated successfully; otherwise 0
 \note It is the caller's responsibility to call pgp_keydata_free(keydata)
*/
static unsigned
rsa_generate_keypair(pgp_key_t *keydata,
      const int numbits,
      const unsigned long e,
      const char *hashalg,
      const char *cipher)
{
  pgp_seckey_t *seckey;
  pgp_output_t *output;
  pgp_memory_t   *mem;
        botan_privkey_t rsa_key;
        botan_rng_t rng;
        botan_mp_t rsa_n, rsa_e, rsa_d, rsa_p, rsa_q, rsa_u;

  pgp_keydata_init(keydata, PGP_PTAG_CT_SECRET_KEY);
  seckey = pgp_get_writable_seckey(keydata);

  /* generate the key pair */

        if(e != 65537)
        {
           fprintf(stderr, "Unexpected RSA e value %zu, key generation failed\n", e);
           return 0;
        }

        if(botan_rng_init(&rng, NULL) != 0)
           return 0;
        if(botan_privkey_create_rsa(&rsa_key, rng, numbits) != 0)
           return 0;

        if(botan_privkey_check_key(rsa_key, rng, 1) != 0)
           return 0;

        botan_rng_destroy(rng);

  /* populate pgp key from ssl key */

  seckey->pubkey.version = PGP_V4;
  seckey->pubkey.birthtime = time(NULL);
  seckey->pubkey.days_valid = 0;
  seckey->pubkey.alg = PGP_PKA_RSA;

        botan_mp_init(&rsa_n);
        botan_mp_init(&rsa_e);
        botan_privkey_rsa_get_n(rsa_n, rsa_key);
        botan_privkey_rsa_get_e(rsa_e, rsa_key);

        // not released by this function
  seckey->pubkey.key.rsa.n = new_BN_take_mp(rsa_n);
  seckey->pubkey.key.rsa.e = new_BN_take_mp(rsa_e);

  seckey->s2k_usage = PGP_S2KU_ENCRYPTED_AND_HASHED;
  seckey->s2k_specifier = PGP_S2KS_SALTED;
  /* seckey->s2k_specifier=PGP_S2KS_SIMPLE; */
  if ((seckey->hash_alg = pgp_str_to_hash_alg(hashalg)) == PGP_HASH_UNKNOWN) {
    seckey->hash_alg = PGP_HASH_SHA1;
  }
  seckey->alg = pgp_str_to_cipher(cipher);
  seckey->octetc = 0;
  seckey->checksum = 0;

        botan_mp_init(&rsa_d);
        botan_mp_init(&rsa_p);
        botan_mp_init(&rsa_q);
        botan_mp_init(&rsa_u);

        botan_privkey_rsa_get_p(rsa_p, rsa_key);
        botan_privkey_rsa_get_q(rsa_q, rsa_key);
        botan_privkey_rsa_get_d(rsa_d, rsa_key);

        if(botan_mp_mod_inverse(rsa_u, rsa_p, rsa_q) != 0 || botan_mp_is_zero(rsa_u))
        {
           fprintf(stderr, "Error computing RSA u param\n");
           return 0;
        }

  seckey->key.rsa.d = new_BN_take_mp(rsa_d);
  seckey->key.rsa.p = new_BN_take_mp(rsa_p);
  seckey->key.rsa.q = new_BN_take_mp(rsa_q);
  seckey->key.rsa.u = new_BN_take_mp(rsa_u);

  pgp_keyid(keydata->sigid, PGP_KEY_ID_SIZE, &keydata->key.seckey.pubkey, seckey->hash_alg);
  pgp_fingerprint(&keydata->sigfingerprint, &keydata->key.seckey.pubkey, seckey->hash_alg);

  /* Generate checksum */

  output = NULL;
  mem = NULL;

  pgp_setup_memory_write(&output, &mem, 128);

  pgp_push_checksum_writer(output, seckey);

  switch (seckey->pubkey.alg) {
  case PGP_PKA_DSA:
    return pgp_write_mpi(output, seckey->key.dsa.x);
  case PGP_PKA_RSA:
  case PGP_PKA_RSA_ENCRYPT_ONLY:
  case PGP_PKA_RSA_SIGN_ONLY:
    if (!pgp_write_mpi(output, seckey->key.rsa.d) ||
        !pgp_write_mpi(output, seckey->key.rsa.p) ||
        !pgp_write_mpi(output, seckey->key.rsa.q) ||
        !pgp_write_mpi(output, seckey->key.rsa.u)) {
      return 0;
    }
    break;
  case PGP_PKA_ELGAMAL:
    return pgp_write_mpi(output, seckey->key.elgamal.x);

  default:
    (void) fprintf(stderr, "Bad seckey->pubkey.alg\n");
    return 0;
  }

  /* close rather than pop, since its the only one on the stack */
  pgp_writer_close(output);
  pgp_teardown_memory_write(output, mem);

  /* should now have checksum in seckey struct */

  return 1;
}

/**
 \ingroup HighLevel_KeyGenerate
 \brief Creates a self-signed RSA keypair
 \param numbits Modulus size
 \param e Public Exponent
 \param userid User ID
 \return The new keypair or NULL

 \note It is the caller's responsibility to call pgp_keydata_free(keydata)
 \sa rsa_generate_keypair()
 \sa pgp_keydata_free()
*/
pgp_key_t  *
pgp_rsa_new_selfsign_key(const int numbits,
        const unsigned long e,
        uint8_t *userid,
        const char *hashalg,
        const char *cipher)
{
  pgp_key_t  *keydata;

  keydata = pgp_keydata_new();
  if (!rsa_generate_keypair(keydata, numbits, e, hashalg, cipher) ||
      !pgp_add_selfsigned_userid(keydata, userid)) {
    pgp_keydata_free(keydata);
    return NULL;
  }
  return keydata;
}

pgp_key_t  *
pgp_rsa_new_key(const int numbits,
    const unsigned long e,
    const char *hashalg,
    const char *cipher)
{
  pgp_key_t  *keydata;

  keydata = pgp_keydata_new();
  if (!rsa_generate_keypair(keydata, numbits, e, hashalg, cipher)) {
    pgp_keydata_free(keydata);
    return NULL;
  }
  return keydata;
}