/**
 * @brief Prepares and starts the upgrade for the TPM
 * @author Jan Haeussermann
 * 
 */

#include <cstdint>
#include <cstddef>

#include <openssl/evp.h>

#include <iostream>
#include <cstring>
#include <cstdlib>

// Fix for conversion
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#include <unistd.h>
#pragma GCC diagnostic pop

#include "endian_helper.h"

#include <tss2/tss2_esys.h>
#include <tss2/tss2_tcti.h>
#include <tss2/tss2_tcti_device.h>
#include <tss2/tss2_tpm2_types.h>

#include "tpm2_updater.h"
#include "tpm2_types.h"
#include "tpm2_file_parser.h"
#include "tpm2_get_info.h"

/*****************************************************************************************************/
// Raw transmission functions
/*****************************************************************************************************/

/**
 * @brief Writes a uint8 to a buffer and advances the offset
 * 
 * @param buffer The buffer to write to
 * 
 * @param buffer_size The size of the buffer
 * 
 * @param offset The offset to write to and advance
 * 
 * @param value The value to write
 * 
 * @return true when successful, otherwise false
 */
bool tpm_update_write_uint8(uint8_t *buffer, size_t buffer_size, size_t *offset, uint8_t value){
    if(buffer == NULL || offset == NULL || *offset + 1 > buffer_size){
        return false;
    }

    buffer[*offset] = value;
    (*offset)++;
    
    return true;
}

/**
 * @brief Writes a uint16 to a buffer and advances the offset
 * 
 * @param buffer The buffer to write to
 * 
 * @param buffer_size The size of the buffer
 * 
 * @param offset The offset to write to and advance
 * 
 * @param value The value to write
 * 
 * @return true when successful, otherwise false
 */
bool tpm_update_write_uint16(uint8_t *buffer, size_t buffer_size, size_t *offset, uint16_t value){
    if(buffer == NULL || offset == NULL || *offset + 2 > buffer_size){
        return false;
    }
    
    buffer[*offset] = (uint8_t)((value >> 8) & 0xFF);
    (*offset)++;
    buffer[*offset] = (uint8_t)(value & 0xFF);
    (*offset)++;

    return true;
}

/**
 * @brief Writes a uint32 to a buffer and advances the offset
 * 
 * @param buffer The buffer to write to
 * 
 * @param buffer_size The size of the buffer
 * 
 * @param offset The offset to write to and advance
 * 
 * @param value The value to write
 * 
 * @return true when successful, otherwise false
 */
bool tpm_update_write_uint32(uint8_t *buffer, size_t buffer_size, size_t *offset, uint32_t value){
    if(buffer == NULL || offset == NULL || *offset + 4 > buffer_size){
        return false;
    }
    
    buffer[*offset] = (uint8_t)((value >> 24) & 0xFF);
    (*offset)++;
    buffer[*offset] = (uint8_t)((value >> 16) & 0xFF);
    (*offset)++;
    buffer[*offset] = (uint8_t)((value >> 8) & 0xFF);
    (*offset)++;
    buffer[*offset] = (uint8_t)(value & 0xFF);
    (*offset)++;

    return true;
}

/**
 * @brief Reads a uint8 from the buffer
 * 
 * @param buffer The buffer to read from
 * 
 * @return The uint8 value read from the buffer
 */
uint8_t tpm_update_read_uint8(uint8_t *buffer){
    return buffer[0];
}

/**
 * @brief Reads a uint16 from the buffer
 * 
 * @param buffer The buffer to read from
 * 
 * @return The uint16 value read from the buffer
 */
uint16_t tpm_update_read_uint16(uint8_t *buffer){
    return ((((uint16_t)buffer[0]) << 8) |
            ((uint16_t) buffer[1]));
}

/**
 * @brief Reads a uint32 from the buffer
 * 
 * @param buffer The buffer to read from
 * 
 * @return The uint32 value read from the buffer
 */
uint32_t tpm_update_read_uint32(uint8_t *buffer){
    return ((((uint32_t)buffer[0]) << 24) |
            (((uint32_t)buffer[1]) << 16) |
            (((uint32_t)buffer[2]) << 8) |
            ((uint32_t) buffer[3]));
}

/**
 * @brief Transmits a command buffer to the TPM and receives the response via a buffer
 * 
 * @param connection The TPM connection struct
 * 
 * @param command_buf The command buffer to send
 * 
 * @param command_size The size of the command buffer
 * 
 * @param response_buf The buffer to receive the response
 * 
 * @param response_size The size of the response buffer
 * 
 * @param response_code The TPM response code
 * 
 * @return true when successful, otherwise false
 */
bool tpm_update_transmit(struct TPMConnection **connection, uint8_t *command_buf, size_t command_size, uint8_t *response_buf, size_t *response_size, TPM2_RC *response_code){
    if(connection == NULL || *connection == NULL || (*connection)->tcti_ctx == NULL || command_buf == NULL || command_size < 10 || response_buf == NULL || response_size == NULL || response_code == NULL){
        std::cerr << "Error: Invalid argument in tpm_update_transmit" << std::endl;
        return false;
    }
    
    if((*connection)->config.verbose_print){
        std::cout << "Info: Transmitting command to TPM" << std::endl;
    }

    TSS2_RC ret = Tss2_Tcti_Transmit((*connection)->tcti_ctx, command_size, command_buf);
    if(ret != TSS2_RC_SUCCESS){
        std::cerr << "Error: Tss2_Tcti_Transmit failed" << std::endl;
        return false;
    }

    ret = Tss2_Tcti_Receive((*connection)->tcti_ctx, response_size, response_buf, TSS2_TCTI_TIMEOUT_BLOCK);
    if(ret != TSS2_RC_SUCCESS){
        std::cerr << "Error: Tss2_Tcti_Receive failed" << std::endl;
        return false;
    }

    if(*response_size < 10){
        std::cerr << "Error: Invalid response size" << std::endl;
        return false;
    }

    uint32_t returned_size = tpm_update_read_uint32(response_buf + 2);
    if(returned_size > *response_size || returned_size < 10){
        std::cerr << "Error: Invalid response size" << std::endl;
        return false;
    }

    *response_code = tpm_update_read_uint32(response_buf + 6);
    if(*response_code != TPM2_RC_SUCCESS){
        std::cerr << "Error: TPM returned error code: 0x" << std::hex << *response_code << std::dec << std::endl;
        return false;
    }

    return true;
}

/*****************************************************************************************************/
// Helper functions
/*****************************************************************************************************/

/**
 * @brief Calculate the CRC value of a given buffer
 * 
 * @param data buffer to check the value
 * 
 * @param data_size size of the buffer
 * 
 * @return 32 bit crc value
 */
uint32_t tpm_update_crc32(uint8_t *data, size_t data_size){
    if(data == NULL || data_size == 0){
        return 0;
    }

    uint32_t crc = 0;
    crc = ~crc;

    while(data_size-- != 0){
        crc ^= *data++;

        for(int bit = 0; bit < 8; bit++){ // Taken from Crypt.C from Infineon updater
            crc = (crc >> 1) ^ (-((int)(crc & 1)) & 0xEDB88320);
        }
    }

    return ~crc;
}

/**
 * @brief Calculate and checks the checksum of the image_blob
 * 
 * @param firmware struct of the firmware
 * 
 * @return true when correct crc, false otherwise
 */
bool tpm_update_check_image_crc(struct TPMFirmwareImageFile *firmware){
    if(firmware == NULL || firmware->image_blob == NULL || firmware->image_blob_size <= 4){
        std::cerr << "Error: Invalid firmware image buffer for CRC" << std::endl;
        return false;
    }

    uint32_t calculated_crc = tpm_update_crc32(firmware->image_blob, firmware->image_blob_size - 4);

    if(calculated_crc != firmware->image.unChecksum){
        std::cerr << "Error: Firmware image CRC is invalid" << std::endl;
        return false;
    }

    return true;
}

/**
 * @brief Returns the current firmware version of the image
 * 
 * @param info struct of the TPM information
 * 
 * @param version firmware version 
 * 
 * @param version_short firmware version
 * 
 * @return returns true when successful, false otherwise
 */
bool tpm_get_firmware_version_string(struct TPMInformation **info, char *version, char *version_short){

    if(info == NULL || *info == NULL || version == NULL || version_short == NULL){
        std::cerr << "Error: Invalid argument in tpm_get_firmware_version_string" << std::endl;
        return false;
    }

    uint16_t major = (uint16_t)(((*info)->firmware_version >> 48) & 0xFFFF);
    uint16_t minor = (uint16_t)(((*info)->firmware_version >> 32) & 0xFFFF);
    uint16_t build = (uint16_t)(((*info)->firmware_version >> 8) & 0xFFFF);
    uint8_t revision = (uint8_t)((*info)->firmware_version & 0xFF);

    snprintf(version, TPM_FIRMWARE_MAX_VERSION_LENGTH + 1, "%u.%u.%u.%u", major, minor, build, revision);

    snprintf(version_short, TPM_FIRMWARE_MAX_VERSION_LENGTH + 1, "%u.%u.%u", major, minor, build);

    return true;
}

/**
 * @brief Checks if the source version of the firmware image is compatible with the current firmware version of the TPM
 * 
 * @param info struct of the TPM information
 * 
 * @param image struct of the firmware image
 * 
 * @return true when compatible, false otherwise
 */
