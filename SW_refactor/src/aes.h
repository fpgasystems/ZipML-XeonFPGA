// Copyright (C) 2018 Kaan Kara - Systems Group, ETH Zurich

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published
// by the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.

// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
//*************************************************************************

#pragma once

#include <wmmintrin.h>
#include <emmintrin.h>
#include <smmintrin.h>

inline void KEY_256_ASSIST_1(__m128i* temp1, __m128i * temp2) { 
    __m128i temp4; 
    *temp2 = _mm_shuffle_epi32(*temp2, 0xff);
    temp4 = _mm_slli_si128 (*temp1, 0x4);  
    *temp1 = _mm_xor_si128 (*temp1, temp4); 
    temp4 = _mm_slli_si128 (temp4, 0x4);  
    *temp1 = _mm_xor_si128 (*temp1, temp4); 
    temp4 = _mm_slli_si128 (temp4, 0x4);  
    *temp1 = _mm_xor_si128 (*temp1, temp4); 
    *temp1 = _mm_xor_si128 (*temp1, *temp2); 
}

inline void KEY_256_ASSIST_2(__m128i* temp1, __m128i * temp3) { 
    __m128i temp2,temp4; 
    temp4 = _mm_aeskeygenassist_si128 (*temp1, 0x0); 
    temp2 = _mm_shuffle_epi32(temp4, 0xaa); 
    temp4 = _mm_slli_si128 (*temp3, 0x4);  
    *temp3 = _mm_xor_si128 (*temp3, temp4); 
    temp4 = _mm_slli_si128 (temp4, 0x4);  
    *temp3 = _mm_xor_si128 (*temp3, temp4); 
    temp4 = _mm_slli_si128 (temp4, 0x4);  
    *temp3 = _mm_xor_si128 (*temp3, temp4); 
    *temp3 = _mm_xor_si128 (*temp3, temp2); 
}

static void AES_256_Key_Expansion (const unsigned char *userkey, 
                            unsigned char *key) { 
    __m128i temp1, temp2, temp3; 
    __m128i *Key_Schedule = (__m128i*)key;

    temp1 = _mm_loadu_si128((__m128i*)userkey); 
    temp3 = _mm_loadu_si128((__m128i*)(userkey+16)); 
    Key_Schedule[0]=temp1; 
    Key_Schedule[1]=temp3; 
    temp2 = _mm_aeskeygenassist_si128 (temp3,0x01); 
    KEY_256_ASSIST_1(&temp1, &temp2); 
    Key_Schedule[2]=temp1; 
    KEY_256_ASSIST_2(&temp1, &temp3); 
    Key_Schedule[3]=temp3; 
    temp2 = _mm_aeskeygenassist_si128 (temp3,0x02); 
    KEY_256_ASSIST_1(&temp1, &temp2); 
    Key_Schedule[4]=temp1; 
    KEY_256_ASSIST_2(&temp1, &temp3); 
    Key_Schedule[5]=temp3; 
    temp2 = _mm_aeskeygenassist_si128 (temp3,0x04); 
    KEY_256_ASSIST_1(&temp1, &temp2); 
    Key_Schedule[6]=temp1; 
    KEY_256_ASSIST_2(&temp1, &temp3); 
    Key_Schedule[7]=temp3;
    temp2 = _mm_aeskeygenassist_si128 (temp3,0x08); 
    KEY_256_ASSIST_1(&temp1, &temp2); 
    Key_Schedule[8]=temp1; 
    KEY_256_ASSIST_2(&temp1, &temp3); 
    Key_Schedule[9]=temp3; 
    temp2 = _mm_aeskeygenassist_si128 (temp3,0x10); 
    KEY_256_ASSIST_1(&temp1, &temp2); 
    Key_Schedule[10]=temp1; 
    KEY_256_ASSIST_2(&temp1, &temp3); 
    Key_Schedule[11]=temp3; 
    temp2 = _mm_aeskeygenassist_si128 (temp3,0x20); 
    KEY_256_ASSIST_1(&temp1, &temp2); 
    Key_Schedule[12]=temp1; 
    KEY_256_ASSIST_2(&temp1, &temp3); 
    Key_Schedule[13]=temp3; 
    temp2 = _mm_aeskeygenassist_si128 (temp3,0x40); 
    KEY_256_ASSIST_1(&temp1, &temp2); 
    Key_Schedule[14]=temp1; 
}

