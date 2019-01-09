/*
 * Copyright(c) 2006 to 2018 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#ifdef DDSI_INCLUDE_ENCRYPTION

#include "ddsi/q_security.h"
#include "ddsi/q_config.h"
#include "ddsi/q_log.h"
#include "ddsi/q_error.h"
#include "os/os_stdlib.h"
#include "os/os_process.h"
#include "os/os_thread.h"
#include "os/os_heap.h"

#include <string.h>       /* for memcpy */
#include <ctype.h>        /* for isspace */
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <openssl/rand.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/err.h>
#include <openssl/bn.h>
#include <openssl/bio.h>

/* Supported URI schema by parser */
#define URI_FILESCHEMA "file://"

/* The max. key-length is defined by crypto-lib, here set to 32 bytes by
 * OpenSSL */
#define Q_MAX_KEY_LENGTH  EVP_MAX_KEY_LENGTH

/* The block-size defines the range of sequence-counter, it varies per
 * cipher */
#define Q_BLOWFISH_BLOCK_SIZE (8)
#define Q_AES_BLOCK_SIZE      (16)
#define Q_BLOCK_SIZE_MAX      (16)

/** The counter length corresponds to the specific blocksize */
#define Q_NULL_COUNTER_SIZE        (0L)
#define Q_BLOWFISH_COUNTER_SIZE    Q_BLOWFISH_BLOCK_SIZE
#define Q_AES_COUNTER_SIZE         Q_AES_BLOCK_SIZE

/* Define macros for 20 bytes digest, but as a single bit-flip shall change
 * half of the digests bits, only 12 bytes of them are transfered in security
 * header (CHECKME, shall we take lower 12 bytes or higher 12 bytes?)
 * Assure the used digest length is 4bytes alligned, to ensure proper alignment of headersize*/
#define Q_DIGEST_LENGTH          (SHA_DIGEST_LENGTH)
#define Q_DIGEST_LENGTH_2        (12)
#define Q_SHA1                   SHA1

/* For future usage, byte-size of unique value to define lower 4 bytes of each
 * counter, shall be chosen randomly */
#define Q_KEYID_LENGTH 4

#define Q_REPORT_OPENSSL_ERR(x) \
while ( ERR_peek_error() ) \
   DDS_ERROR(x "%s", ERR_error_string(ERR_get_error(), NULL));



typedef unsigned char q_sha1Digest[SHA_DIGEST_LENGTH];  /* 20 bytes + '\0' */


C_STRUCT(q_sha1Header) {
    unsigned char hash[Q_DIGEST_LENGTH_2]; /* hash over body and security attributes */
    /* ----- up to here encrypted ------*/
};


/* class declarations */
C_CLASS(q_nullHeader);
C_CLASS(q_blowfishHeader);
C_CLASS(q_aesHeader);


/* structure declarations */
C_STRUCT(q_nullHeader) {
    q_cipherType  cipherType;   /* network order */ /* OPTME */
    os_uint32 partitionId;  /* network order */
};

C_STRUCT(q_blowfishHeader) {
    unsigned char  keyId[Q_KEYID_LENGTH];   /* required for re-keying (reserved for future usage) */
    unsigned char  counter[Q_BLOWFISH_BLOCK_SIZE]; /* cipher block length */
    q_cipherType  cipherType;   /* network order */ /* OPTME */
    os_uint32 partitionId;  /* network order */
};

C_STRUCT(q_aesHeader) {
    unsigned char  keyId[Q_KEYID_LENGTH];   /* required for re-keying (reserved for future usage)*/
    unsigned char  counter[Q_AES_BLOCK_SIZE]; /* cipher block length */
    q_cipherType  cipherType;   /* network order */ /* OPTME */
    os_uint32 partitionId;  /* network order */
};

/* To prevent fragmentations of heap and costly pointer dereferencing with
 * possible lot of cache-misses, we declare a union that allows us to allocate
 * a single array of codecs, each entry realizing a different cipher and
 * header-size.  */
C_STRUCT(q_securityHeader) {
    union {
        C_STRUCT(q_nullHeader)     null; /* obsolete */
        C_STRUCT(q_blowfishHeader) blowfish;
        C_STRUCT(q_aesHeader)      aes;
    } u;
};


/* a number of error states, each codec within the set can be in:
 *
 * Q_CODEC_STATE_OK: everything is Ok, encryption/decryption is performed
 *
 * Q_CODEC_STATE_REQUIRES_REKEYING: same as Q_CODEC_STATE_OK, but codec has
 * reached a state which requires re-keying, shall be used as signal for codec
 * manegement
 *
 * Q_CODEC_STATE_DROP_TEMP: drop encodings/decodings temporarily, caused by
 * faulty cipher-keys read form file. Updating the key-file will solve the
 * problem and release the block.
 *
 * Q_CODEC_STATE_DROP_PERM: drop encodings/decodings permanently, caused by
 * 'un-connected' partitions, or faulty security-profiles, eg. un-known
 * cipher.
*/
typedef enum {
    Q_CODEC_STATE_OK=0,
    Q_CODEC_STATE_REQUIRES_REKEYING=1,
    Q_CODEC_STATE_DROP_TEMP=2,
    Q_CODEC_STATE_DROP_PERM=4
} q_securityCodecState;

/* if not 0, the codec is blocked */
#define IS_DROP_STATE(state) \
   ((state)&(Q_CODEC_STATE_DROP_TEMP|Q_CODEC_STATE_DROP_PERM))

/* declaration of codec */
C_CLASS(q_securityPartitionDecoder);
C_CLASS(q_securityPartitionEncoder);

C_STRUCT(q_securityPartitionDecoder) {
    q_securityCodecState  state;
    char                  *cipherKeyURL;
    q_cipherType          cipherType;
    os_char *             partitionName;
    EVP_CIPHER_CTX        cipherContext;
    /* this codec does hold state and therfor does not require securityHeader
     * attributes */
};


C_STRUCT(q_securityPartitionEncoder){
    q_securityCodecState state;
    char                 *cipherKeyURL;
    q_cipherType         cipherType;
    os_char *            partitionName;
    EVP_CIPHER_CTX       cipherContext;
    /* The current state will be appendend to message and will be used by
     * receiver to decrypt the message in question. To avoid cache misses
     * store the current state close to EVP_CIPHER_CTX  */
    C_STRUCT(q_securityHeader) securityHeader; /* holds the state */
};


C_STRUCT(q_securityDecoderSet) {
    os_uint32 nofPartitions;
    q_securityPartitionDecoder decoders;
};

C_STRUCT(q_securityEncoderSet) {
    os_uint32 nofPartitions;
    os_uint32 headerSizeMax;
    q_securityPartitionEncoder encoders;
};


#if 1
/* no dumping of buffer before and after en-/decrypting */
#define DUMP_BUFFER(partitionName,chan,buffer,length,label,counter,counterLength, bufferLength)
#else
/* Use this line to dump to the ddsi tracefile */
#define DUMP_BUFFER(partitionName,chan,buffer,length,label,counter,counterLength,bufferLenght) tdumpBuffer(partitionName,chan,buffer,length,label,counter,counterLength, bufferLength)
/* Use this line to dump to a file in the /tmp/directory */
#define DUMP_BUFFER(partitionName,chan,buffer,length,label,counter,counterLength,bufferLenght) dumpBuffer(partitionName,chan,buffer,length,label,counter,counterLength, bufferLength)
#endif

/** private operations */

#if 0
static void dumpCounter (FILE *of,  unsigned char* counter, int counterLength)
{
    int i = 0;
    if (counter) {
        for (i=0; i<counterLength; ++i) {
            unsigned int tmpChar = (unsigned int) counter[i]; /* dont know how to tell ::printf the argument is simple char*/
            fprintf (of, "%02x", tmpChar);
        }
    } else {
        fprintf(of, "NULL");
    }
}
#endif