bool tpm_update_check_source_version(struct TPMInformation **info, struct TPMFirmwareImage *image){
    if(info == NULL || *info == NULL || image == NULL){
        std::cerr << "Error: Invalid argument in tpm_update_check_source_version" << std::endl;
        return false;
    }

    if((*info)->type == INFINEON_SLB9670_TYPE && (*info)->operation_mode == SMS_BTLDR_ACTIVE){
        if((*info)->active_firmware_version == 0){
            return true; // no active version
        }

        for(uint16_t i = 0; i < image->usIntSourceVersionCount && i < MAX_SOURCE_VERSIONS_COUNT; i++){
            if(image->rgunIntSourceVersions[i] == (*info)->active_firmware_version){
                return true;
            }
        }
        return false;
    }

    char version[TPM_FIRMWARE_MAX_VERSION_LENGTH + 1] = {};
    char version_short[TPM_FIRMWARE_MAX_VERSION_LENGTH + 1] = {};

    if(!tpm_get_firmware_version_string(info, version, version_short)){
        return false;
    }

    if(image->prgwszSourceVersions == NULL || image->usSourceVersionsCount == 0){
        return false;
    }

    struct TPMFirmwareReader reader = {
        image->prgwszSourceVersions,
        image->usSourceVersionsSize,
        0
    };

    for(uint16_t i = 0; i < image->usSourceVersionsCount; i++){
        size_t string_size = 0;

        for(size_t pos = reader.offset; pos + 1 < reader.size; pos += 2){
            string_size += 2;
            if(reader.buffer[pos] == 0x00 && reader.buffer[pos + 1] == 0x0){
                break;
            }
        }

        if(string_size == 0 || string_size > reader_remaining_size(&reader)){
            return false;
        }

        char source_version[TPM_FIRMWARE_MAX_VERSION_LENGTH + 1] = {0};
        size_t source_version_length = 0;

        if(!reader_utf16_string(&reader, string_size, sizeof(source_version), source_version, &source_version_length)){
            return false;
        }

        if(strcmp(source_version, version) == 0 || strcmp(source_version, version_short) == 0){
            return true;
        }
    }

    return false;
}

/**
 * @brief Updates the hash of the given data using the specified algorithm and returns the digest
 * 
 * @param connection The TPM connection struct
 * 
 * @param algorithm The hash algorithm to use
 * 
 * @param data The data to hash
 * 
 * @param data_size The size of the data to hash
 * 
 * @param digest The buffer to store the resulting digest
 * 
 * @param digest_size The size of the resulting digest
 * 
 * @return true when successful, otherwise false
 */
bool tpm_update_hash(struct TPMConnection **connection, TPMI_ALG_HASH algorithm, uint8_t *data, size_t data_size, uint8_t *digest, uint16_t *digest_size){
    (void)connection;
    if(data == NULL || digest == NULL || digest_size == NULL){
        std::cerr << "Error: Invalid argument in tpm_update_hash" << std::endl;
        return false;
    }

    const EVP_MD *md = NULL;
    if(algorithm == TPM2_ALG_SHA256){
        md = EVP_sha256();
    } else if(algorithm == TPM2_ALG_SHA384){
        md = EVP_sha384();
    } else if(algorithm == TPM2_ALG_SHA512){
        md = EVP_sha512();
    } else{
        std::cerr << "Error: Unsupported hash algorithm" << std::endl;
        return false;
    }

    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if(ctx == NULL){
        std::cerr << "Error: Failed to create EVP_MD_CTX" << std::endl;
        return false;
    }

    if(EVP_DigestInit_ex(ctx, md, NULL) != 1){
        std::cerr << "Error: EVP_DigestInit_ex failed" << std::endl;
        EVP_MD_CTX_free(ctx);
        return false;
    }

    if(EVP_DigestUpdate(ctx, data, data_size) != 1){
        std::cerr << "Error: EVP_DigestUpdate failed" << std::endl;
        EVP_MD_CTX_free(ctx);
        return false;
    }

    unsigned int result_size = 0;
    if(EVP_DigestFinal_ex(ctx, digest, &result_size) != 1){
        std::cerr << "Error: EVP_DigestFinal_ex failed" << std::endl;
        EVP_MD_CTX_free(ctx);
        return false;
    }

    if(result_size > 64) {
        std::cerr << "Error: Digest result is too large" << std::endl;
        EVP_MD_CTX_free(ctx);
        return false;
    }

    *digest_size = static_cast<uint16_t>(result_size);

    EVP_MD_CTX_free(ctx);

    return true;
}

/**
 * @brief Gets a random nonce from the TPM 
 * 
 * @param connection The TPM connection struct
 * 
 * @param nonce The buffer to store the resulting nonce
 * 
 * @return true when successful, otherwise false
 */
bool tpm_update_get_random_nonce(struct TPMConnection **connection, TPM2B_DIGEST *nonce){
    if(connection == NULL || *connection == NULL || nonce == NULL || (*connection)->esys_ctx == NULL){
        std::cerr << "Error: Invalid argument in tpm_update_get_random_nonce" << std::endl;
        return false;
    }

    memset(nonce, 0, sizeof(*nonce));
    nonce->size = 32;
    
    size_t offset = 0;

    while(offset < nonce->size){
        TPM2B_DIGEST *random = NULL;
        uint16_t requested = (uint16_t)(nonce->size - offset);

        TSS2_RC ret = Esys_GetRandom((*connection)->esys_ctx, ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE, requested, &random);
        if(ret != TSS2_RC_SUCCESS || random == NULL || random->size == 0){
            if(random != NULL){
                Esys_Free(random);
            }
            std::cerr << "Error: Esys_GetRandom failed" << std::endl;
            return false;
        }

        size_t copy_size = std::min<size_t>(random->size, nonce->size - offset);
        memcpy(nonce->buffer + offset, random->buffer, copy_size);
        offset += copy_size;
        Esys_Free(random);
    }

    return true;
}

/**
 * @brief Prepares the TPM for a firmware update by setting the appropriate policy and starting a policy session
 * 
 * @param connection The TPM connection struct
 * 
 * @param policy_session The policy session handle to be returned
 * 
 * @param policy_handle The policy handle to be returned
 * 
 * @return true when successful, otherwise false
 */
bool tpm_prepare_firmware_update_policy(struct TPMConnection **connection, ESYS_TR *policy_session, TPM2_HANDLE *policy_handle){
    if(connection == NULL || *connection == NULL || (*connection)->esys_ctx == NULL || policy_session == NULL || policy_handle == NULL){
        std::cerr << "Error: Invalid argument in tpm_prepare_firmware_update_policy" << std::endl;
        return false;
    }

    *policy_session = ESYS_TR_NONE;
    *policy_handle = 0;

    if((*connection)->config.verbose_print){
        std::cout << "Info: Preparing firmware update policy" << std::endl;
    }

    TPM2B_AUTH empty_plat_auth = {};
    TSS2_RC ret = Esys_TR_SetAuth((*connection)->esys_ctx, ESYS_TR_RH_PLATFORM, &empty_plat_auth);
    if(ret != TSS2_RC_SUCCESS){
        std::cerr << "Error: Esys_TR_SetAuth failed" << std::endl;
        return false;
    }

    TPM2B_DIGEST policy_digest = {};
    policy_digest.size = TPM20_FIRMWARE_UPDATE_POLICY_SIZE;

    memcpy(policy_digest.buffer, TPM_FW_UPDATE_POLICY_DIGEST, TPM20_FIRMWARE_UPDATE_POLICY_SIZE);

    if((*connection)->config.verbose_print){
        std::cout << "Info: Setting primary policy for firmware update" << std::endl;
    }
    ret = Esys_SetPrimaryPolicy((*connection)->esys_ctx, ESYS_TR_RH_PLATFORM, ESYS_TR_PASSWORD, ESYS_TR_NONE, ESYS_TR_NONE, &policy_digest, TPM2_ALG_SHA256);
    if(ret != TSS2_RC_SUCCESS){
        std::cerr << "Error: Esys_SetPrimaryPolicy failed" << std::endl;
        return false;
    }

    TPM2B_NONCE nonce = {};
    if(!tpm_update_get_random_nonce(connection, &nonce)){
        std::cerr << "Error: Failed to get random nonce" << std::endl;
        return false;
    }

    TPMT_SYM_DEF symmetric = {};
    symmetric.algorithm = TPM2_ALG_NULL;

    if((*connection)->config.verbose_print){
        std::cout << "Info: Starting auth session for firmware update policy" << std::endl;
    }
    ret = Esys_StartAuthSession((*connection)->esys_ctx, ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE, &nonce, TPM2_SE_POLICY, &symmetric, TPM2_ALG_SHA256, policy_session);
    if(ret != TSS2_RC_SUCCESS){
        std::cerr << "Error: Esys_StartAuthSession failed" << std::endl;
        return false;
    }

    TPM2B_NONCE *tpm_nonce = NULL;
    ret = Esys_TRSess_GetNonceTPM((*connection)->esys_ctx, *policy_session, &tpm_nonce);
    if(ret != TSS2_RC_SUCCESS || tpm_nonce == NULL){
        Esys_FlushContext((*connection)->esys_ctx, *policy_session);
        *policy_session = ESYS_TR_NONE;
        return false;
    }

    TPM2B_DIGEST cp_hash = {};
    TPM2B_NONCE policy_ref = {};
    policy_ref.size = 4;
    policy_ref.buffer[0] = 0x40;
    policy_ref.buffer[1] = 0x00;
    policy_ref.buffer[2] = 0x00;
    policy_ref.buffer[3] = 0x0C;

    TPM2B_TIMEOUT *timeout = NULL;
    TPMT_TK_AUTH *policy_ticket = NULL;

    if((*connection)->config.verbose_print){
        std::cout << "Info: Executing PolicySecret for firmware update policy" << std::endl;
    }
    ret = Esys_PolicySecret((*connection)->esys_ctx, ESYS_TR_RH_PLATFORM, *policy_session, ESYS_TR_PASSWORD, ESYS_TR_NONE, ESYS_TR_NONE, tpm_nonce, &cp_hash, &policy_ref, 0, &timeout, &policy_ticket);
    Esys_Free(tpm_nonce);
    Esys_Free(timeout);
    Esys_Free(policy_ticket);

    if(ret != TSS2_RC_SUCCESS){
        Esys_FlushContext((*connection)->esys_ctx, *policy_session);
        *policy_session = ESYS_TR_NONE;
        std::cerr << "Error: Esys_PolicySecret failed" << std::endl;
        return false;
    }

    if((*connection)->config.verbose_print){
        std::cout << "Info: Executing PolicyCommandCode for firmware update policy" << std::endl;
    }
    ret = Esys_PolicyCommandCode((*connection)->esys_ctx, *policy_session, ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE, TPM2_CC_Field_Upgrade_Start_Vendor);
    if(ret != TSS2_RC_SUCCESS){
        Esys_FlushContext((*connection)->esys_ctx, *policy_session);
        *policy_session = ESYS_TR_NONE;
        std::cerr << "Error: Esys_PolicyCommandCode failed" << std::endl;
        return false;
    }

    if((*connection)->config.verbose_print){
        std::cout << "Info: Retrieving TPM handle for firmware update policy session" << std::endl;
    }
    ret = Esys_TR_GetTpmHandle((*connection)->esys_ctx, *policy_session, policy_handle);
    if(ret != TSS2_RC_SUCCESS){
        Esys_FlushContext((*connection)->esys_ctx, *policy_session);
        *policy_session = ESYS_TR_NONE;
        *policy_handle = 0;
        std::cerr << "Error: Esys_TR_GetTpmHandle failed" << std::endl;
        return false;
    }

    return true;
}