static void AES_256_Decryption_Keys (const unsigned char *key,
                              unsigned char *decryptionkey) {
    __m128i *Key_Schedule = (__m128i*)key;
    __m128i *Key_Schedule_Decrypt = (__m128i*)decryptionkey;

    Key_Schedule_Decrypt[0] = Key_Schedule[14];
    Key_Schedule_Decrypt[14] = Key_Schedule[0];

    for (int i = 1; i < 14; i++) {
        Key_Schedule_Decrypt[i] = _mm_aesimc_si128(((__m128i*)key)[14-i]);
    }
}

static void AES_CBC_encrypt(const unsigned char *in,  
                     unsigned char *out, 
                     unsigned char ivec[16], 
                     unsigned long length, 
                     unsigned char *key, 
                     int number_of_rounds) { 
    __m128i feedback,data; 
    unsigned long i;
    int j; 
    if (length%16) 
        length = length/16+1; 
    else
        length /=16; 
    feedback = _mm_loadu_si128 ((__m128i*)ivec); 
    for(i = 0; i < length; i++) { 
        data = _mm_loadu_si128 (&((__m128i*)in)[i]); 
        feedback = _mm_xor_si128 (data,feedback); 
        feedback = _mm_xor_si128 (feedback,((__m128i*)key)[0]);    
        for(j = 1; j < number_of_rounds; j++) {
            feedback = _mm_aesenc_si128 (feedback,((__m128i*)key)[j]); 
        }
        feedback = _mm_aesenclast_si128 (feedback,((__m128i*)key)[j]); 
        _mm_storeu_si128 (&((__m128i*)out)[i],feedback); 
    }
}

static void AES_CBC_decrypt(const unsigned char *in,  
                     unsigned char *out, 
                     unsigned char ivec[16], 
                     unsigned long length, 
                     unsigned char *key, 
                     int number_of_rounds) { 
    __m128i data,feedback,last_in; 
    unsigned long i;
    int j;
    if (length%16) 
        length = length/16+1; 
    else
        length /=16; 
    feedback = _mm_loadu_si128 ((__m128i*)ivec); 
    for(i = 0; i < length; i++) { 
        last_in = _mm_loadu_si128 (&((__m128i*)in)[i]); 
        data = _mm_xor_si128 (last_in,((__m128i*)key)[0]);    
        for(j = 1; j < number_of_rounds; j++) { 
            data = _mm_aesdec_si128 (data,((__m128i*)key)[j]); 
        } 
        data = _mm_aesdeclast_si128 (data,((__m128i*)key)[j]); 
        data = _mm_xor_si128 (data,feedback); 
        _mm_storeu_si128 (&((__m128i*)out)[i],data); 
        feedback = last_in;
    } 
}

static void AES_CTR_encrypt (const unsigned char *in, 
                      unsigned char *out, 
                      const unsigned char ivec[8], 
                      const unsigned char nonce[4],
                      unsigned long length, 
                      const unsigned char *key, 
                      int number_of_rounds) {
    __m128i ctr_block, tmp, ONE, BSWAP_EPI64; 
    unsigned long i;
    int j;
    if (length%16) 
        length = length/16 + 1; 
    else 
        length/=16;

    ONE = _mm_set_epi32(0,1,0,0); 
    BSWAP_EPI64 = _mm_setr_epi8(7,6,5,4,3,2,1,0,15,14,13,12,11,10,9,8); 
    ctr_block = _mm_setzero_si128(); 
    ctr_block = _mm_insert_epi64(ctr_block, *(long long*)ivec, 1); 
    ctr_block = _mm_insert_epi32(ctr_block, *(long*)nonce, 1);     
    ctr_block = _mm_srli_si128(ctr_block, 4); 
    ctr_block = _mm_shuffle_epi8(ctr_block, BSWAP_EPI64); 
    ctr_block = _mm_add_epi64(ctr_block, ONE); 
    for(i = 0; i < length; i++) { 
        tmp = _mm_shuffle_epi8(ctr_block, BSWAP_EPI64); 
        ctr_block = _mm_add_epi64(ctr_block, ONE); 
        tmp = _mm_xor_si128(tmp, ((__m128i*)key)[0]); 
        for(j = 1; j < number_of_rounds; j++) { 
            tmp = _mm_aesenc_si128 (tmp, ((__m128i*)key)[j]); 
        }
        tmp = _mm_aesenclast_si128 (tmp, ((__m128i*)key)[j]); 
        tmp = _mm_xor_si128(tmp, _mm_loadu_si128(&((__m128i*)in)[i])); 
        _mm_storeu_si128 (&((__m128i*)out)[i], tmp);
    } 
}