#if 0
static void dumpPayload (FILE *of, unsigned char* buffer, int mesgLength, int bufferLength)
{
    int where = 0;
    unsigned int perLine = 16;
    unsigned int i,j;

    for(i=0; where < bufferLength; ++i) {
        int current = where;

        /* line index */
        fprintf (of," 0x%08x", i);

        /* 16 octets in hex format */
        for (j=0; current<bufferLength && j<perLine; ++j, ++current) {
            unsigned int tmpChar = buffer[current];
            if (current != mesgLength) {
                fprintf (of," %02x", tmpChar);
            } else {
                /* with marker */
                fprintf (of,"#%02x", tmpChar);
            }
        }
        /* padding */
        while(j++<perLine) {
            fprintf (of,"   ");
        }
        /* seperator */
        fprintf (of, "  ");

        current = where;
        /* the same octets as before as plain text, if alpha-num,
         * otherwise if non-printable as a dot */
        for (j=0; current<bufferLength && j<perLine; ++j, ++current) {
            int c = buffer[current];
            c = (isalnum(c) ? c : '.');
            if (current!=mesgLength) {
                fprintf (of," %c", c);
            } else {
                fprintf (of,"#%c", c);
            }
        }

        fprintf (of,"\n");

        where = current;
    }
}
#endif

#if 0
static void dumpBuffer
(
  char* partitionName,
  os_uint32 partitionId,
  unsigned char* buffer,
  int length,
  const char* direction,
  unsigned char* counter,
  os_uint32 counterLength,
  int bufferLength)
{
  char filename[FILENAME_MAX+1];

  (void) partitionName;

  os_sprintf (filename, "/tmp/secure_ddsi-%d.dump", partitionId);

  {
    FILE *of = fopen(filename, "a");

    fprintf (of, "[%s] Buffer HexDump %u  Counter: ", direction, (unsigned int) length);

    dumpCounter(of, counter, counterLength);
    fprintf (of,"\n");

    dumpPayload(of, buffer, length, bufferLength);

    fclose(of);
  }
}
#endif

#if 0
static void tdumpCounter(unsigned char* counter, int counterLength)
{
    int i = 0;
    if (counter) {
        for (i=0; i<counterLength; ++i) {
            unsigned int tmpChar = (unsigned int) counter[i]; /* dont know how to tell ::printf the argument is simple char*/
            DDS_TRACE("%02x", tmpChar);
        }
    } else {
        DDS_TRACE("NULL");
    }
}
#endif

#if 0
static void tdumpPayload (unsigned char* buffer, int mesgLength, int bufferLength)
{
    int where = 0;
    unsigned int perLine = 16;
    unsigned int i,j;

    for(i=0; where < bufferLength; ++i) {
        int current = where;

        /* line index */
        DDS_TRACE(" 0x%08x", i);

        /* 16 octets in hex format */
        for (j=0; current<bufferLength && j<perLine; ++j, ++current) {
            unsigned int tmpChar = buffer[current];
            if (current != mesgLength) {
                DDS_TRACE(" %02x", tmpChar);
            } else {
                /* with marker */
                DDS_TRACE("#%02x", tmpChar);
            }
        }
        /* padding */
        while(j++<perLine) {
            DDS_TRACE("   ");
        }
        /* seperator */
        DDS_TRACE("  ");

        current = where;
        /* the same octets as before as plain text, if alpha-num,
         * otherwise if non-printable as a dot */
        for (j=0; current<bufferLength && j<perLine; ++j, ++current) {
                int c = buffer[current];
                c = (isalnum(c) ? c : '.');
                if (current!=mesgLength) {
                    TRACE((" %c", c));
                } else {
                    TRACE(("#%c", c));
                }
        }

        DDS_TRACE("\n");

        where = current;
    }
}
#endif

#if 0
static void tdumpBuffer
(
  char* partitionName,
  os_uint32 partitionId,
  unsigned char* buffer,
  int length,
  const char* direction,
  unsigned char* counter,
  os_uint32 counterLength,
  int bufferLength
)
{
  (void) partitionName;
  (void) partitionId;

  DDS_TRACE("[%s] Buffer HexDump %u  Counter: ", direction, (unsigned int) length);

  tdumpCounter(counter, counterLength);
  DDS_TRACE("\n");

  tdumpPayload(buffer, length, bufferLength);
}
#endif

/* returns the required space, codec must be non-NULL */

static os_uint32 q_securityEncoderSetHeaderSize (q_securityEncoderSet codec)
{
  assert (codec);
  return codec->headerSizeMax;
}


static c_bool decoderIsBlocked (q_securityPartitionDecoder codec)
{
  return (IS_DROP_STATE(codec->state) > 0);
}

static c_bool encoderIsBlocked (q_securityPartitionEncoder codec)
{
  return (IS_DROP_STATE(codec->state) > 0);
}

/* returns -1 on error */
static short hex2bin(char hexChar)
{
    switch (hexChar) {
        case '0': return 0;
        case '1': return 1;
        case '2': return 2;
        case '3': return 3;
        case '4': return 4;
        case '5': return 5;
        case '6': return 6;
        case '7': return 7;
        case '8': return 8;
        case '9': return 9;

        case 'a': return 10;
        case 'b': return 11;
        case 'c': return 12;
        case 'd': return 13;
        case 'e': return 14;
        case 'f': return 15;

        case 'A': return 10;
        case 'B': return 11;
        case 'C': return 12;
        case 'D': return 13;
        case 'E': return 14;
        case 'F': return 15;
        default:
            return -1; /* error */
    }
}

/* returns NULL on error, eg bad hex-string */
static c_bool hex2key
(
  const char* hexKey,
  os_uint32 expectedLength,
  unsigned char *result /* out */ )
{
    size_t i, len=0;
    short val;

    len = strlen(hexKey);

    for (i=0; i<len/2 && i<expectedLength; ++i) {
        short high = hex2bin(hexKey[i*2]);
        short low  = hex2bin(hexKey[(i*2)+1]);
        if (high < 0 || low < 0) {
            /* error, bad hex-string */
            return FALSE;
        }

        val = (short) (high << 4 | low);
        result[i] = (unsigned char) val;
    }

    /* hexString too short or too long */
    if (i!=expectedLength || len/2 != expectedLength) {
        return FALSE;
    }

    return TRUE;
}


/* return the key-length this cipher-type requires */
static
os_uint32
q_securityCipherKeyLength(q_cipherType cipherType) {
    switch (cipherType) {
        case Q_CIPHER_UNDEFINED: return 0;
        case Q_CIPHER_NULL:      return 0;
        case Q_CIPHER_NONE:      return 0;
        case Q_CIPHER_BLOWFISH:  return (os_uint32)EVP_CIPHER_key_length(EVP_bf_ecb()); /* 16 */
        case Q_CIPHER_AES128:    return (os_uint32)EVP_CIPHER_key_length(EVP_aes_128_ecb()); /* 16 */
        case Q_CIPHER_AES192:    return (os_uint32)EVP_CIPHER_key_length(EVP_aes_192_ecb()); /* 24 */
        case Q_CIPHER_AES256:    return (os_uint32)EVP_CIPHER_key_length(EVP_aes_256_ecb()); /* 32 */

        default:
            assert(0 && "never reach");
            return 0;
    }
}