/**
 * @brief Reconnects to the TPM by finalizing the current connection and reinitializing 
 * 
 * @param connection The TPM connection struct
 * 
 * @return true when successful, otherwise false
 */
bool tpm_update_reconnect(struct TPMConnection **connection){
    if(connection == NULL || *connection == NULL){
        std::cerr << "Error: Invalid argument in tpm_update_reconnect" << std::endl;
        return false;
    }

    if((*connection)->config.verbose_print){
        std::cout << "Info: Reconnecting to TPM" << std::endl;
    }

    if((*connection)->esys_ctx != NULL){
        Esys_Finalize(&(*connection)->esys_ctx);
        (*connection)->esys_ctx = NULL;
    }

    if((*connection)->tcti_ctx != NULL){
        Tss2_Tcti_Finalize((*connection)->tcti_ctx);
        (*connection)->tcti_ctx = NULL;
    }

    if((*connection)->tcti_buf != NULL){
        free((*connection)->tcti_buf);
        (*connection)->tcti_buf = NULL;
    }

    (*connection)->tcti_buf_size = 0;

    TSS2_RC ret = Tss2_Tcti_Device_Init(NULL, &(*connection)->tcti_buf_size, NULL);
    if(ret != TSS2_RC_SUCCESS){
        return false;
    }

    (*connection)->tcti_buf = (uint8_t*)malloc((*connection)->tcti_buf_size);
    if((*connection)->tcti_buf == NULL){
        return false;
    }

    (*connection)->tcti_ctx = (TSS2_TCTI_CONTEXT*)(*connection)->tcti_buf;
    ret = Tss2_Tcti_Device_Init((*connection)->tcti_ctx, &(*connection)->tcti_buf_size, (*connection)->config.tcti_device_path);
    if(ret != TSS2_RC_SUCCESS){
        free((*connection)->tcti_buf);
        (*connection)->tcti_buf = NULL;
        (*connection)->tcti_ctx = NULL;
        return false;
    }

    ret = Esys_Initialize(&(*connection)->esys_ctx, (*connection)->tcti_ctx, NULL);
    if(ret != TSS2_RC_SUCCESS){
        Tss2_Tcti_Finalize((*connection)->tcti_ctx);
        free((*connection)->tcti_buf);
        (*connection)->tcti_buf = NULL;
        (*connection)->tcti_ctx = NULL;
        return false;
    }

    if((*connection)->config.verbose_print){
        std::cout << "Info: Successfully reconnected to TPM" << std::endl;
    }

    return true;
}

/*****************************************************************************************************/
// SLB9670 check functions
/*****************************************************************************************************/

/**
 * @brief Checks the SLB9670 firmware policy parameter block against the TPM information and firmware image
 * 
 * @param connection The TPM connection struct
 * 
 * @param info The TPM information struct
 * 
 * @param image The firmware image struct
 * 
 * @return true when successful, otherwise false
 */
bool tpm_update_check_slb9670_policy(struct TPMConnection **connection, struct TPMInformation **info, struct TPMFirmwareImage *image){
    if(connection == NULL || *connection == NULL || info == NULL || *info == NULL || image == NULL || image->rgbPolicyParameterBlock == NULL || image->usPolicyParameterBlockSize == 0){
        std::cerr << "Error: Invalid argument in tpm_update_check_slb9670_policy" << std::endl;
        return false;
    }

    struct TPMFirmwareReader policy_reader = {
        image->rgbPolicyParameterBlock,
        image->usPolicyParameterBlockSize,
        0
    };

    if((*connection)->config.verbose_print){
        std::cout << "Info: SLB9670 TPM ssigned data check" << std::endl;
    }
    sSignedData_d signed_data;
    if(!tpm_parse_signeddata(&policy_reader, &signed_data)){
        std::cerr << "Error: Failed to parse signed data" << std::endl;
        return false;
    }

    if(signed_data.sSignerInfo.sSignedAttributes.sMessageDigest.wSize != 32){
        std::cerr << "Error: SLB9670 firmware policy parameter block has invalid message digest size" << std::endl;
        return false;
    }

    uint8_t digest[64] = {0};
    uint16_t digest_size = 0;

    if((*connection)->config.verbose_print){
        std::cout << "Info: SLB9670 TPM hash check" << std::endl;
    }
    if(!tpm_update_hash(connection, TPM2_ALG_SHA256, image->rgbFirmware, image->unFirmwareSize, digest, &digest_size)){
        std::cerr << "Error: Failed to update hash" << std::endl;
        return false;
    }

    if(digest_size != 32 || memcmp(digest, signed_data.sSignerInfo.sSignedAttributes.sMessageDigest.rgbMessageDigest, 32) != 0){
        std::cerr << "Error: SLB9670 firmware policy parameter block has invalid message digest" << std::endl;
        return false;
    }

    if((*info)->decrypt_key_count > 4){
        std::cerr << "Error: SLB9670 TPM has invalid decrypt key count" << std::endl;
        return false;
    }

    bool decrypt_key_found = false;
    uint32_t required_key = signed_data.sSignerInfo.sSignedAttributes.DecryptKeyId;

    if((*connection)->config.verbose_print){
        std::cout << "Info: SLB9670 TPM decrypt key check" << std::endl;
    }
    for(uint16_t i = 0; i < (*info)->decrypt_key_count; i++){
        if((*info)->decrypt_key_ids[i] == required_key){
            decrypt_key_found = true;
            break;
        }
    }

    if(!decrypt_key_found){
        std::cerr << "Error: SLB9670 TPM does not have the required decrypt key" << std::endl;
        return false;
    }

    if((*info)->operation_mode == SMS_BTLDR_ACTIVE && (image->bfCapabilities & TPM_FIRMWARE_CAP_MATCH_UNIQUE_ID) != 0){
        uint8_t unique_id = (uint8_t)(signed_data.sSignerInfo.sSignedAttributes.sFirmwarePackage.StaleVersion & 0x0F);

        if(unique_id != (*info)->process_firmware_unique_id){
            std::cerr << "Error: SLB9670 TPM unique id does not match the firmware image unique id" << std::endl;
            return false;
        }
    }

    return true;
}

/**
 * @brief Checks the SLB9670 firmware image against the TPM information and policy parameter block
 * 
 * @param connection The TPM connection struct
 * 
 * @param info The TPM information struct
 * 
 * @param image The firmware image struct
 * 
 * @return true when successful, otherwise false
 */
bool tpm_update_check_slb9670(struct TPMConnection **connection, struct TPMInformation **info, struct TPMFirmwareImage *image){
    if(connection == NULL || *connection == NULL || info == NULL || *info == NULL || image == NULL){
        std::cerr << "Error: Invalid argument in tpm_update_check_slb9670" << std::endl;
        return false;
    }

    if((*connection)->config.verbose_print){
        std::cout << "Info: SLB9670 TPM firmware image parsing" << std::endl;
    }

    if(image->rgbPolicyParameterBlock == NULL || image->usPolicyParameterBlockSize == 0){
        std::cerr << "Error: SLB9670 firmware has no policy parameter block" << std::endl;
        return false;
    }

    if((*info)->operation_mode != SMS_FWCONFIG_ACTIVE && (*info)->operation_mode != SMS_BTLDR_ACTIVE){
        std::cerr << "Error: SLB9670 TPM is not in a valid operation mode for firmware update" << std::endl;
        return false;
    }

    if((*info)->target_hardware_identifier != 0 && (*info)->target_hardware_identifier != SLB9670_TARGET_HARDWARE_IDENTIFIER){
        std::cerr << "Error: SLB9670 TPM target hardware identifier does not match the firmware image target hardware identifier" << std::endl;
        return false;
    }

    if(!tpm_update_check_source_version(info, image)){
        std::cerr << "Error: SLB9670 firmware source version does not match the TPM firmware version" << std::endl;
        return false;
    }

    if((*connection)->config.verbose_print){
        std::cout << "Info: SLB9670 TPM policy check" << std::endl;
    }
    if(!tpm_update_check_slb9670_policy(connection, info, image)){
        std::cerr << "Error: SLB9670 firmware policy check failed" << std::endl;
        return false;
    }

    return true;
}

/*****************************************************************************************************/
// SLB9672 check functions
/*****************************************************************************************************/

