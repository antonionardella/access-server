/*
 * This file is part of the IOTA Access distribution
 * (https://github.com/iotaledger/access)
 *
 * Copyright (c) 2020 IOTA Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/****************************************************************************
 * \project IOTA Access
 * \file asn_client.c
 * \brief
 * Client side implemntation for ssl based authentication module
 *
 * @Author Djordje Golubovic
 *
 * \notes
 *
 * \history
 * 05.05.2020. Initial version.
 ****************************************************************************/

#include <stdlib.h>
#include <string.h>

//#define NDEBUG

#include "asn_debug.h"
#include "asn_auth.h"
#include "asn_internal.h"
#include "asn_utils.h"

//////////////////////////////////////////
// Macros and defines
//////////////////////////////////////////

/* AUTH_STAGES */
#define AUTH_ERROR    0
#define AUTH_INIT     1
#define AUTH_GENERATE 2
#define AUTH_VERIFY   3
#define AUTH_FINISH   4
#define AUTH_DONE     5

//////////////////////////////////////////
// Function declarations and definitions
//////////////////////////////////////////

/*
 * Stage 0 - Client initialization
 *
 */

/* AUTH_STAGES */static int authClientInit(asnSession_t *session)
{
   int next_stage = AUTH_ERROR;
   BIGNUM *e = BN_new();
   debug("authClientInit START");

   /* Generate RSA key */
   getInternalRSA_C(session) = RSA_new();
   BN_set_word(e, RSA_F4);
   RSA_generate_key_ex(getInternalRSA_C(session), ASN_RSA_KEY_LEN, e, NULL);

   getInternalDH(session) = DH_new();

   asnUtils_randmem(getInternalVc(session), ASN_V_STRING_LEN);

   getInternalOutP_Count(session) = 1;
   getInternalInP_Count(session) = 1;

   memset(getInternalH(session), 0, RSA_DIGEST_LENGTH);
   memset(getInternalHc(session), 0, RSA_DIGEST_LENGTH);

   next_stage = AUTH_GENERATE;

   debug("authClientInit END");

   return next_stage;
}

/*
 * Stage 1 - DH parameter generation
 *
 * Client generates p, g, Vc and x.
 * Client calculates e = gx mod p.
 * Client sends ( p || g || e || Vc ) to Server.
 *
 */

static inline int authClientGenerateSend(asnSession_t *session)
{
   int ret = ASN_ERROR;
   const BIGNUM *p = NULL, *g = NULL, *pub_key = NULL;

   DH_get0_pqg(getInternalDH(session), &p, NULL, &g);
   DH_get0_key(getInternalDH(session), &pub_key, NULL);

   debug("send p");
   ret = asnUtils_send_message_part_bignum(session, p);
   //TODO: Check return

   debug("send g");
   ret = asnUtils_send_message_part_bignum(session, g);
   //TODO: Check return

   debug("send e");
   ret = asnUtils_send_message_part_bignum(session, pub_key);
   //TODO: Check return

   debug("send Vc");
   ret = asnUtils_send_message_part(session, getInternalVc(session), ASN_V_STRING_LEN);
   //TODO: Check return

   return ret;
}

/* AUTH_STAGES */static int authClientGenerate(asnSession_t *session)
{
   int next_stage = AUTH_ERROR;
   BIGNUM *pp, *gg;
   int ret = ASN_ERROR;

   debug("authClientGenerate START");

   if (NULL != getInternalDH(session)) /* DH parameters generated */
   {
      /* Client generates p, g, Vc and x. */
      gg = BN_new();
      if (gg != NULL)
      {
         BN_set_word(gg, 5);
      }

      pp = BN_get_rfc3526_prime_2048(NULL);
      if (pp == NULL || gg == NULL || !DH_set0_pqg(getInternalDH(session), pp, NULL, gg))
      {
         DH_free(getInternalDH(session));
         BN_free(pp);
         BN_free(gg);
      } else
      {
         /* Client calculates e = gx mod p. */
         int ret_ossl = DH_generate_key(getInternalDH(session));

         if (0 != ret_ossl) /* Keys generated */
         {
            int len = DH_size(getInternalDH(session));
            getInternalK(session) = malloc(len);

            if (NULL != getInternalK(session))
            {
               /* Send ( p || g || e || Vc ) */
               ret = authClientGenerateSend(session);
               //TODO: Check return

               if(ASN_ERROR != ret)
               {
                  next_stage = AUTH_VERIFY;
               }

               next_stage = AUTH_VERIFY;
            }
         }
      }
   }

   debug("authClientGenerate END");

   return next_stage;
}