/* return the const char-pointer  */
static char * cipherTypeAsString (q_cipherType cipherType)
{
    switch (cipherType) {
        case Q_CIPHER_UNDEFINED: return "undefined";
        case Q_CIPHER_NULL:      return "NULL";
        case Q_CIPHER_NONE:      return "None";
        case Q_CIPHER_BLOWFISH:  return "Blowfish-SHA1";
        case Q_CIPHER_AES128:    return "AES128-SHA1";
        case Q_CIPHER_AES192:    return "AES192-SHA1";
        case Q_CIPHER_AES256:    return "AES256-SHA1";
        default:
            assert(0 && "never reach");
            return "undefined";
    }
}

/* return the const char-pointer  */
static char*
stateAsString(q_securityCodecState state) {
    if (state&(Q_CODEC_STATE_DROP_PERM)) return "drop-permanently";
    else if (state&(Q_CODEC_STATE_DROP_TEMP)) return "drop-temporary";
    else if (state > 0) return "unknown";
    else return "ok";
}

/* this function is based on original code of
 * components/configuration/parser/code/cfg_parser.y */

static c_bool q_securityResolveCipherKeyFromUri
(
  const char *uriStr,
  os_uint32 expectedLength,
  unsigned char *cipherKey /* out buffer */
)
{
    char *filename;
    FILE *file = NULL;
    char  readBuffer[256]; /*at most strings of 255 chars */
    char *hexStr = NULL;
    int ret;
    c_bool result = FALSE;

    if ((uriStr != NULL) &&
        (strncmp(uriStr, URI_FILESCHEMA, strlen(URI_FILESCHEMA)) == 0)) {

    /* TBD: compare file-permissions with uid/gid of this process, the
     * file should be protected against read/write by others, otherwise we
     * should refuse to read from it */
        const char *justPath =
                (char *)(uriStr + strlen(URI_FILESCHEMA));

        filename = os_strdup (justPath);
        file = fopen(filename, "r");
        if (file) {
            /* read at most 255 chars from file, this should suffice if the
             * secret key has atmost 32 chars  */
            ret = fscanf (file, "%255s", readBuffer);

            if (ret != EOF)
            {
              /* skip leading white spaces */
              for (hexStr=readBuffer;
                   isspace((unsigned char) *hexStr);
                   ++hexStr);

              result = hex2key(hexStr, expectedLength, cipherKey);
            }

            fclose(file);
        } else {
            DDS_ERROR("q_securityResolveCipherKeyFromUri: Could not open %s",uriStr);
        }

        os_free(filename);

    } else if (uriStr != NULL) {
        /* seems to be a hex string */
        result = hex2key(uriStr, expectedLength, cipherKey);
    }

    return result;
}


/* Validate the cipherkey, parsing the hex-string directly or the content of
 * file */
static c_bool q_securityIsValidCipherKeyUri
(
  q_cipherType cipherType,
  const char* cipherKeyUri
)
{
  unsigned char tmpCipherKey[Q_MAX_KEY_LENGTH]; /* transient */
  os_uint32 expectedLength = q_securityCipherKeyLength(cipherType);

  assert(expectedLength > 0);

  /* we are not interested in the key, jsut doing the syntax check */
  return q_securityResolveCipherKeyFromUri(cipherKeyUri, expectedLength, tmpCipherKey);
}

/* compare cipherName to known identifiers, comparison is case-insensitive  */
static c_bool q_securityCipherTypeFromString(const char* cipherName,
                                q_cipherType *cipherType) /* out */
{
    if (cipherName == NULL)
    {
        DDS_ERROR("q_securityCipherTypeFromString:internal error, empty cipher string");
        *cipherType = Q_CIPHER_UNDEFINED;
        return FALSE;
    }

    if (os_strcasecmp(cipherName, "null") == 0) {
        *cipherType = Q_CIPHER_NULL;
    } else if (os_strcasecmp(cipherName, "blowfish") == 0 ||
               os_strcasecmp(cipherName, "blowfish-sha1") == 0) {
        *cipherType = Q_CIPHER_BLOWFISH;
    } else if (os_strcasecmp(cipherName, "aes128") == 0 ||
               os_strcasecmp(cipherName, "aes128-sha1") == 0) {
        *cipherType = Q_CIPHER_AES128;
    } else if (os_strcasecmp(cipherName, "aes192") == 0 ||
               os_strcasecmp(cipherName, "aes192-sha1") == 0) {
        *cipherType = Q_CIPHER_AES192;
    } else if (os_strcasecmp(cipherName, "aes256") == 0 ||
              os_strcasecmp(cipherName, "aes256-sha1") == 0) {
        *cipherType = Q_CIPHER_AES256;
#if 0
    } else if (os_strcasecmp(cipherName, "rsa-null") == 0) {
        *cipherType = Q_CIPHER_RSA_WITH_NULL;
    } else if (os_strcasecmp(cipherName, "rsa-blowfish") == 0 ||
               os_strcasecmp(cipherName, "rsa-blowfish-sha1") == 0) {
        *cipherType = Q_CIPHER_RSA_WITH_BLOWFISH;
    } else if (os_strcasecmp(cipherName, "rsa-aes128") == 0 ||
               os_strcasecmp(cipherName, "rsa-aes128-sha1") == 0) {
        *cipherType = Q_CIPHER_RSA_WITH_AES128;
    } else if (os_strcasecmp(cipherName, "rsa-aes192") == 0 ||
               os_strcasecmp(cipherName, "rsa-aes192-sha1") == 0) {
        *cipherType = Q_CIPHER_RSA_WITH_AES192;
    } else if (os_strcasecmp(cipherName, "rsa-aes256") == 0 ||
               os_strcasecmp(cipherName, "rsa-aes256-sha1") == 0) {
        *cipherType = Q_CIPHER_RSA_WITH_AES256;
#endif
    } else {
        *cipherType = Q_CIPHER_UNDEFINED;
        return FALSE;
    }
    return TRUE;
}

static os_uint32 cipherTypeToHeaderSize(q_cipherType cipherType) {
    switch (cipherType) {
        case Q_CIPHER_UNDEFINED:
        case Q_CIPHER_NONE:
            return 0;
        case Q_CIPHER_NULL:
            return sizeof(C_STRUCT(q_nullHeader));

        case Q_CIPHER_BLOWFISH:
            return sizeof(C_STRUCT(q_sha1Header)) +
                   sizeof(C_STRUCT(q_blowfishHeader));

        case Q_CIPHER_AES128:
        case Q_CIPHER_AES192:
        case Q_CIPHER_AES256:
            return sizeof(C_STRUCT(q_sha1Header)) +
                   sizeof(C_STRUCT(q_aesHeader));

        default:
            assert(0 && "unsupported cipher");
    }

    assert(FALSE);
    return 0;
}

/*these two methods are not static for the moment because of tests*/

static
void q_securityRNGSeed (void)
{
        os_time time=os_timeGetMonotonic();
        RAND_seed((const void *)&time.tv_nsec,sizeof(time.tv_nsec));
}

static
void q_securityRNGGetRandomNumber(int number_length,unsigned char * randNumber)
{
    RAND_bytes(randNumber,number_length);
}