/**
 * @brief Selects the appropriate firmware manifest from the firmware image based on the TPM information
 * 
 * @param info The TPM information struct
 * 
 * @param image The firmware image struct
 * 
 * @return true when successful, otherwise false
 */
bool tpm_select_firmware_manifest(struct TPMInformation **info, struct TPMFirmwareImage *image){
    if(info == NULL || *info == NULL || image == NULL){
        std::cerr << "Error: Invalid argument in tpm_select_firmware_manifest" << std::endl;
        return false;
    }

    if(image->rgbManifestData == NULL ||image->unManifestDataSize < 2){
        std::cerr << "Error: SLB9672 firmware has no manifest data" << std::endl;
        return false;
    }

    struct TPMFirmwareReader manifest_reader = {
        image->rgbManifestData,
        image->unManifestDataSize,
        0
    };

    uint16_t manifest_count = 0;

    if(!reader_uint16(&manifest_reader, &manifest_count) || manifest_count == 0){
        std::cerr << "Error: SLB9672 firmware manifest count does not match the number of manifests" << std::endl;
        return false;
    }

    image->usNumManifests = manifest_count;

    for(uint16_t i = 0; i < manifest_count; i++){
        uint32_t key_group_id = 0;
        uint16_t manifest_size = 0;

        if(!reader_uint32(&manifest_reader, &key_group_id) || !reader_uint16(&manifest_reader, &manifest_size)){
            std::cerr << "Error: SLB9672 firmware manifest data is invalid" << std::endl;
            return false;
        }

        if(manifest_size > reader_remaining_size(&manifest_reader)){
            std::cerr << "Error: SLB9672 firmware manifest size is larger than remaining data" << std::endl;
            return false;
        }

        if(key_group_id == (*info)->key_group_id){
            image->rgbPolicyParameterBlock = manifest_reader.buffer + manifest_reader.offset;
            image->usPolicyParameterBlockSize = manifest_size;
            return true;
        }

        if(!reader_skip(&manifest_reader, manifest_size)){
            std::cerr << "Error: SLB9672 firmware manifest data is invalid" << std::endl;
            return false;
        }
    }

    return false;
}

/**
 * @brief Checks the SLB9672 firmware start hash against the TPM information and firmware image
 * 
 * @param connection The TPM connection struct
 * 
 * @param info The TPM information struct
 * 
 * @param image The firmware image struct
 * 
 * @return true when successful, otherwise false
 */
bool tpm_update_check_slb9672_start_hash(struct TPMConnection **connection, struct TPMInformation **info, struct TPMFirmwareImage *image){
    if(connection == NULL || *connection == NULL || info == NULL || *info == NULL || image == NULL || image->rgbPolicyParameterBlock == NULL){
        std::cerr << "Error: Invalid argument in tpm_update_check_slb9672_start_hash" << std::endl;
        return false;
    }

    if((*info)->start_hash_algorithm != TPM2_ALG_SHA384 && (*info)->start_hash_algorithm != TPM2_ALG_SHA512){
        std::cerr << "Error: SLB9672 TPM has invalid start hash algorithm or size" << std::endl;
        return false;
    }

    uint8_t digest[64] = {0};
    uint16_t digest_size = 0;

    if(!tpm_update_hash(connection, (TPMI_ALG_HASH)(*info)->start_hash_algorithm, image->rgbPolicyParameterBlock, image->usPolicyParameterBlockSize, digest, &digest_size)){
        std::cerr << "Error: Failed to update hash" << std::endl;
        return false;
    }

    if(digest_size != (*info)->start_hash_size){
        return false;
    }

    return memcmp(digest, (*info)->start_hash_digest, digest_size) == 0;
}

/**
 * @brief Checks the SLB9672 firmware image against the TPM information and manifest data
 * 
 * @param connection The TPM connection struct
 * 
 * @param info The TPM information struct
 * 
 * @param image The firmware image struct
 * 
 * @return true when successful, otherwise false
 */
bool tpm_update_check_slb9672(struct TPMConnection **connection, struct TPMInformation **info, struct TPMFirmwareImage *image){
    if(connection == NULL || *connection == NULL || info == NULL || *info == NULL || image == NULL){
        std::cerr << "Error: Invalid argument in tpm_update_check_slb9672" << std::endl;
        return false;
    }

    if((*connection)->config.verbose_print){
        std::cout << "Info: Checking SLB9672 firmware image against TPM information and manifest data" << std::endl;
    }

    if(image->rgbManifestData == NULL || image->unManifestDataSize == 0){
        std::cerr << "Error: SLB9672 firmware has no manifest data" << std::endl;
        return false;
    }

    if((*info)->upgrade_counter_same == 0){ // In the Source/Common/FirmwareUpdate.c(Line 1364) Does not make sense to only check for 0
        std::cerr << "Error: SLB9672 TPM has no upgrades left for the same firmware version" << std::endl;
        return false;
    }

    char version[TPM_FIRMWARE_MAX_VERSION_LENGTH + 1] = {};
    char version_short[TPM_FIRMWARE_MAX_VERSION_LENGTH + 1] = {};

    if(!tpm_get_firmware_version_string(info, version, version_short)){
        return false;
    }

    bool same_version = false;
    if(strcmp(image->wszTargetVersion, version) == 0 || strcmp(image->wszTargetVersion, version_short) == 0){
        same_version = true;
    }

    if(same_version && !(*connection)->config.update_same_firmware){
        std::cerr << "Error: SLB9672 target firmware version is already installed" << std::endl;
        return false;
    }

    switch ((*info)->operation_mode){
    case TPM_OPERATION_MODE_NORMAL:
    case TPM_OPERATION_MODE_UPDATE_CAN_ABANDON:
    case TPM_OPERATION_MODE_UPDATE_CANNOT_ABANDON:
    case TPM_OPERATION_MODE_BEFORE_FINALIZE:
    case TPM_OPERATION_MODE_RECOVERY_CAN_ABANDON:
    case TPM_OPERATION_MODE_RECOVERY_CANNOT_ABANDON:
    case TPM_OPERATION_MODE_RECOVERY_BEFORE_FINALIZE:
        break;
    case TPM_OPERATION_MODE_RESTART_REQUIRED:
    case TPM_OPERATION_MODE_RECOVERY_RESTART_REQUIRED:
        std::cerr << "Error: SLB9672 TPM is in a restart required operation mode" << std::endl;
        return false;
    default:
        std::cerr << "Error: SLB9672 TPM is in an unknown operation mode" << std::endl;
        return false;
    }

    if(!tpm_select_firmware_manifest(info, image)){
        return false;
    }

    if(!tpm_update_check_source_version(info, image)){
        std::cerr << "Error: SLB9672 firmware source version does not match the TPM firmware version" << std::endl;
        return false;
    }

    if((*info)->operation_mode != TPM_OPERATION_MODE_NORMAL && (*info)->operation_mode != TPM_OPERATION_MODE_BEFORE_FINALIZE && (*info)->operation_mode != TPM_OPERATION_MODE_RECOVERY_BEFORE_FINALIZE){
        if(!tpm_update_check_slb9672_start_hash(connection, info, image)){
            std::cerr << "Error: SLB9672 firmware start hash check failed" << std::endl;
            return false;
        }
    }

    return true;
}

/*****************************************************************************************************/
// Initalisation updater functions
/*****************************************************************************************************/

/**
 * @brief Checks if the firmware image is compatible with the TPM information and connection
 * 
 * @param connection The TPM connection struct
 * 
 * @param info The TPM information struct
 * 
 * @param image The firmware image struct
 * 
 * @return true when successful, otherwise false
 */
bool tpm_check_firmware_update(struct TPMConnection **connection, struct TPMInformation **info, struct TPMFirmwareImageFile *image){
    if(connection == NULL || *connection == NULL || info == NULL || *info == NULL || image == NULL){
        std::cerr << "Error: Invalid argument in tpm_check_firmware_update" << std::endl;
        return false;
    }

    struct TPMFirmwareImage *firmware_image = &image->image;

    if((*connection)->config.verbose_print){
        std::cout << "CRC Check" << std::endl;
    }
    
    if(!tpm_update_check_image_crc(image)){
        std::cerr << "Error: Firmware image CRC check failed" << std::endl;
        return false;
    }

    if((*info)->manufacturer != INFINEON_MANUFACTORY_CODE){
        std::cerr << "Error: Unsupported TPM manufacturer " << big_endian_to_host((*info)->manufacturer) << std::endl;
        return false;
    }

    if((*info)->type != INFINEON_SLB9670_TYPE && (*info)->type != INFINEON_SLB9672_TYPE){
        std::cerr << "Error: Unsupported TPM type" << std::endl;
        return false;
    }

    if(firmware_image->rgbFirmware == NULL || firmware_image->unFirmwareSize == 0){
        std::cerr << "Error: Firmware image has no firmware data" << std::endl;
        return false;
    }

    if((*info)->upgrade_counter == 0){
        std::cerr << "Error: TPM has no upgrades left" << std::endl;
        return false;
    }

    if((*info)->type == INFINEON_SLB9670_TYPE){
        return tpm_update_check_slb9670(connection, info, firmware_image);
    }

    return tpm_update_check_slb9672(connection, info, firmware_image);
}


/*****************************************************************************************************/
// Updater functions
/*****************************************************************************************************/

/**
 * @brief Starts the firmware update process by sending the Field_Upgrade_Start_Vendor command to the TPM
 * 
 * @param connection The TPM connection struct
 * 
 * @param policy_handle The policy handle to be used for the command
 * 
 * @param command The vendor command to be sent
 * 
 * @param data The vendor data to be sent
 * 
 * @param data_size The size of the vendor data to be sent
 * 
 * @return true when successful, otherwise false
 */