/*
 * Stage 2 - Compute and verify Server parameters
 *
 * Client receives ( f || s || Ks || Vs )
 * Client computes K = fx mod p
 * Client computes H  = hash( Vc || Vs || Ks || e || f || K )
 * Client verifies the signature s on H
 * Client computes Hc = hash( Vc || Vs || Kc || e || f || K )
 * Client computes signature sc = sign( skc, Hc )
 * Client sends ( Kc || sc ) to Server
 *
 */

static inline int authClientVerifyReceive(asnSession_t *session,
      unsigned char **f, unsigned short *cf_len,
      unsigned char **s, unsigned short *cs_len,
      unsigned char **Ks, unsigned short *Ks_len,
      unsigned char **Vs, unsigned short *Vs_len)
{
   int ret = ASN_ERROR;

   ret = asnUtils_receive_message_part(session, f, cf_len);
   //TODO: Check return

   ret = asnUtils_receive_message_part(session, s, cs_len);
   //TODO: Check return

   ret = asnUtils_receive_message_part(session, Ks, Ks_len);
   //TODO: Check return

   ret = asnUtils_receive_message_part(session, Vs, Vs_len);
   //TODO: Check return

   return ret;
}

static inline int authClientVerifySend(asnSession_t *session,
      unsigned char *Kc, unsigned short Kc_len,
      unsigned char *sc, unsigned short len)
{
   int ret = ASN_ERROR;

   debug("send Kc");
   ret = asnUtils_send_message_part(session, Kc, Kc_len);
   //TODO: Check return

   debug("send sc");
   ret = asnUtils_send_message_part(session, sc, len);
   //TODO: Check return

   return ret;
}

/* AUTH_STAGES */static int authClientVerify(asnSession_t *session)
{
   int next_stage = AUTH_ERROR;
   int ret;

   unsigned char *Kc = NULL, *cKs = NULL, *s = NULL, *cVs = NULL, *cf = NULL;
   unsigned short f_len = 0, cVs_len = 0, s_len = 0, cKs_len = 0;
   const BIGNUM *cpub_key = NULL;
   unsigned int sc_len = 0;
   unsigned char sc[ASN_RSA_SIGN_LEN] = { 0, };
   int Kc_len = 0;

   debug("authClientVerify START");

   // Receive ( f || s || Ks || Vs )
   ret = authClientVerifyReceive(session, &cf, &f_len, &s, &s_len, &cKs, &cKs_len, &cVs, &cVs_len);
   //TODO: Check return

   memcpy(getInternalVs(session), cVs, cVs_len);

   getInternalPubK(session) = BN_bin2bn(cf, f_len, NULL);

   /* Server computes K = fx mod p */
   getInternalK_len(session) = DH_compute_key(getInternalK(session),
         getInternalPubK(session), getInternalDH(session));

   /* Client checks Server key */
   ret = session->f_verify(cKs, cKs_len);
   //TODO: Check return

   DH_get0_key(getInternalDH(session), &cpub_key, NULL);
   /* Client computes H  = hash( Vc || Vs || Ks || e || f || K ) */
   ret = asnUtils_compute_hash(session, getInternalH(session), cKs, cKs_len,
         cpub_key, getInternalPubK(session));

   /* Client verifies the signature s on H */
   getInternalRSA_S(session) = d2i_RSAPublicKey(NULL, (const unsigned char **) &cKs, cKs_len);
   ret = RSA_verify(/*NID_sha256*/ 0, getInternalH(session), RSA_DIGEST_LENGTH, s, s_len, getInternalRSA_S(session));
   debug("!!!!!!!!!!!!!!!!!!!!!!!!!!!RSA_verify (%d)", ret);

   /* Client computes Hc = hash( Vc || Vs || Kc || e || f || K ) */
   Kc_len = i2d_RSAPublicKey(getInternalRSA_C(session), &Kc);
   ret = asnUtils_compute_hash(session, getInternalHc(session), Kc, Kc_len,
         cpub_key, getInternalPubK(session));

   /* Client computes signature sc = sign( skc, Hc ) */
   /*NID_sha256*/
   RSA_sign(/*NID_sha256*/ 0, getInternalHc(session), RSA_DIGEST_LENGTH, sc, &sc_len, getInternalRSA_C(session));
   asnUtils_debug_binary("RSA_Client_sign", sc, sc_len);

   // Send ( Kc || sc )
   ret = authClientVerifySend(session, Kc, Kc_len, sc, sc_len);
   //TODO: Check return

   free(Kc);
   free(s);
   free(cVs);
   free(cf);

   next_stage = AUTH_FINISH;

   debug("authClientVerify END");

   return next_stage;
}