static
c_bool
q_securityPartitionEncoderInit(q_securityPartitionEncoder encoder,struct config_networkpartition_listelem *p)
{
    unsigned char  cipherKey[Q_MAX_KEY_LENGTH];
    char      *cipherKeyURL = p->securityProfile?p->securityProfile->key:NULL;
    os_char * partitionName = p->name;
    c_bool        connected = (c_bool) p->connected;
    q_cipherType cipherType = p->securityProfile?p->securityProfile->cipher: Q_CIPHER_NONE;
    os_uint32          hash = p->partitionHash;
    os_uint32   partitionId = p->partitionId;

    /* init */
    memset(encoder, 0, sizeof(*encoder));
    memset(cipherKey, 0, sizeof(*cipherKey));


    if (!connected) {
        DDS_TRACE("Network Partition '%s' (%d) not connected, dropping outbound  traffic permanently", partitionName, partitionId);

        encoder->state = Q_CODEC_STATE_DROP_PERM;
        encoder->cipherType = Q_CIPHER_UNDEFINED;
        encoder->partitionName = partitionName;

        return TRUE;
    }

    assert(cipherType != Q_CIPHER_UNDEFINED);

    {
        /* init the cipher */

        const os_uint32 partitionHashNetworkOrder =  htonl(hash);
        const q_cipherType  cipherTypeNetworkOrder  = htonl(cipherType);
        const unsigned char *iv = NULL;  /* ignored by ECBs ciphers */
        const EVP_CIPHER *cipher = NULL;
        const os_uint32 cipherKeyLength = q_securityCipherKeyLength(cipherType);
        unsigned char randCounter[Q_KEYID_LENGTH];

        /*TRACE(("Security Encoder init:  partition '%s' (%d) (connected), cipherType %d, cipherKey %s\n",
                   partitionName,partitionId, cipherType, cipherKeyURL));*/

        encoder->state = Q_CODEC_STATE_OK;
        encoder->cipherKeyURL = cipherKeyURL;  /* const, required for re-keying */
        encoder->cipherType = cipherType;
        encoder->partitionName = partitionName; /* const */

        q_securityRNGGetRandomNumber(Q_KEYID_LENGTH, randCounter);

        switch (cipherType) {
            case Q_CIPHER_NULL:
            case Q_CIPHER_NONE:
                cipher = EVP_enc_null();
                encoder->securityHeader.u.null.cipherType = cipherTypeNetworkOrder;
                encoder->securityHeader.u.null.partitionId = partitionHashNetworkOrder;
                break;

            case Q_CIPHER_BLOWFISH:
                cipher = EVP_bf_ecb();
                assert(8 == EVP_CIPHER_block_size(cipher));
                assert(16 == cipherKeyLength);

                encoder->securityHeader.u.blowfish.cipherType = cipherTypeNetworkOrder;
                encoder->securityHeader.u.blowfish.partitionId = partitionHashNetworkOrder;

                memcpy(encoder->securityHeader.u.blowfish.counter,&randCounter, sizeof(randCounter));
                break;

            case Q_CIPHER_AES128:
                cipher = EVP_aes_128_ecb();
                assert(16 == EVP_CIPHER_block_size(cipher));
                assert(16 == cipherKeyLength);

                encoder->securityHeader.u.aes.cipherType = cipherTypeNetworkOrder;
                encoder->securityHeader.u.aes.partitionId = partitionHashNetworkOrder;

                memcpy(encoder->securityHeader.u.aes.counter, &randCounter, sizeof(randCounter));
                break;

            case Q_CIPHER_AES192:
                cipher = EVP_aes_192_ecb();

                assert(16 == EVP_CIPHER_block_size(cipher));
                assert(24 == cipherKeyLength);

                encoder->securityHeader.u.aes.cipherType = cipherTypeNetworkOrder;
                encoder->securityHeader.u.aes.partitionId = partitionHashNetworkOrder;

                memcpy(encoder->securityHeader.u.aes.counter, &randCounter, sizeof(randCounter));
                break;

            case Q_CIPHER_AES256:
                cipher = EVP_aes_256_ecb();

                assert(16 == EVP_CIPHER_block_size(cipher));
                assert(32 == cipherKeyLength);

                encoder->securityHeader.u.aes.cipherType = cipherTypeNetworkOrder;
                encoder->securityHeader.u.aes.partitionId = partitionHashNetworkOrder;

                memcpy(encoder->securityHeader.u.aes.counter, &randCounter, sizeof(randCounter));
                break;

            default:
                assert(0 && "never reach");
        }

        /* intitialize the key-buffer */
        if (cipherType != Q_CIPHER_NULL && cipherType != Q_CIPHER_NONE &&
            !q_securityResolveCipherKeyFromUri(cipherKeyURL,cipherKeyLength,cipherKey)) {
            DDS_ERROR("DDSI Security Encoder: dropping traffic of partition '%s' (%d) due to invalid cipher key",
                              encoder->partitionName, partitionId);
            encoder->state = Q_CODEC_STATE_DROP_TEMP;
        }

        EVP_CIPHER_CTX_init(&(encoder->cipherContext));

        EVP_EncryptInit_ex(&(encoder->cipherContext),
                           cipher,
                           NULL,
                           cipherKey,
                           iv); /* IV is ignored by ECB ciphers */
    }

    return TRUE;
}


static
c_bool
q_securityPartitionEncoderFini(q_securityPartitionEncoder encoder)
{
    if (encoder->cipherType != Q_CIPHER_UNDEFINED) {
        /* release the cipher */
        EVP_CIPHER_CTX *ctx = NULL;

        ctx = &(encoder->cipherContext);

        EVP_CIPHER_CTX_cleanup(ctx);
    }

    return TRUE;
}

static
c_bool
q_securityPartitionDecoderInit(q_securityPartitionDecoder decoder,struct config_networkpartition_listelem *p)
{
    unsigned char  cipherKey[Q_MAX_KEY_LENGTH];
    char *cipherKeyURL = p->securityProfile?p->securityProfile->key:NULL;
    os_char * partitionName = p->name;
    c_bool        connected = (c_bool) p->connected;
    q_cipherType cipherType = p->securityProfile?p->securityProfile->cipher: Q_CIPHER_NONE;
    os_uint32   partitionId = p->partitionId;


    /* init */
    memset(decoder, 0, sizeof(*decoder));
    memset(cipherKey, 0, sizeof(cipherKey));

    if (!connected) {
        DDS_TRACE("Network Partition '%s' (%d) not connected, dropping inbound traffic permanently\n", partitionName, partitionId);

        decoder->state = Q_CODEC_STATE_DROP_PERM;
        decoder->cipherType = Q_CIPHER_UNDEFINED;
        decoder->partitionName = partitionName;
        return TRUE;
    }


    assert(cipherType != Q_CIPHER_UNDEFINED);

    {
        /* init the cipher */
        const unsigned char *iv = NULL;  /* ignored by ECBs ciphers */
        const EVP_CIPHER *cipher = NULL;
        const os_uint32  cipherKeyLength = q_securityCipherKeyLength(cipherType);

        /*TRACE(("Security Decoder init:  partition '%s' (%d) (connected), cipherType %d, cipherKey %s \n",
                   partitionName,partitionId, cipherType, cipherKeyURL));*/

        decoder->state = Q_CODEC_STATE_OK;
        decoder->cipherKeyURL = cipherKeyURL; /* const, required for re-keying */
        decoder->cipherType = cipherType;
        decoder->partitionName = partitionName; /* const */

        switch (cipherType) {
            case Q_CIPHER_NULL:
            case Q_CIPHER_NONE:
                cipher = EVP_enc_null();
                break;

            case Q_CIPHER_BLOWFISH:
                cipher = EVP_bf_ecb();

                assert(8 == EVP_CIPHER_block_size(cipher));
                assert(16 == cipherKeyLength);
                break;

            case Q_CIPHER_AES128:
                cipher = EVP_aes_128_ecb();

                assert(16 == EVP_CIPHER_block_size(cipher));
                assert(16 == cipherKeyLength);
                break;

            case Q_CIPHER_AES192:
                cipher = EVP_aes_192_ecb();

                assert(16 == EVP_CIPHER_block_size(cipher));
                assert(24 == cipherKeyLength);
                break;

            case Q_CIPHER_AES256:
                cipher = EVP_aes_256_ecb();

                assert(16 == EVP_CIPHER_block_size(cipher));
                assert(32 == cipherKeyLength);
                break;

            default:
                assert(0 && "never reach");
        }

        /* init key-buffer from URL */
        if (cipherType != Q_CIPHER_NULL && cipherType != Q_CIPHER_NONE &&
            !q_securityResolveCipherKeyFromUri(cipherKeyURL,cipherKeyLength,cipherKey)) {
            DDS_ERROR("DDSI Security Decoder: dropping traffic of partition '%s' (%d) due to invalid cipher key",
                              decoder->partitionName, partitionId);
            /* can be solved by re-keying, rewriting the file */
            decoder->state = Q_CODEC_STATE_DROP_TEMP;
        }

        EVP_CIPHER_CTX_init(&(decoder->cipherContext));

        EVP_EncryptInit_ex(&(decoder->cipherContext),
                           cipher,
                           NULL,
                           cipherKey,
                           iv); /* IV is ignored by ECB ciphers */

    }

    return TRUE;
}