bool tpm_field_upgrade_start_vendor(struct TPMConnection **connection, TPM2_HANDLE policy_handle, uint8_t command, uint8_t *data, uint16_t data_size){
    if(connection == NULL || *connection == NULL || policy_handle == 0 || data == NULL || data_size == 0){
        std::cerr << "Error: Invalid argument in tpm_field_upgrade_start_vendor" << std::endl;
        return false;
    }

    if((*connection)->config.verbose_print){
        std::cout << "Info: Starting Field_Upgrade_Start_Vendor command" << std::endl;
    }

    uint8_t command_buf[TPM2_MAX_COMMAND_SIZE] = {0};
    uint8_t response_buf[TPM2_MAX_RESPONSE_SIZE] = {0};
    size_t offset = 0;

    // TPM2 header
    if(!tpm_update_write_uint16(command_buf, sizeof(command_buf), &offset, TPM2_ST_SESSIONS)){
        std::cerr << "Error: Failed to write command header" << std::endl;
        return false;
    }
    size_t command_size_offset = offset;
    if(!tpm_update_write_uint32(command_buf, sizeof(command_buf), &offset, 0)){ // Placeholder for command size
        std::cerr << "Error: Failed to write command header" << std::endl;
        return false;
    }
    if(!tpm_update_write_uint32(command_buf, sizeof(command_buf), &offset, TPM2_CC_Field_Upgrade_Start_Vendor)){
        std::cerr << "Error: Failed to write command header" << std::endl;
        return false;
    }

    // Handle area
    if(!tpm_update_write_uint32(command_buf, sizeof(command_buf), &offset, TPM2_RH_PLATFORM)){
        std::cerr << "Error: Failed to write command handle" << std::endl;
        return false;
    }

    // Authorization area
    if(!tpm_update_write_uint32(command_buf, sizeof(command_buf), &offset, 9)){
        std::cerr << "Error: Failed to write command authorization handle" << std::endl;
        return false;
    }
    if(!tpm_update_write_uint32(command_buf, sizeof(command_buf), &offset, policy_handle)){
        std::cerr << "Error: Failed to write command authorization handle" << std::endl;
        return false;
    }
    if(!tpm_update_write_uint16(command_buf, sizeof(command_buf), &offset, 0)){
        std::cerr << "Error: Failed to write command authorization handle" << std::endl;
        return false;
    }
    if(!tpm_update_write_uint8(command_buf, sizeof(command_buf), &offset, 0)){
        std::cerr << "Error: Failed to write command authorization handle" << std::endl;
        return false;
    }
    if(!tpm_update_write_uint16(command_buf, sizeof(command_buf), &offset, 0)){
        std::cerr << "Error: Failed to write command authorization handle" << std::endl;
        return false;
    }

    // Vendor parameters
    if(!tpm_update_write_uint8(command_buf, sizeof(command_buf), &offset, command)){
        std::cerr << "Error: Failed to write vendor command" << std::endl;
        return false;
    }
    if(!tpm_update_write_uint16(command_buf, sizeof(command_buf), &offset, data_size)){
        std::cerr << "Error: Failed to write vendor data size" << std::endl;
        return false;
    }
    
    if(offset + data_size > sizeof(command_buf)){
        std::cerr << "Error: Vendor data size is too large" << std::endl;
        return false;
    }

    memcpy(command_buf + offset, data, data_size);
    offset += data_size;

    if(!tpm_update_write_uint32(command_buf, sizeof(command_buf), &command_size_offset, (uint32_t)offset)){
        std::cerr << "Error: Failed to write command size" << std::endl;
        return false;
    }

    size_t response_size = sizeof(response_buf);
    TPM2_RC response_code = 0;

    return tpm_update_transmit(connection, command_buf, offset, response_buf, &response_size, &response_code);
}

/*****************************************************************************************************/
// SLB9670 update functions
/*****************************************************************************************************/

/**
 * @brief Sends the SLB9670 firmware info request command to the TPM and retrieves the maximum block size and operation mode
 * 
 * @param connection The TPM connection struct
 * 
 * @param max_block_size The maximum block size to be returned
 * 
 * @param operation_mode The operation mode to be returned
 * 
 * @return true when successful, otherwise false
 */
bool tpm_slb9670_info_request2(struct TPMConnection **connection, uint16_t *max_block_size, uint16_t *operation_mode){
    if(connection == NULL || *connection == NULL || max_block_size == NULL || operation_mode == NULL){
        std::cerr << "Error: Invalid argument in tpm_slb9670_info_request2" << std::endl;
        return false;
    }

    uint8_t command_buf[32] = {0};
    uint8_t response_buf[TPM2_MAX_RESPONSE_SIZE] = {0};
    size_t offset = 0;

    if(!tpm_update_write_uint16(command_buf, sizeof(command_buf), &offset, SLB9670_FU_COMMAND_TAG)){
        std::cerr << "Error: Failed to write command" << std::endl;
        return false;
    }
    size_t command_size_offset = offset;
    if(!tpm_update_write_uint32(command_buf, sizeof(command_buf), &offset, 0)){ // Placeholder for command size
        std::cerr << "Error: Failed to write command size" << std::endl;
        return false;
    }
    if(!tpm_update_write_uint32(command_buf, sizeof(command_buf), &offset, SLB9670_FU_COMMAND_CODE)){
        std::cerr << "Error: Failed to write command" << std::endl;
        return false;
    }
    if(!tpm_update_write_uint8(command_buf, sizeof(command_buf), &offset, SLB9670_FU_INFO_REQUEST2)){
        std::cerr << "Error: Failed to write command" << std::endl;
        return false;
    }
    if(!tpm_update_write_uint16(command_buf, sizeof(command_buf), &offset, 0)){
        std::cerr << "Error: Failed to write command" << std::endl;
        return false;
    }

    if(!tpm_update_write_uint32(command_buf, sizeof(command_buf), &command_size_offset, (uint32_t)offset)){
        std::cerr << "Error: Failed to write command size" << std::endl;
        return false;
    }

    size_t response_size = sizeof(response_buf);
    TPM2_RC response_code = 0;

    if(!tpm_update_transmit(connection, command_buf, offset, response_buf, &response_size, &response_code)){
        return false;
    }

    if(response_size < 12){
        std::cerr << "Error: Invalid response size" << std::endl;
        return false;
    }

    uint16_t info_size = tpm_update_read_uint16(response_buf + 10);
    if((size_t)info_size + 12 > response_size){
        std::cerr << "Error: Invalid info size" << std::endl;
        return false;
    }

    struct TPMFirmwareReader reader = {
        response_buf + 12,
        info_size,
        0
    };

    uint16_t ignored16 = 0;
    uint32_t ignored32 = 0;
    uint16_t package_entries = 0;

    // sSecurityModuleLogicInfo_d.internal1
    if(!reader_uint16(&reader, &ignored16)){ 
        return false;
    }
    // sSecurityModuleLogicInfo_d.wMaxDataSize
    if(!reader_uint16(&reader, max_block_size)){
        return false;
    }
    // sSecurityModuleLogicInfo_d.sSecurityModuleLogic_d.internal1
    if(!reader_uint16(&reader, &ignored16)){
        return false;
    }
    // sSecurityModuleLogicInfo_d.sSecurityModuleLogic_d.unTargetHardwareIdentifier
    if(!reader_uint32(&reader, &ignored32)){
        return false;
    }
    // sSecurityModuleLogicInfo_d.sSecurityModuleLogic_d.internal3[34]
    if(!reader_skip(&reader, 34)){
        return false;
    }
    // sSecurityModuleLogicInfo_d.sSecurityModuleLogic_d.sFirmwarePackage_d (3 32 bit values)
    if(!reader_skip(&reader, 12)){
        return false;
    }
    // sSecurityModuleLogicInfo_d.sSecurityModuleLogic_d.sFirmwareConfiguration.wEntries
    if(!reader_uint16(&reader, &package_entries)){
        return false;
    }

    if(package_entries > MAX_NUM_FW_PACKAGES_DEVICE){
        return false;
    }
    // Firmware package entries (12 bytes each)
    if(!reader_skip(&reader, (size_t)package_entries * 12)){
        return false;
    }
    // sSecurityModuleLogicInfo_d.SecurityModuleStatus
    if(!reader_uint16(&reader, operation_mode)){
        return false;
    }

    return true;
}

/**
 * @brief Waits for the SLB9670 TPM to enter bootloader mode by sending the info request command and checking the operation mode
 * 
 * @param connection The TPM connection struct
 * 
 * @param info The TPM information struct
 * 
 * @return true when successful, otherwise false
 */
bool tpm_slb9670_wait_for_bootloader(struct TPMConnection **connection, struct TPMInformation **info){
    if(connection == NULL || *connection == NULL || info == NULL || *info == NULL){
        std::cerr << "Error: Invalid argument in tpm_slb9670_wait_for_bootloader" << std::endl;
        return false;
    }

    if((*connection)->config.verbose_print){
        std::cout << "Info: Waiting for TPM to enter bootloader mode" << std::endl;
    }

    for(size_t retry = 0; retry < TPM_FU_START_MAX_RETRIES; retry++){
        uint16_t max_block_size = 0;
        uint16_t operation_mode = 0;

        if(tpm_slb9670_info_request2(connection, &max_block_size, &operation_mode)){
            if(operation_mode == SMS_BTLDR_ACTIVE){
                (*info)->max_block_size = max_block_size;
                (*info)->operation_mode = operation_mode;
                return true;
            }
        }
        usleep(TPM_FU_START_RETRY_WAIT_TIME);
    }

    return false;
}

/**
 * @brief Calculates the LRC for the given data
 * 
 * @param data The data to calculate the LRC for
 * 
 * @param size The size of the data
 * 
 * @return The calculated LRC value
 */