/*
 * Stage 3 - Finish authentication
 *
 * Client generates AES keys.
 * Waits for encryption / decryption tasks :)
 *
 */

/* AUTH_STAGES */static int authClientFinish(asnSession_t *session)
{
   int next_stage = AUTH_ERROR;

   debug("authClientFinish START");

   asnUtils_debug_binary("K", getInternalK(session), getInternalK_len(session));
   asnUtils_debug_binary("H", getInternalH(session), RSA_DIGEST_LENGTH);
   asnUtils_debug_binary("Hc", getInternalHc(session), RSA_DIGEST_LENGTH);

   // Generate AES, HMAC keys
   asnUtils_generate_keys(session);

   getInternalKey_CS(session) = malloc(sizeof(AES_KEY));
   getInternalKey_SC(session) = malloc(sizeof(AES_KEY));

   memset(getInternalKey_CS(session), 0, sizeof(AES_KEY));
   memset(getInternalKey_SC(session), 0, sizeof(AES_KEY));

   // Initialize Client -> Server Encrypt Key
   AES_set_encrypt_key(getInternalEKey_C(session), ASN_AES_KEY_LEN, getInternalKey_CS(session));

   // Initialize Server -> Client Decrypt Key
   AES_set_decrypt_key(getInternalEKey_S(session), ASN_AES_KEY_LEN, getInternalKey_SC(session));

   {
      const BIGNUM *priv_key = NULL;
      unsigned char *Kc = NULL;
      int Kc_len = 0;
      int len = 0;

      DH_get0_key(getInternalDH(session), NULL, &priv_key);

      len = BN_num_bytes(priv_key);

      unsigned char *data = malloc(len);

      len = BN_bn2bin(priv_key, data);

      asnUtils_debug_binary("x", data, len);

      Kc_len = i2d_RSAPrivateKey(getInternalRSA_C(session), &Kc);

      asnUtils_debug_binary("Kc", Kc, Kc_len);
   }

   next_stage = AUTH_DONE;

   debug("authClientFinish END");

   return next_stage;
}

/*
 *
 */

/* ASN_ERRORS */int asnInternal_client_authenticate(asnSession_t *session)
{
   int ret = ASN_ERROR;

   int authStage = AUTH_INIT;

   debug("asnInternal_client_authenticate START");

   while ((AUTH_DONE != authStage) && (AUTH_ERROR != authStage))
   {
      switch (authStage) {
         case AUTH_INIT:
            authStage = authClientInit(session);
            break;
         case AUTH_GENERATE:
            authStage = authClientGenerate(session);
            break;
         case AUTH_VERIFY:
            authStage = authClientVerify(session);
            break;
         case AUTH_FINISH:
            authStage = authClientFinish(session);
            break;
         default:
            break;
      }
   }

   if(AUTH_DONE == authStage)
   {
      ret = ASN_OK;
   }

   debug("asnInternal_client_authenticate END");

   return ret;
}

/* ASN_ERRORS */int asnInternal_client_send(asnSession_t *session, const unsigned char *data, unsigned short  data_len)
{
   return asnUtils_send(session, getInternalIKey_C(session), getInternalKey_CS(session),
         getInternalIV_C(session), data, data_len);
}

/* ASN_ERRORS */int asnInternal_client_receive(asnSession_t *session, unsigned char **data, unsigned short  *data_len)
{
   return asnUtils_receive(session, getInternalIKey_S(session), getInternalKey_SC(session),
         getInternalIV_S(session), data, data_len);
}

void asnInternal_release_client(asnSession_t *session)
{

}


int asnInternal_client_set_option(asnSession_t *session, const char *key, unsigned char *value)
{
	return 0;
}