static c_bool q_securityPartitionDecoderFini (q_securityPartitionDecoder decoder)
{
    if (decoder->cipherType != Q_CIPHER_UNDEFINED) {
        /* release the cipher */
        EVP_CIPHER_CTX *ctx = NULL;

        ctx = &(decoder->cipherContext);

        EVP_CIPHER_CTX_cleanup(ctx);
    }

    return 1; /* true */
}

/* returns NULL on error */

static q_securityEncoderSet q_securityEncoderSetNew (void)
{
    const os_uint32 nofPartitions = config.nof_networkPartitions;

    q_securityEncoderSet result =
        os_malloc(sizeof(C_STRUCT(q_securityEncoderSet)));

    if (!result) {
        return NULL;
    }

    result->nofPartitions = 0; /* init */
    result->headerSizeMax = 0;
    if (nofPartitions == 0) {
        result->encoders = NULL;
    } else {
        result->encoders = os_malloc(sizeof(C_STRUCT(q_securityPartitionEncoder)) * nofPartitions);
        memset(result->encoders,
               0,
               sizeof(C_STRUCT(q_securityPartitionEncoder)) *
               nofPartitions);
    }

    /* if not done yet, init the RNG within this thread*/
    if(!RAND_status()) {
        q_securityRNGSeed();
    }

    /* init each codec per network parition */
    {
        q_securityPartitionEncoder currentEncoder = NULL;
        os_uint32 headerSizeProfile = 0;

        struct config_networkpartition_listelem *p = config.networkPartitions;

        while (p) {

            currentEncoder =
                &(result->encoders[p->partitionId-1]);

            if (p->securityProfile) {
                if (!q_securityPartitionEncoderInit(currentEncoder,p)) {
                    /* the codec config is faulty, the codec has been set into
                    * DROP_TERMP or DROP_PERM state, depending on the kind of
                    * fault. Continue to intitialize remaining codecs */
                    DDS_ERROR("q_securityEncoderSet:failed to initialize codec of partition '%s' (%d)\n",
                                    p->name,p->partitionId );
                }

                DDS_TRACE("Network Partition '%s' (%d) encoder secured by %s cipher, in status '%s'\n",
                          p->name,
                          p->partitionId,
                          cipherTypeAsString(currentEncoder->cipherType),
                          stateAsString(currentEncoder->state));

                headerSizeProfile = cipherTypeToHeaderSize(currentEncoder->cipherType);
                result->headerSizeMax = (headerSizeProfile > (result->headerSizeMax)) ? headerSizeProfile: (result->headerSizeMax);
                result->headerSizeMax = (result->headerSizeMax + 4u) & ~4u; /* enforce mutiple of 4 */
            } else {
                memset(currentEncoder, 0, sizeof(*currentEncoder));
                currentEncoder->state = Q_CODEC_STATE_DROP_PERM;
                currentEncoder->cipherType = Q_CIPHER_NONE;
                currentEncoder->partitionName = p->name;

                DDS_TRACE("Network Partition '%s' (%d) is not secured by a cipher\n",
                          p->name,
                          p->partitionId);
            }
            /* count up step by step, so in case of error
             * q_securityEncoderSetFree will only iterate those already
             * intialized */
            ++(result->nofPartitions);
            p = p->next;

        }


        DDS_TRACE("reserving %d bytes for security header\n", result->headerSizeMax);
    }

    return result;
}

/* returns NULL on error */

static q_securityDecoderSet q_securityDecoderSetNew (void)
{
    q_securityDecoderSet result;
    const os_uint32 nofPartitions = config.nof_networkPartitions;

    if (nofPartitions == 0)
    {
      return NULL;
    }

    result = os_malloc (sizeof(C_STRUCT(q_securityDecoderSet)));
    result->nofPartitions = 0;

    result->decoders =
        os_malloc(sizeof(C_STRUCT(q_securityPartitionDecoder)) * nofPartitions);

    /* init the memory region */
    memset(result->decoders,
           0,
           sizeof(C_STRUCT(q_securityPartitionDecoder)) *
           nofPartitions);

    /* if not done yet, init the RNG within this thread*/
    if(!RAND_status()) {
        q_securityRNGSeed();
    }

    /* init codec per network partition */
    {
        q_securityPartitionDecoder currentDecoder = NULL;

        struct config_networkpartition_listelem *p = config.networkPartitions;

        while (p) {
            currentDecoder =
                &(result->decoders[p->partitionId-1]);
            if ( p->securityProfile ) {
            if (!q_securityPartitionDecoderInit(currentDecoder,p)) {
                /* the codec config is faulty, the codec has been set into
                 * DROP_TERMP or DROP_PERM state, depending on the kind of
                 * fault. Continue to intitialize remaining codecs */
                DDS_ERROR("q_securityDecoderSet:failed to initialize codec of partition '%s' (%d)\n",
                        p->name,p->partitionId);
            }

            DDS_TRACE("Network Partition '%s' (%d) decoder secured by %s cipher, in status '%s'\n",
                     p->name,
                     p->partitionId,
                     cipherTypeAsString(currentDecoder->cipherType),
                     stateAsString(currentDecoder->state));
            } else {
                memset(currentDecoder, 0, sizeof(*currentDecoder));
                currentDecoder->state = Q_CODEC_STATE_DROP_PERM;
                currentDecoder->cipherType = Q_CIPHER_NONE;
                currentDecoder->partitionName = p->name;

                DDS_TRACE("Network Partition '%s' (%d) is not secured by a cipher\n",
                        p->name,
                        p->partitionId);
            }
            /* count up step by step, so in case of error
             * q_securityEncoderSetFree will only iterate those already
             * intialized */
            ++(result->nofPartitions);
            p = p->next;
        }
    }

    return result;

}

static c_bool q_securityEncoderSetFree (q_securityEncoderSet codec)
{
    q_securityPartitionEncoder currentEncoder = NULL;
    os_uint32 ix;

    if (!codec) {
        /* parameter is NULL */
        return TRUE;
    }

    for (ix=0; ix<codec->nofPartitions; ++ix) {
        currentEncoder =
            &(codec->encoders[ix]);

        q_securityPartitionEncoderFini(currentEncoder);
    }
    os_free(codec->encoders);
    os_free(codec);

    return 1; /* true */
}

static c_bool q_securityDecoderSetFree (q_securityDecoderSet codec)
{
    q_securityPartitionDecoder currentDecoder = NULL;
    os_uint32 ix;

    if (!codec) {
        /* parameter is NULL */
        return TRUE;
    }

    for (ix=0; ix<codec->nofPartitions; ++ix) {
        currentDecoder =
            &(codec->decoders[ix]);

        q_securityPartitionDecoderFini(currentDecoder);
    }
    os_free(codec->decoders);
    os_free(codec);

    return TRUE; /* true */
}