uint8_t tpm_slb9670_calculate_lrc(uint8_t *data, size_t size){
    uint8_t lrc = 0;

    if(data == NULL){
        return lrc;
    }

    for(size_t i = 0; i < size; i++){
        lrc ^= data[i];
    }

    return lrc;
}

/**
 * @brief Sends a block of firmware data to the SLB9670 TPM using the Field_Upgrade_Update command
 * 
 * @param connection The TPM connection struct
 * 
 * @param block The block of firmware data to send
 * 
 * @param block_size The size of the block of firmware data to send
 * 
 * @return true when successful, otherwise false
 */
bool tpm_slb9670_send_block(struct TPMConnection **connection, uint8_t *block, uint16_t block_size){
    if(connection == NULL || *connection == NULL || block == NULL || block_size == 0){
        std::cerr << "Error: Invalid argument in tpm_slb9670_send_block" << std::endl;
        return false;
    }

    uint8_t command_buf[TPM2_MAX_COMMAND_SIZE] = {0};
    uint8_t response_buf[TPM2_MAX_RESPONSE_SIZE] = {0};
    size_t offset = 0;

    if(!tpm_update_write_uint16(command_buf, sizeof(command_buf), &offset, SLB9670_FU_COMMAND_TAG)){
        return false;
    }
    size_t command_size_offset = offset;
    if(!tpm_update_write_uint32(command_buf, sizeof(command_buf), &offset, 0)){
        return false;
    }
    if(!tpm_update_write_uint32(command_buf, sizeof(command_buf), &offset, SLB9670_FU_COMMAND_CODE)){
        return false;
    }

    size_t lrc_start = offset;
    if(!tpm_update_write_uint8(command_buf, sizeof(command_buf), &offset, SLB9670_FU_UPDATE)){
        return false;
    }
    if(!tpm_update_write_uint16(command_buf, sizeof(command_buf), &offset, block_size)){
        return false;
    }

    if(offset + block_size + 1> sizeof(command_buf)){
        return false;
    }

    memcpy(command_buf + offset, block, block_size);
    offset += block_size;

    uint8_t lrc = tpm_slb9670_calculate_lrc(command_buf + lrc_start, offset - lrc_start);
    if(!tpm_update_write_uint8(command_buf, sizeof(command_buf), &offset, lrc)){
        return false;
    }

    if(!tpm_update_write_uint32(command_buf, sizeof(command_buf), &command_size_offset, (uint32_t)offset)){
        return false;
    }

    size_t response_size = sizeof(response_buf);
    TPM2_RC response_code = 0;

    return tpm_update_transmit(connection, command_buf, offset, response_buf, &response_size, &response_code);
}

/**
 * @brief Sends the entire firmware image to the SLB9670 TPM in blocks using the Field_Upgrade_Update command
 * 
 * @param connection The TPM connection struct
 * 
 * @param info The TPM information struct
 * 
 * @param image The firmware image struct
 * 
 * @return true when successful, otherwise false
 */
bool tpm_slb9670_send_firmware(struct TPMConnection **connection, struct TPMInformation **info, struct TPMFirmwareImage *image){
    if(connection == NULL || *connection == NULL || info == NULL || *info == NULL || image == NULL || image->rgbFirmware == NULL || image->unFirmwareSize == 0){
        std::cerr << "Error: Invalid argument in tpm_slb9670_send_firmware" << std::endl;
        return false;
    }

    if((*connection)->config.verbose_print){
        std::cout << "Info: Starting to send firmware in blocks" << std::endl;
    }

    if((*info)->max_block_size == 0){
        std::cerr << "Error: Invalid max block size" << std::endl;
        return false;
    }

    uint32_t offset = 0;
    while(offset < image->unFirmwareSize){
        if((*connection)->config.verbose_print){
            std::cout << "Info: Preparing to send firmware block at offset " << offset << "/" << image->unFirmwareSize << std::endl;
        }

        uint32_t remaining = image->unFirmwareSize - offset;
        uint16_t block_size = (uint16_t)std::min<uint32_t>(remaining, (*info)->max_block_size);

        if(!tpm_slb9670_send_block(connection, image->rgbFirmware + offset, block_size)){
            std::cerr << "Error: Failed to send firmware block" << std::endl;
            return false;
        }
        offset += block_size;
    }

    return true;
}

/**
 * @brief Completes the firmware update process by sending the Field_Upgrade_Complete command to the SLB9670
 * 
 * @param connection The TPM connection struct
 * 
 * @return true when successful, otherwise false
 */
bool tpm_slb9670_complete(struct TPMConnection **connection){
    if(connection == NULL || *connection == NULL){
        std::cerr << "Error: Invalid argument in tpm_slb9670_complete" << std::endl;
        return false;
    }

    if((*connection)->config.verbose_print){
        std::cout << "Info: Completing firmware update" << std::endl;
    }

    uint8_t command_buf[32] = {0};
    uint8_t response_buf[TPM2_MAX_RESPONSE_SIZE] = {0};
    size_t offset = 0;

    if(!tpm_update_write_uint16(command_buf, sizeof(command_buf), &offset, SLB9670_FU_COMMAND_TAG)){
        return false;
    }
    size_t command_size_offset = offset;
    if(!tpm_update_write_uint32(command_buf, sizeof(command_buf), &offset, 0)){
        return false;
    }
    if(!tpm_update_write_uint32(command_buf, sizeof(command_buf), &offset, SLB9670_FU_COMMAND_CODE)){
        return false;
    }

    size_t lrc_start = offset;
    if(!tpm_update_write_uint8(command_buf, sizeof(command_buf), &offset, SLB9670_FU_COMPLETE)){
        return false;
    }
    if(!tpm_update_write_uint16(command_buf, sizeof(command_buf), &offset, 1)){
        return false;
    }
    if(!tpm_update_write_uint8(command_buf, sizeof(command_buf), &offset, 0)){
        return false;
    }

    uint8_t lrc = tpm_slb9670_calculate_lrc(command_buf + lrc_start, offset - lrc_start);
    if(!tpm_update_write_uint8(command_buf, sizeof(command_buf), &offset, lrc)){
        return false;
    }

    if(!tpm_update_write_uint32(command_buf, sizeof(command_buf), &command_size_offset, (uint32_t)offset)){
        return false;
    }

    size_t response_size = sizeof(response_buf);
    TPM2_RC response_code = 0;

    if(!tpm_update_transmit(connection, command_buf, offset, response_buf, &response_size, &response_code)){
        return false;
    }

    if(response_size < 12 || tpm_update_read_uint16(response_buf + 10) != 0){
        std::cerr << "Error: Invalid response size" << std::endl;
        return false;
    }

    usleep(TPM_FU_COMPLETE_WAIT_TIME);

    return true;
}

/*****************************************************************************************************/
// SLB9672 update functions
/*****************************************************************************************************/

/**
 * @brief Waits for the SLB9672 to enter update mode by sending the info request command and checking the operation mode
 * 
 * @param connection The TPM connection struct
 * 
 * @param info The TPM information struct
 * 
 * @return true when successful, otherwise false
 */
bool tpm_slb9672_wait_for_update_mode(struct TPMConnection **connection, struct TPMInformation **info){
    if(connection == NULL || *connection == NULL || info == NULL || *info == NULL){
        std::cerr << "Error: Invalid argument in tpm_slb9672_wait_for_update_mode" << std::endl;
        return false;
    }

    if((*connection)->config.verbose_print){
        std::cout << "Info: Checking if TPM is in update mode" << std::endl;
    }

    if((*info)->operation_mode != TPM_OPERATION_MODE_NORMAL){
        return true;
    }

    for(size_t retry = 0; retry < TPM20_FU_RETRY_COUNT; retry++){
        usleep(TPM_FU_START_RETRY_WAIT_TIME);

        if(!tpm_update_reconnect(connection)){
            continue;
        }

        uint16_t operation_mode = 0;
        if(!tpm_get_operation_mode(connection, &operation_mode)){
            continue;
        }

        if(operation_mode != TPM_OPERATION_MODE_NORMAL){
            (*info)->operation_mode = operation_mode;
            return true;
        }

    }
    return false;
}

/**
 * @brief Sends a block of firmware manifest data to the SLB9672 using the Field_Upgrade_Manifest_Vendor command
 * 
 * @param connection The TPM connection struct
 * 
 * @param processing_info The processing info byte to indicate the block type
 * 
 * @param block The firmware manifest data block
 * 
 * @param block_size The size of the firmware manifest data block
 * 
 * @return true when successful, otherwise false
 */
bool tpm_slb9672_send_manifest_block(struct TPMConnection **connection, uint8_t processing_info, uint8_t *block, uint16_t block_size){
    if(connection == NULL || *connection == NULL || block == NULL || block_size == 0){
        std::cerr << "Error: Invalid argument in tpm_slb9672_send_manifest_block" << std::endl;
        return false;
    }

    uint8_t command_buf[TPM2_MAX_COMMAND_SIZE] = {0};
    uint8_t response_buf[TPM2_MAX_RESPONSE_SIZE] = {0};
    size_t offset = 0;

    if(!tpm_update_write_uint16(command_buf, sizeof(command_buf), &offset, TPM2_ST_NO_SESSIONS)){
        return false;
    }
    size_t command_size_offset = offset;
    if(!tpm_update_write_uint32(command_buf, sizeof(command_buf), &offset, 0)){
        return false;
    }
    if(!tpm_update_write_uint32(command_buf, sizeof(command_buf), &offset, TPM2_CC_Field_Upgrade_Manifest_Vendor)){
        return false;
    }
    if(!tpm_update_write_uint8(command_buf, sizeof(command_buf), &offset, processing_info)){
        return false;
    }
    if(!tpm_update_write_uint16(command_buf, sizeof(command_buf), &offset, block_size)){
        return false;
    }

    if(offset + block_size > sizeof(command_buf)){
        return false;
    }

    memcpy(command_buf + offset, block, block_size);
    offset += block_size;

    if(!tpm_update_write_uint32(command_buf, sizeof(command_buf), &command_size_offset, (uint32_t)offset)){
        return false;
    }

    size_t response_size = sizeof(response_buf);
    TPM2_RC response_code = 0;

    return tpm_update_transmit(connection, command_buf, offset, response_buf, &response_size, &response_code);
}

