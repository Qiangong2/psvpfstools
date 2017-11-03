#include "PfsKeyGenerator.h"

#include <string>

#include "SceKernelUtilsForDriver.h"
#include "PfsKeys.h"
#include "CryptoEngine.h"
#include "SecretGenerator.h"

int calculate_sha1_chain(unsigned char* dec_key, unsigned char* iv_key, const unsigned char* klicensee, uint32_t unicv_page_salt)
{
   int saltin[2] = {0};
   unsigned char base0[0x14] = {0};
   unsigned char base1[0x14] = {0};
   unsigned char combo[0x28] = {0};
   unsigned char drvkey[0x14] = {0};

   saltin[0] = unicv_page_salt;

   SceKernelUtilsForDriver_sceSha1DigestForDriver(klicensee, 0x10, base0); //calculate hash of klicensee

   // derive key 0

   saltin[1] = 1;
   
   SceKernelUtilsForDriver_sceSha1DigestForDriver((unsigned char*)saltin, 8, base1); //calculate hash of salt 0

   memcpy(combo, base0, 0x14);
   memcpy(combo + 0x14, base1, 0x14);
   
   SceKernelUtilsForDriver_sceSha1DigestForDriver(combo, 0x28, drvkey); //calculate hash from combination of salt 0 hash and klicensee hash

   memcpy(dec_key, drvkey, 0x10);  //copy derived key

   // derive key 1
   
   saltin[1] = 2;

   SceKernelUtilsForDriver_sceSha1DigestForDriver((unsigned char*)saltin, 8, base1); //calculate hash of salt 1

   memcpy(combo, base0, 0x14);
   memcpy(combo + 0x14, base1, 0x14);

   SceKernelUtilsForDriver_sceSha1DigestForDriver(combo, 0x28, drvkey); //calculate hash from combination of salt 1 hash and klicensee hash

   memcpy(iv_key, drvkey, 0x10); //copy derived key

   return 0;
}

int hmac1_sha1_or_sha1_chain(unsigned char* dec_key, unsigned char* iv_key, const unsigned char* klicensee, uint32_t files_salt, uint16_t flag, uint32_t unicv_page_salt)
{
   if((flag & 2) == 0)
   {
      calculate_sha1_chain(dec_key, iv_key, klicensee, unicv_page_salt);
      return 0;
   }

   int saltin0[1] = {0};
   int saltin1[2] = {0};
   unsigned char drvkey[0x14] = {0};

   memcpy(dec_key, klicensee, 0x10);

   if(files_salt == 0)
   {
      saltin0[0x00] = unicv_page_salt;
      SceKernelUtilsForDriver_sceHmacSha1DigestForDriver(hmac_key0, 0x14, (unsigned char*)saltin0, 4, drvkey); // derive key with one salt
   }
   else
   {
      saltin1[0] = files_salt;
      saltin1[1] = unicv_page_salt;
      SceKernelUtilsForDriver_sceHmacSha1DigestForDriver(hmac_key0, 0x14, (unsigned char*)saltin1, 8, drvkey); // derive key with two salts
   }

   memcpy(iv_key, drvkey, 0x10); //copy derived key

   return 0;
}

int hmac_sha1(unsigned char* dec_key, unsigned char* iv_key, const unsigned char* klicensee, uint16_t ignored_flag, uint16_t ignored_key_id, const unsigned char* base_key, uint32_t base_key_len)
{
   unsigned char drvkey[0x14] = {0};

   SceKernelUtilsForDriver_sceHmacSha1DigestForDriver(hmac_key0, 0x14, base_key, base_key_len, drvkey);

   memcpy(dec_key, klicensee, 0x10);

   memcpy(iv_key, drvkey, 0x10);

   return 0;
}

int derive_data_ctx_keys(CryptEngineData* data, const derive_keys_ctx* drv_ctx)
{
   int some_flag_base = (uint32_t)(data->pmi_bcl_flag - 2);
   int some_flag = 0xC0000B03 & (1 << some_flag_base);

   if((some_flag_base > 0x1F) || (some_flag == 0))
   {
      calculate_sha1_chain(data->dec_key, data->iv_key, data->klicensee, data->unicv_page);
      return scePfsUtilGetSecret(data->secret, data->klicensee, data->files_salt, data->pmi_bcl_flag, data->unicv_page, data->key_id);
   }
   else
   {
      if((drv_ctx->unk_40 != 0 && drv_ctx->unk_40 != 3) || (drv_ctx->sceiftbl_version <= 1))
      {    
         hmac1_sha1_or_sha1_chain(data->dec_key, data->iv_key, data->klicensee, data->files_salt, data->pmi_bcl_flag, data->unicv_page);
         return scePfsUtilGetSecret(data->secret, data->klicensee, data->files_salt, data->pmi_bcl_flag, data->unicv_page, data->key_id);
      }
      else
      {
         if(drv_ctx->unk_40 == 0 || drv_ctx->unk_40 == 3)
         {
            hmac_sha1(data->dec_key, data->iv_key, data->klicensee, data->pmi_bcl_flag, data->key_id, drv_ctx->base_key, 0x14);
            return scePfsUtilGetSecret(data->secret, data->klicensee, data->files_salt, data->pmi_bcl_flag, data->unicv_page, data->key_id);
         }
         else
         {
            hmac_sha1(data->dec_key, data->iv_key, data->klicensee, data->pmi_bcl_flag, data->key_id, 0, 0x14);
            return scePfsUtilGetSecret(data->secret, data->klicensee, data->files_salt, data->pmi_bcl_flag, data->unicv_page, data->key_id);
         }
      }
   }
}