static os_uint32 q_securityEncoderHeaderSize (q_securityEncoderSet codec, os_uint32 partitionId)
{
    assert(partitionId > 0);
    if (!codec) {
        /* security not initialized or disabled  */
        return 0;
    }
    assert(partitionId <= codec->nofPartitions);
    return cipherTypeToHeaderSize(codec->encoders[partitionId-1].cipherType);
}

static q_cipherType q_securityEncoderCipherType (q_securityEncoderSet codec, os_uint32 partitionId)
{
    assert(partitionId > 0);
    if (!codec) {
        /* security not initialized or disabled  */
        return 0;
    }
    assert(partitionId <= codec->nofPartitions);
    return codec->encoders[partitionId-1].cipherType;
}



/* returns 0 on error, otherwise 1, */
static c_bool counterEncryptOrDecryptInPlace
(
  EVP_CIPHER_CTX *ctx,
  unsigned char *counter, /* in/out */
  unsigned char *buffer,
  int length
)
{
    int i, j, num;
    int where = 0;
    int bl = EVP_CIPHER_CTX_block_size(ctx);
    unsigned char  keyStream[Q_BLOCK_SIZE_MAX];

    /* <= is correct, so that we handle any possible non-aligned data */
    for (i = 0; i <= length / bl && where < length; ++i) {
        /* encrypt the current counter */
        if (!EVP_EncryptUpdate(ctx, keyStream, &num, counter, bl)) { /* ECB encrypts exactly 'bl' bytes */

            DDS_WARNING("Incoming encrypted sub-message dropped: Decrypt failed (bufferLength %u, blockSize %u, where %u)\n",length, bl, where);

            return FALSE;
        }

        /* use the keystream to encrypt a single block of buffer */
        {
            if ( ((int) (length - where)) < bl) {
                /* non aligned data, encrypt remaining block-fragment only */
                for (j = 0; j < ((int) (length - where)); ++j) {
                    buffer[where+j] ^= keyStream[j];
                }
            } else {
                /* default case, encrypt full block */
                for (j = 0; j < bl; ++j) {
                    buffer[where+j] ^= keyStream[j];
                }
            }
        }

        /* increment the counter, remember it's an array of single characters */
        for (j = Q_KEYID_LENGTH; j < bl; ++j) { /*the four first bytes=random value. It is kept unchanged */
            if (++(counter[j]))
                break;
        }

        where += num;
    }

    return TRUE;
}


static void
attachHeaderAndDoSha1(unsigned char* data, os_uint32 dataLength,
                      const void *symCipherHeader, os_uint32 symCipherHeaderLength)
{
    const os_uint32 sha1HeaderLength = sizeof(C_STRUCT(q_sha1Header));
    const os_uint32 overallLength = dataLength +
                                    sha1HeaderLength +
                                    symCipherHeaderLength;

    unsigned char md[Q_DIGEST_LENGTH];

    /* put the fixed attributes into buffer to calculate the digest */
    void *digStart = &(data[dataLength]);
    void *cipStart = &(data[dataLength + sha1HeaderLength]);

    memset(digStart, 0, sha1HeaderLength); /* zero out */
    memcpy(cipStart, symCipherHeader, symCipherHeaderLength);

    /* calculate over complete send buffer */
    Q_SHA1(data, overallLength, md);

    /* Finally place the (half of) digest */
    memcpy(digStart, md, Q_DIGEST_LENGTH_2);
}

static void
attachHeader(unsigned char* data, os_uint32 dataLength,
         const void *symCipherHeader, os_uint32 symCipherHeaderLength)
{
    /* pur the fixed attributes into buffer to calculate the digest */
    void *cipStart = &(data[dataLength]);
    memcpy(cipStart, symCipherHeader, symCipherHeaderLength);
}

static c_bool
verifySha1(unsigned char* data, os_uint32 dataLength, void *digStart)
{
    const os_uint32 sha1HeaderLength = sizeof(C_STRUCT(q_sha1Header));
    C_STRUCT(q_sha1Header) sha1Header;
    unsigned char md[Q_DIGEST_LENGTH];

    /* backup the sha1 digest */
    memcpy(&sha1Header, digStart, sha1HeaderLength);

    /* zero out the bytes in buffer */
    memset(digStart, 0, sha1HeaderLength);

    /* verify digest */
    Q_SHA1(data, dataLength, md);

    return !memcmp(md, sha1Header.hash, Q_DIGEST_LENGTH_2);
}

static c_bool q_securityEncodeInPlace_Generic
(
  q_securityPartitionEncoder encoder,
  os_uint32 partitionId, /*  debugging  */
  unsigned char *buffer,
  os_uint32 *dataLength, /* in/out */
  os_uint32 bufferLength /* for debug */
)
{
  const os_uint32 overallHeaderSize = cipherTypeToHeaderSize(encoder->cipherType);

  EVP_CIPHER_CTX    *ctx   = &(encoder->cipherContext);
  unsigned char     *plainText = buffer;
  os_uint32          plainTextLength = *dataLength;
  c_bool result = TRUE;


  TRACE((":ENCRYPT:'%s'(%d)",encoder->partitionName, partitionId));

  (void) bufferLength;

  switch (encoder->cipherType) {
        case Q_CIPHER_NULL: {
            const os_uint32 symCipherHeaderLength = sizeof(C_STRUCT(q_nullHeader));

            q_nullHeader symCipherHeader = &((encoder->securityHeader).u.null);

            DUMP_BUFFER(encoder->partitionName, partitionId, buffer, *dataLength, "encode->",
                        NULL, Q_NULL_COUNTER_SIZE, bufferLength);

            attachHeader(plainText, plainTextLength,
                         symCipherHeader, symCipherHeaderLength);

            TRACE((":NULL:%s", result?"OK":"ERROR")); /* debug */

            DUMP_BUFFER(encoder->partitionName, partitionId, buffer, *dataLength+overallHeaderSize, "<-encode", NULL,
                        Q_NULL_COUNTER_SIZE, bufferLength);
        }
        break;

        case Q_CIPHER_BLOWFISH: {
            const os_uint32    symCipherHeaderLength = sizeof(C_STRUCT(q_blowfishHeader));
            q_blowfishHeader  symCipherHeader = &((encoder->securityHeader).u.blowfish);
            unsigned char     *counter = symCipherHeader->counter;

            DUMP_BUFFER(encoder->partitionName, partitionId, buffer, *dataLength, "encode->", counter,
                        Q_BLOWFISH_COUNTER_SIZE, bufferLength);

            attachHeaderAndDoSha1(plainText, plainTextLength,
                                  symCipherHeader, symCipherHeaderLength);

            /* encrypt load and digest */
            result = counterEncryptOrDecryptInPlace(ctx,
                                                    counter,
                                                    plainText,
                                                    (int) (plainTextLength + sizeof(C_STRUCT(q_sha1Header))));

            TRACE((":BLF:%s", result?"OK":"ERROR")); /* debug */

            DUMP_BUFFER(encoder->partitionName, partitionId, buffer, *dataLength+overallHeaderSize, "<-encode", counter,
                        Q_BLOWFISH_COUNTER_SIZE, bufferLength);
        }
        break;

        case Q_CIPHER_AES128:
        case Q_CIPHER_AES192:
        case Q_CIPHER_AES256: {
            const os_uint32    symCipherHeaderLength = sizeof(C_STRUCT(q_aesHeader));
            q_aesHeader       symCipherHeader = &((encoder->securityHeader).u.aes);
            unsigned char     *counter = symCipherHeader->counter;

            DUMP_BUFFER(encoder->partitionName, partitionId, buffer, *dataLength, "encode->", counter,
                        Q_AES_COUNTER_SIZE, bufferLength);

            attachHeaderAndDoSha1(plainText, plainTextLength,
                                  symCipherHeader, symCipherHeaderLength);

            /* encrypt load and digest */
            result = counterEncryptOrDecryptInPlace(ctx,
                                                    counter,
                                                    plainText,
                                                    (int) (plainTextLength + sizeof(C_STRUCT(q_sha1Header))));

            TRACE((":AES:%s", result?"OK":"ERROR")); /* debug */

            DUMP_BUFFER(encoder->partitionName, partitionId, buffer, *dataLength+overallHeaderSize, "<-encode", counter,
                        Q_AES_COUNTER_SIZE, bufferLength);
        }
        break;
        default:
            assert(0 && "do not reach");
  }
  TRACE((":(%d->%d)", *dataLength, *dataLength + overallHeaderSize));

  *dataLength += overallHeaderSize;
  return result;
}