/**
 * @brief Sends the entire firmware manifest to the SLB9672 in blocks using the Field_Upgrade_Manifest_Vendor command
 * 
 * @param connection The TPM connection struct
 * 
 * @param image The firmware image struct
 * 
 * @return true when successful, otherwise false
 */
bool tpm_slb9672_send_manifest(struct TPMConnection **connection, struct TPMFirmwareImage *image){
    if(connection == NULL || *connection == NULL || image == NULL || image->rgbPolicyParameterBlock == NULL || image->usPolicyParameterBlockSize == 0){
        std::cerr << "Error: Invalid argument in tpm_slb9672_send_manifest" << std::endl;
        return false;
    }

    if((*connection)->config.verbose_print){
        std::cout << "Info: Starting to send firmware manifest" << std::endl;
    }

    uint32_t offset = 0;
    uint32_t remaining = image->usPolicyParameterBlockSize;
    uint32_t block_number = 0;

    while(remaining > 0){

        if((*connection)->config.verbose_print){
            std::cout << "Info: Sending manifest block " << block_number << "/" << (image->usPolicyParameterBlockSize + TSS_MAX_DIGEST_BUFFER - 1) / TSS_MAX_DIGEST_BUFFER << std::endl;
        }

        uint16_t block_size = (uint16_t)std::min<uint32_t>(remaining, TSS_MAX_DIGEST_BUFFER);
        uint8_t processing_info = TPM20_FU_PROCESSING_FIRST_BLOCK;

        if(block_number > 0){
            processing_info = TPM20_FU_PROCESSING_CONSECUTIVE_BLOCK;
        }

        if(remaining <= TSS_MAX_DIGEST_BUFFER){
            processing_info = TPM20_FU_PROCESSING_LAST_BLOCK;
        }

        if(!tpm_slb9672_send_manifest_block(connection, processing_info, image->rgbPolicyParameterBlock + offset, block_size)){
            std::cerr << "Error: Failed to send manifest block" << std::endl;
            return false;
        }
        offset += block_size;
        remaining -= block_size;
        block_number++;
    }
    return true;
}

/**
 * @brief Sends a block of firmware data to the SLB9672 using the Field_Upgrade_Data_Vendor command
 * 
 * @param connection The TPM connection struct
 * 
 * @param block The firmware data block
 * 
 * @param block_size The size of the firmware data block
 * 
 * @return true when successful, otherwise false
 */
bool tpm_slb9672_send_block(struct TPMConnection **connection, uint8_t *block, uint16_t block_size){
    if(connection == NULL || *connection == NULL || block == NULL || block_size == 0){
        std::cerr << "Error: Invalid argument in tpm_slb9672_send_block" << std::endl;
        return false;
    }

    uint8_t command_buf[TPM2_MAX_COMMAND_SIZE] = {0};
    uint8_t response_buf[TPM2_MAX_RESPONSE_SIZE] = {0};
    size_t offset = 0;

    if(!tpm_update_write_uint16(command_buf, sizeof(command_buf), &offset, TPM2_ST_NO_SESSIONS)){
        return false;
    }
    size_t command_size_offset = offset;
    if(!tpm_update_write_uint32(command_buf, sizeof(command_buf), &offset, 0)){
        return false;
    }
    if(!tpm_update_write_uint32(command_buf, sizeof(command_buf), &offset, TPM2_CC_Field_Upgrade_Data_Vendor)){
        return false;
    }
    if(!tpm_update_write_uint16(command_buf, sizeof(command_buf), &offset, block_size)){
        return false;
    }

    if(offset + block_size > sizeof(command_buf)){
        return false;
    }

    memcpy(command_buf + offset, block, block_size);
    offset += block_size;

    if(!tpm_update_write_uint32(command_buf, sizeof(command_buf), &command_size_offset, (uint32_t)offset)){
        return false;
    }

    size_t response_size = sizeof(response_buf);
    TPM2_RC response_code = 0;

    return tpm_update_transmit(connection, command_buf, offset, response_buf, &response_size, &response_code);
}

/**
 * @brief Sends the entire firmware image to the SLB9672 in blocks using the Field_Upgrade_Data_Vendor command
 * 
 * @param connection The TPM connection struct
 * 
 * @param image The firmware image struct
 * 
 * @return true when successful, otherwise false
 */
bool tpm_slb9672_send_firmware(struct TPMConnection **connection, struct TPMFirmwareImage *image){
    if(connection == NULL || *connection == NULL || image == NULL || image->rgbFirmware == NULL || image->unFirmwareSize == 0){
        std::cerr << "Error: Invalid argument in tpm_slb9672_send_firmware" << std::endl;
        return false;
    }

    if((*connection)->config.verbose_print){
        std::cout << "Info: Starting to send firmware in blocks" << std::endl;
    }

    uint32_t offset = 0;
    while(offset < image->unFirmwareSize){

        if((*connection)->config.verbose_print){
            std::cout << "Info: Sending firmware block at offset " << offset << "/" << image->unFirmwareSize << std::endl;
        }

        uint32_t remaining = image->unFirmwareSize - offset;
        uint16_t block_size = (uint16_t)std::min<uint32_t>(remaining, TSS_MAX_DIGEST_BUFFER);

        if(!tpm_slb9672_send_block(connection, image->rgbFirmware + offset, block_size)){
            std::cerr << "Error: Failed to send firmware block" << std::endl;
            return false;
        }
        offset += block_size;
    }
    return true;
}

/**
 * @brief Waits for the SLB9672 to enter before finalize mode by sending the info request command and checking the operation mode
 * 
 * @param connection The TPM connection struct
 * 
 * @param info The TPM information struct
 * 
 * @return true when successful, otherwise false
 */
bool tpm_slb9672_wait_before_finalize(struct TPMConnection **connection, struct TPMInformation **info){
    if(connection == NULL || *connection == NULL || info == NULL || *info == NULL){
        std::cerr << "Error: Invalid argument in tpm_slb9672_wait_before_finalize" << std::endl;
        return false;
    }

    if((*connection)->config.verbose_print){
        std::cout << "Info: Waiting for TPM to enter before finalize mode" << std::endl;
    }

    for(size_t retry = 0; retry < TPM20_FU_RETRY_COUNT; retry++){
        usleep(TPM_FU_START_RETRY_WAIT_TIME);

        if((*connection)->config.verbose_print){
            std::cout << "Info: Retry " << retry + 1 << "/" << TPM20_FU_RETRY_COUNT << std::endl;
        }

        if(!tpm_update_reconnect(connection)){
            continue;
        }

        uint16_t operation_mode = 0;
        if(!tpm_get_operation_mode(connection, &operation_mode)){
            continue;
        }

        (*info)->operation_mode = operation_mode;

        if(operation_mode == TPM_OPERATION_MODE_BEFORE_FINALIZE || operation_mode == TPM_OPERATION_MODE_RECOVERY_BEFORE_FINALIZE){
            if((*connection)->config.verbose_print){
                std::cout << "Info: TPM entered before finalize mode" << std::endl;
            }
            return true;
        }
    }

    std::cerr << "Error: TPM did not enter before finalize mode after " << TPM20_FU_RETRY_COUNT << " retries" << std::endl;
    return false;
}

/**
 * @brief Finalizes the firmware update process by sending the Field_Upgrade_Finalize_Vendor command to the SLB9672
 * 
 * @param connection The TPM connection struct
 * 
 * @return true when successful, otherwise false
 */
bool tpm_slb9672_finalize(struct TPMConnection **connection){
    if(connection == NULL || *connection == NULL){
        std::cerr << "Error: Invalid argument in tpm_slb9672_finalize" << std::endl;
        return false;
    }

    if((*connection)->config.verbose_print){
        std::cout << "Info: Finalizing firmware update" << std::endl;
    }

    uint8_t command_buf[16] = {0};
    uint8_t response_buf[TPM_UPDATE_RESPONSE_BUFFER_SIZE] = {0};
    size_t offset = 0;

    if(!tpm_update_write_uint16(command_buf, sizeof(command_buf), &offset, TPM2_ST_NO_SESSIONS)){
        return false;
    }
    size_t command_size_offset = offset;
    if(!tpm_update_write_uint32(command_buf, sizeof(command_buf), &offset, 0)){
        return false;
    }
    if(!tpm_update_write_uint32(command_buf, sizeof(command_buf), &offset, TPM2_CC_Field_Upgrade_Finalize_Vendor)){
        return false;
    }
    if(!tpm_update_write_uint16(command_buf, sizeof(command_buf), &offset, 0)){
        return false;
    }

    if(!tpm_update_write_uint32(command_buf, sizeof(command_buf), &command_size_offset, (uint32_t)offset)){
        return false;
    }

    size_t response_size = sizeof(response_buf);
    TPM2_RC response_code = 0;

    return tpm_update_transmit(connection, command_buf, offset, response_buf, &response_size, &response_code);
}

/*****************************************************************************************************/
// Update process functions
/*****************************************************************************************************/

/**
 * @brief Starts the firmware update process by sending the appropriate command to the TPM based on its type
 * 
 * @param connection The TPM connection struct
 * 
 * @param info The TPM information struct
 * 
 * @param image The firmware image struct
 * 
 * @param policy_handle The policy handle to be used for the command
 * 
 * @return true when successful, otherwise false
 */