static c_bool q_securityDecodeInPlace_Generic
(
  q_securityPartitionDecoder decoder,
  os_uint32 partitionId,
  unsigned char *buffer,
  os_uint32 *dataLength,          /* in/out */
  q_cipherType sendersCipherType,
  os_uint32 bufferLength
)
{
    const os_uint32 overallHeaderSize = cipherTypeToHeaderSize(sendersCipherType);
    EVP_CIPHER_CTX *ctx = &(decoder->cipherContext);
    c_bool result = TRUE;

    TRACE((":DECRYPT:'%s'(%d)",decoder->partitionName, partitionId));

    (void) bufferLength;

    switch (sendersCipherType) {
        case Q_CIPHER_NULL:
        {
            DUMP_BUFFER(decoder->partitionName, partitionId, buffer, *dataLength, "decode->",
                        NULL,
                        Q_NULL_COUNTER_SIZE, bufferLength);

            /* nothing todo here, just decreasing the buffer length at end of this function */
            TRACE((":NULL:%s", result?"OK":"ERROR")); /* debug */

            DUMP_BUFFER(decoder->partitionName, partitionId, buffer, *dataLength - overallHeaderSize, "<-decode",
                        NULL,
                        Q_NULL_COUNTER_SIZE, bufferLength);
        }
        break;

        case Q_CIPHER_BLOWFISH:
        {
            const os_uint32 sha1HeaderLength = sizeof(C_STRUCT(q_sha1Header));
            const os_uint32 cipherTextLength = *dataLength - overallHeaderSize + sha1HeaderLength;
            C_STRUCT(q_blowfishHeader) symCipherHeader;

            void *cipherText       = buffer;
            void *sha1HeaderStart  = &(buffer[*dataLength - overallHeaderSize]);
            void *symCipherHeaderStart  = &(buffer[*dataLength - overallHeaderSize + sha1HeaderLength]);

            /* copy from buffer into aligned memory */
            memcpy(&symCipherHeader, symCipherHeaderStart, sizeof(C_STRUCT(q_blowfishHeader)));

            DUMP_BUFFER(decoder->partitionName, partitionId, buffer, *dataLength, "decode->", symCipherHeader.counter,Q_BLOWFISH_COUNTER_SIZE, bufferLength);

            /* decrypt the load and the digest */
            result = counterEncryptOrDecryptInPlace(ctx,
                                                    symCipherHeader.counter,
                                                    cipherText,
                                                    (int) cipherTextLength);
            if (result) {
                /* will zero out the sha1Header values in buffer */
                result = verifySha1(cipherText, *dataLength, sha1HeaderStart);
                TRACE((":BLF:%s", result?"OK":"ERROR")); /* debug */
                if (!result) {
                    DDS_WARNING("Incoming encrypted sub-message dropped: Decrypt (blowfish) verification failed for partition '%s' - possible Key-mismatch\n", decoder->partitionName);
                }
            }

            DUMP_BUFFER(decoder->partitionName, partitionId, buffer, *dataLength - overallHeaderSize, "<-decode",symCipherHeader.counter, Q_BLOWFISH_COUNTER_SIZE, bufferLength);
        }
        break;

        case Q_CIPHER_AES128:
        case Q_CIPHER_AES192:
        case Q_CIPHER_AES256:
        {
            const os_uint32 sha1HeaderLength = sizeof(C_STRUCT(q_sha1Header));
            const os_uint32 cipherTextLength = *dataLength - overallHeaderSize + sha1HeaderLength;
            C_STRUCT(q_aesHeader) symCipherHeader;

            void *cipherText = buffer;
            void *sha1HeaderStart = &(buffer[*dataLength - overallHeaderSize]);
            void *symCipherHeaderStart  = &(buffer[*dataLength - overallHeaderSize + sha1HeaderLength]);

            /* copy from buffer into aligned memory */
            memcpy(&symCipherHeader, symCipherHeaderStart, sizeof(C_STRUCT(q_aesHeader)));

            DUMP_BUFFER(decoder->partitionName, partitionId, buffer, *dataLength, "decode->",symCipherHeader.counter, Q_AES_COUNTER_SIZE, bufferLength);

            /* decrypt the load and the digest */
            result = counterEncryptOrDecryptInPlace(ctx,
                                                    symCipherHeader.counter,
                                                    cipherText,
                                                    (int) cipherTextLength);
            if ( result){
                /* will zero out the sha1Header values in buffer */
                result = verifySha1(cipherText, *dataLength, sha1HeaderStart);
                DDS_TRACE(":AES:%s", result?"OK":"ERROR"); /* debug */
                if (!result) {
                    DDS_WARNING("Incoming encrypted sub-message dropped: Decrypt (AES) verification failed for partition '%s' - possible Key-mismatch", decoder->partitionName);
                }
            }

            DUMP_BUFFER(decoder->partitionName, partitionId, buffer, *dataLength - overallHeaderSize, "<-decode", symCipherHeader.counter, Q_AES_COUNTER_SIZE, bufferLength);
        }
        break;
        default:
            assert(0 && "do not reach");
    }

    DDS_TRACE(":(%d->%d)", *dataLength, *dataLength - overallHeaderSize);

    *dataLength      -= overallHeaderSize; /* out */

    return result;
}



/* returns 0 on error, otherwise 1,
   @param codec the security context object
   @param partitionId defines the security policy to be used
   @param buffer with content, with reserved space at end
   @param fragmentLength overall length of buffer
   @param dataLength the occupied space of buffer, must leave enough space for security attribute header */

static c_bool q_securityEncodeInPlace
(
  q_securityEncoderSet codec,
  os_uint32 partitionId,
  void *buffer,
  os_uint32 fragmentLength,
  os_uint32 *dataLength /* in/out */
)
{
    q_securityPartitionEncoder encoder = NULL;
    os_uint32    overallHeaderSize;
    c_bool result = FALSE;

    assert(codec);


    if (partitionId == 0 || partitionId > codec->nofPartitions) {
        /* if partitionId is larger than number of partitions, network service
         * seems to be in undefined state */
        DDS_ERROR("q_securityEncodeInPlace:Sending message blocked, bad partitionid '%d'\n",
                          partitionId);
        return FALSE;
    }

    encoder = &(codec->encoders[partitionId-1]);


    if (encoderIsBlocked(encoder)) {
        DDS_ERROR("q_securityEncodeInPlace:Sending message blocked, encoder of partitionid '%d' in bad state\n",
                          partitionId);
        return FALSE;
    }

    if (*dataLength <= 0) {
        DDS_WARNING("q_securityEncodeInPlace:encoder called with empty buffer\n");
        return FALSE;
    }

    overallHeaderSize = cipherTypeToHeaderSize(encoder->cipherType);

    if (*dataLength + overallHeaderSize  > fragmentLength) {
        DDS_ERROR("q_securityEncodeInPlace:sending message of %"PA_PRIu32" bytes overlaps with reserved space of %"PA_PRIu32" bytes\n",
                          *dataLength, overallHeaderSize);
        return FALSE;
    }

    assert(sizeof(os_uint32) == sizeof(os_uint32));

    /* do the encoding now */
    result = q_securityEncodeInPlace_Generic(encoder, partitionId, buffer, dataLength, fragmentLength);

    return result;
}