bool tpm_start_firmware_update(struct TPMConnection **connection, struct TPMInformation **info, struct TPMFirmwareImage *image, TPM2_HANDLE policy_handle){
    if(connection == NULL || *connection == NULL || info == NULL || *info == NULL || image == NULL || policy_handle == 0){
        std::cerr << "Error: Invalid argument in tpm_start_firmware_update" << std::endl;
        return false;
    }

    if((*connection)->config.verbose_print){
        std::cout << "Info: Starting firmware update for TPM type " << (*info)->type << std::endl;
    }

    if((*info)->type == INFINEON_SLB9670_TYPE){
        return tpm_field_upgrade_start_vendor(connection, policy_handle, TPM20_FieldUpgradeStart, image->rgbPolicyParameterBlock, image->usPolicyParameterBlockSize);
    }

    if((*info)->type == INFINEON_SLB9672_TYPE){
        uint8_t digest[TPM20_FIRMWARE_START_HASH_SIZE] = {0};
        uint16_t digest_size = sizeof(digest);

        if(!tpm_update_hash(connection, TPM2_ALG_SHA512, image->rgbPolicyParameterBlock, image->usPolicyParameterBlockSize, digest, &digest_size)){
            std::cerr << "Error: Failed to update hash" << std::endl;
            return false;
        }

        if(digest_size != TPM20_FIRMWARE_START_HASH_SIZE){
            std::cerr << "Error: Invalid digest size" << std::endl;
            return false;
        }

        uint8_t start_data[TPM20_FIRMWARE_START_HASH_SIZE + 2] = {0};
        start_data[0] = (uint8_t)((TPM2_ALG_SHA512 >> 8) & 0xFFU);
        start_data[1] = (uint8_t)(TPM2_ALG_SHA512 & 0xFFU);
        memcpy(start_data + 2, digest, digest_size);

        return tpm_field_upgrade_start_vendor(connection, policy_handle, TPM20_FieldUpgradeStartManifestHash, start_data, sizeof(start_data));
    }

    return false;
}

/**
 * @brief Transfers the firmware image to the TPM by sending the appropriate commands based on its type
 * 
 * @param connection The TPM connection struct
 * 
 * @param info The TPM information struct
 * 
 * @param image The firmware image struct
 * 
 * @return true when successful, otherwise false
 */
bool tpm_transfer_firmware(struct TPMConnection **connection, struct TPMInformation **info, struct TPMFirmwareImage *image){
    if(connection == NULL || *connection == NULL || info == NULL || *info == NULL || image == NULL){
        std::cerr << "Error: Invalid argument in tpm_transfer_firmware" << std::endl;
        return false;
    }

    if((*connection)->config.verbose_print){
        std::cout << "Info: Preparing to transfer firmware to TPM type " << (*info)->type << std::endl;
    }

    if((*info)->type == INFINEON_SLB9670_TYPE){
        if(!tpm_slb9670_wait_for_bootloader(connection, info)){
            return false;
        }
        return tpm_slb9670_send_firmware(connection, info, image);
    }

    if((*info)->type == INFINEON_SLB9672_TYPE){
        if(!tpm_slb9672_wait_for_update_mode(connection, info)){
            return false;
        }

        if((*info)->operation_mode == TPM_OPERATION_MODE_BEFORE_FINALIZE || (*info)->operation_mode == TPM_OPERATION_MODE_RECOVERY_BEFORE_FINALIZE){
            return true;
        }

        if((*connection)->config.verbose_print){
            std::cout << "Info: Sending firmware manifest" << std::endl;
        }
        if(!tpm_slb9672_send_manifest(connection, image)){
            return false;
        }

        if((*connection)->config.verbose_print){
            std::cout << "Info: Sending firmware image" << std::endl;
        }
        return tpm_slb9672_send_firmware(connection, image);
    }
    return false;
}

/**
 * @brief Finalizes the firmware update process by sending the appropriate command to the TPM based on its type
 * 
 * @param connection The TPM connection struct
 * 
 * @param info The TPM information struct
 * 
 * @return true when successful, otherwise false
 */
bool tpm_finalize_firmware_update(struct TPMConnection **connection, struct TPMInformation **info){
    if(connection == NULL || *connection == NULL || info == NULL || *info == NULL){
        std::cerr << "Error: Invalid argument in tpm_finalize_firmware_update" << std::endl;
        return false;
    }

    if((*info)->type == INFINEON_SLB9670_TYPE){
        return tpm_slb9670_complete(connection);
    }

    if((*info)->type == INFINEON_SLB9672_TYPE){
        if(!tpm_slb9672_wait_before_finalize(connection, info)){
            return false;
        }
        return tpm_slb9672_finalize(connection);
    }

    return false;
}

/**
 * @brief Abandons the firmware update process for the SLB9672 TPM by sending the Field_Upgrade_Abandon_Vendor command
 * 
 * @param connection The TPM connection struct
 * 
 * @return true when successful, otherwise false
 */
bool tpm_slb9672_abandon_firmware_update(struct TPMConnection **connection){
    if(connection == NULL || *connection == NULL || (*connection)->tcti_ctx == NULL){
        std::cerr << "Error: Invalid argument in tpm_slb9672_abandon_firmware_update" << std::endl;
        return false;
    }

    if((*connection)->config.verbose_print){
        std::cout << "Info: Abandoning firmware update" << std::endl;
    }

    uint8_t command_buf[16] = {0};
    uint8_t response_buf[TPM_UPDATE_RESPONSE_BUFFER_SIZE] = {0};

    size_t offset = 0;

    if(!tpm_update_write_uint16(command_buf, sizeof(command_buf), &offset, TPM2_ST_NO_SESSIONS)){
        return false;
    }

    size_t command_size_offset = offset;
    if(!tpm_update_write_uint32(command_buf, sizeof(command_buf), &offset, 0)){
        return false;
    }

    if(!tpm_update_write_uint32(command_buf, sizeof(command_buf), &offset, TPM2_CC_Field_Upgrade_Abandon_Vendor)){
        return false;
    }

    if(!tpm_update_write_uint16(command_buf, sizeof(command_buf), &offset, 0)){
        return false;
    }

    /*
     * Write actual command size into the header.
     *
     * This will be 12 bytes.
     */
    if(!tpm_update_write_uint32(command_buf, sizeof(command_buf), &command_size_offset, (uint32_t)offset)){
        return false;
    }

    size_t response_size = sizeof(response_buf);
    TPM2_RC response_code = 0;

    std::cout << "Sending TPM2_FieldUpgradeAbandonVendor" << std::endl;

    if(!tpm_update_transmit(connection, command_buf, offset, response_buf, &response_size, &response_code)){
        std::cerr << "Error: TPM2_FieldUpgradeAbandonVendor failed" << std::endl;
        return false;
    }

    usleep(300000);

    std::cout << "Firmware update successfully abandoned." << std::endl;
    std::cout << "SLB9672 now requires a hardware reset / power cycle." << std::endl;
    return true;
}

/**
 * @brief Updates the firmware of the TPM by performing the entire update process:
 * 1. Check if the firmware update is possible 
 * 2. Prepare the firmware update policy 
 * 3. Start the firmware update 
 * 4. Transfer the firmware image 
 * 5. Finalize the firmware update
 * 
 * @param connection The TPM connection struct
 * 
 * @param info The TPM information struct
 * 
 * @param image The firmware image file struct
 * 
 * @return true when successful, otherwise false
 */
bool tpm_update_firmware(struct TPMConnection **connection, struct TPMInformation **info, struct TPMFirmwareImageFile *image){
    if(connection == NULL || *connection == NULL || info == NULL || *info == NULL || image == NULL){
        std::cerr << "Error: Invalid argument in tpm_update_firmware" << std::endl;
        return false;
    }

    std::cout << "Checking firmware update" << std::endl;
    if(!tpm_check_firmware_update(connection, info, image)){
        return false;
    }

    struct TPMFirmwareImage *firmware_image = &image->image;
    bool update_started = false;

    if((*info)->type == INFINEON_SLB9670_TYPE){
        update_started = ((*info)->operation_mode == SMS_FWCONFIG_ACTIVE);
    } else if((*info)->type == INFINEON_SLB9672_TYPE){
        update_started = ((*info)->operation_mode == TPM_OPERATION_MODE_NORMAL);
    }

    if(update_started){
        ESYS_TR policy_session = ESYS_TR_NONE;
        TPM2_HANDLE policy_handle = 0;

        if((*connection)->config.verbose_print){
            std::cout << "Preparing firmware update policy" << std::endl;
        }
        if(!tpm_prepare_firmware_update_policy(connection, &policy_session, &policy_handle)){
            return false;
        }

        if((*connection)->config.verbose_print){
            std::cout << "Starting firmware update" << std::endl;
        }
        if(!tpm_start_firmware_update(connection, info, firmware_image, policy_handle)){
            if(policy_session != ESYS_TR_NONE){
                Esys_FlushContext((*connection)->esys_ctx, policy_session);
            }
            return false;
        }
    }

    if((*connection)->config.verbose_print){
        std::cout << "Transferring firmware" << std::endl;
    }
    if(!tpm_transfer_firmware(connection, info, firmware_image)){
        return false;
    }

    if((*connection)->config.verbose_print){
        std::cout << "Finalizing firmware update" << std::endl;
    }
    if(!tpm_finalize_firmware_update(connection, info)){
        return false;
    }

    if((*connection)->config.verbose_print){
        std::cout << "Resetting TPM" << std::endl;
    }
    // Reset the tpm or restart it
    Esys_Startup((*connection)->esys_ctx, TPM2_SU_CLEAR);
    // Esys_Shutdown((*connection)->esys_ctx, TPM2_SU_CLEAR);
    return true;
}