static c_bool q_securityGetHashFromCipherText
(
  unsigned char*  buffer,
  os_uint32       dataLength,
  os_uint32       *hash,
  q_cipherType    *sendersCipherType
)
{
    C_STRUCT(q_nullHeader) header;

    const os_uint32 headerSize = sizeof(C_STRUCT(q_nullHeader));

    unsigned char* end = NULL;

    assert(dataLength >= headerSize);
    assert(sizeof(os_uint32) == sizeof(os_uint32));

    end = &(buffer[dataLength - headerSize]);

    memcpy(&header, end, headerSize);
    *hash = ntohl(header.partitionId);
    *sendersCipherType = ntohl(header.cipherType);
    return TRUE;
}

/* returns 0 on error, otherwise 1,
   @param codec the security context object
   @param buffer containing the ciphertext
   @param fragmentLength overall length of buffer
   @param dataLength the occupied space within buffer, on return it contains the length of plaintext in buffer */
static c_bool q_securityDecodeInPlace
(
  q_securityDecoderSet codec,
  void *buffer,
  size_t fragmentLength,
  size_t *dataLength /* in/out */
)
{
    q_securityPartitionDecoder decoder = NULL;
    c_bool result = FALSE;
    os_uint32 hash;
    os_uint32 partitionId = 0;
    os_uint32 dataLength32 = (os_uint32) *dataLength;
    os_uint32 overallHeaderSize;
    q_cipherType sendersCipherType;
    struct config_networkpartition_listelem *p = config.networkPartitions;

    assert(codec);

    q_securityGetHashFromCipherText(buffer,dataLength32,&hash,&sendersCipherType);

    /* lookup hash in config to determine partitionId */
    while(p && ! partitionId) {
        if (p->partitionHash == hash) partitionId = p->partitionId;
        p = p->next;
    }

    if ((partitionId < 1) || (partitionId > codec->nofPartitions)) {
        DDS_WARNING("Incoming encrypted sub-message dropped, bad partition hash '%u'\n", hash);
        return FALSE;
    }

    decoder = &(codec->decoders[partitionId-1]);

    overallHeaderSize = cipherTypeToHeaderSize(sendersCipherType);

    if (sendersCipherType!=decoder->cipherType) {
        DDS_WARNING("Incoming encrypted sub-message dropped: cipherType mismatch (%d != %d) for partition '%s'\n", sendersCipherType,decoder->cipherType, decoder->partitionName);
        return FALSE;
    }
    if (decoderIsBlocked(decoder)) {
        DDS_WARNING("Incoming encrypted sub-message dropped: decoder is blocked for partition '%s'\n", decoder->partitionName);
        return FALSE;
    }

    if (overallHeaderSize > dataLength32) {
        DDS_WARNING("Incoming encrypted sub-message dropped: submessage too small(%"PA_PRIu32" bytes),for partition '%s'\n", dataLength32, decoder->partitionName);
        return FALSE;
    }

    result = q_securityDecodeInPlace_Generic(decoder, partitionId, buffer, &dataLength32, sendersCipherType, (os_uint32) fragmentLength);
    *dataLength = dataLength32;

    return result;
}

/*
 * Substitute for the sendmsg call that send the message encrypted:
 * iov[0] contains the RTPS header and is not encrypted
 * iov[1] contains the security header and is also not encrypted
 * iov[2..n] are concatenated into one buffer
 * Buffer is encrypted and will be the new third iov.
 * The size of the encrypted data is set in the second iov as the "octets to next message"
 *
 */

static os_ssize_t q_security_sendmsg
(
  ddsi_tran_conn_t conn,
  const nn_locator_t *dst,
  size_t niov, os_iovec_t *iov,
  q_securityEncoderSet *codec,
  os_uint32 encoderId,
  os_uint32 flags
)
{
  char stbuf[2048], *buf;
  size_t sz, data_size;
  uint32_t sz32, data_size32;
  ssize_t ret = ERR_UNSPECIFIED;
  PT_InfoContainer_t * securityHeader;
  unsigned i;

  assert (niov > 2);
  securityHeader = iov[1].iov_base;
  /* first determine the size of the message, then select the
     on-stack buffer or allocate one on the heap ... */
  sz = q_securityEncoderSetHeaderSize (*codec); /* reserve appropriate headersize */
  for (i = 2; i < niov; i++)
  {
    sz += iov[i].iov_len;
  }
  if (sz <= sizeof (stbuf))
  {
    buf = stbuf;
  }
  else
  {
    buf = os_malloc (sz);
  }
  /* ... then copy data into buffer */
  data_size = 0;
  for (i = 2; i < niov; i++)
  {
    memcpy (buf + data_size, iov[i].iov_base, iov[i].iov_len);
    data_size += iov[i].iov_len;
  }
  sz = data_size + q_securityEncoderSetHeaderSize (*codec);
  assert(sz <= UINT32_MAX);
  sz32 = (uint32_t) sz;
  data_size32 = (uint32_t) data_size;

  /* Encrypt the buf in place with the given encoder */

  if (q_securityEncodeInPlace (*codec, encoderId, buf, sz32, &data_size32))
  {
    os_size_t nbytes;
    /* replace encrypted buffer into iov */

    iov[2].iov_base = buf;
    iov[2].iov_len = data_size;
    niov = 3;
    /* correct size in security header */
    securityHeader->smhdr.octetsToNextHeader = (unsigned short) (data_size + 4);

    /* send the encrypted data to the connection */

    nbytes = message->msg_iov[0].iov_len + message->msg_iov[1].iov_len + message->msg_iov[2].iov_len;
    if (!gv.mute)
      ret = ddsi_conn_write (conn, dst, niov, iov, flags);
    else
    {
      TRACE (("(dropped)"));
      ret = (ssize_t) (iov[0].iov_len + iov[1].iov_len + iov[2].iov_len);
    }
  }

  if (buf != stbuf)
  {
    os_free (buf);
  }
  return ret;
}

void ddsi_security_plugin (void)
{
  q_security_plugin.encode = q_securityEncodeInPlace;
  q_security_plugin.decode = q_securityDecodeInPlace;
  q_security_plugin.new_encoder = q_securityEncoderSetNew;
  q_security_plugin.new_decoder = q_securityDecoderSetNew;
  q_security_plugin.free_encoder = q_securityEncoderSetFree;
  q_security_plugin.free_decoder = q_securityDecoderSetFree;
  q_security_plugin.send_encoded = q_security_sendmsg;
  q_security_plugin.cipher_type = cipherTypeAsString;
  q_security_plugin.cipher_type_from_string = q_securityCipherTypeFromString;
  q_security_plugin.header_size = q_securityEncoderHeaderSize;
  q_security_plugin.encoder_type = q_securityEncoderCipherType;
  q_security_plugin.valid_uri = q_securityIsValidCipherKeyUri;
}

#else

int ddsi_dummy_val = 0;

#endif /* DDSI_INCLUDE_ENCRYPTION */
