/**
 * @brief structs used by the firmware updater for the updates
 * @author Jan Haeussermann
 * 
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

/*****************************************************************************************************/
// Infineon firmware update command codes 
/*****************************************************************************************************/

#define TPM2_CC_Field_Upgrade_Start_Vendor                     0x2000012F
#define TPM2_CC_Field_Upgrade_Abandon_Vendor                   0x20000130
#define TPM2_CC_Field_Upgrade_Manifest_Vendor                  0x20000131
#define TPM2_CC_Field_Upgrade_Data_Vendor                      0x20000132
#define TPM2_CC_Field_Upgrade_Finalize_Vendor                  0x20000133

// SLB9670 used
#define TPM20_FieldUpgradeStart                                0x34

#define SLB9670_FU_COMMAND_TAG                                 0xC1
#define SLB9670_FU_COMMAND_CODE                                0xAA

#define SLB9670_FU_INFO_REQUEST2                               0x11
#define SLB9670_FU_UPDATE                                      0x31
#define SLB9670_FU_COMPLETE                                    0x32

#define SLB9670_TARGET_HARDWARE_IDENTIFIER              0x00000102U

// SLB9672 used
#define TPM20_FieldUpgradeStartManifestHash                    0x01

#define TPM20_FU_PROCESSING_LAST_BLOCK                         0x00
#define TPM20_FU_PROCESSING_FIRST_BLOCK                        0x01
#define TPM20_FU_PROCESSING_CONSECUTIVE_BLOCK                  0x02

#define TPM_UPDATE_RESPONSE_BUFFER_SIZE                        4096

/*****************************************************************************************************/
// Constants 
/*****************************************************************************************************/

static const uint8_t TPM_FW_UPDATE_POLICY_DIGEST[] = {
    0x6D, 0x9B, 0x4B, 0x75, 0x61, 0xCA, 0xC7, 0x7B,
    0x26, 0x1B, 0x31, 0xE2, 0x42, 0x31, 0xBD, 0x87,
    0x09, 0x7B, 0x47, 0x2E, 0x45, 0xF0, 0x7D, 0x4D,
    0x8F, 0xED, 0xDD, 0xB0, 0x4B, 0x91, 0x89, 0xF4
    };

#define TPM_FU_START_MAX_RETRIES                               16 
#define TPM_FU_START_RETRY_WAIT_TIME                           1000
#define TPM_FU_COMPLETE_WAIT_TIME                              2000

#define TPM20_FU_RETRY_COUNT                                   5
#define TPM20_FU_WAIT_TIME                                     1000


#define TPM20_FIRMWARE_UPDATE_POLICY_SIZE                      32
#define TPM20_FIRMWARE_START_HASH_SIZE                         64

#define TPM_FU_PROPERTIES_FW_RECOVERY_SUPPORTED                0x01

#define TPM_FIRMWARE_CAP_MATCH_UNIQUE_ID                       0x01
#define TPM_FIRMWARE_CAP_ALLOW_SAME_VERSION                    0x02
#define TPM_FIRMWARE_CAP_ALLOW_RECOVERY                        0x04

/*****************************************************************************************************/
// Structs
/*****************************************************************************************************/



/*****************************************************************************************************/
// Functions
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
bool tpm_update_write_uint8(uint8_t *buffer, size_t buffer_size, size_t *offset, uint8_t value);

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
bool tpm_update_write_uint16(uint8_t *buffer, size_t buffer_size, size_t *offset, uint16_t value);

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
bool tpm_update_write_uint32(uint8_t *buffer, size_t buffer_size, size_t *offset, uint32_t value);

/**
 * @brief Reads a uint8 from the buffer
 * 
 * @param buffer The buffer to read from
 * 
 * @return The uint8 value read from the buffer
 */
uint8_t tpm_update_read_uint8(uint8_t *buffer);

/**
 * @brief Reads a uint16 from the buffer
 * 
 * @param buffer The buffer to read from
 * 
 * @return The uint16 value read from the buffer
 */
uint16_t tpm_update_read_uint16(uint8_t *buffer);

/**
 * @brief Reads a uint32 from the buffer
 * 
 * @param buffer The buffer to read from
 * 
 * @return The uint32 value read from the buffer
 */
uint32_t tpm_update_read_uint32(uint8_t *buffer);

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
bool tpm_update_transmit(struct TPMConnection **connection, uint8_t *command_buf, size_t command_size, uint8_t *response_buf, size_t *response_size, TPM2_RC *response_code);

/**
 * @brief Calculate the CRC value of a given buffer
 * 
 * @param data buffer to check the value
 * 
 * @param data_size size of the buffer
 * 
 * @return 32 bit crc value
 */
uint32_t tpm_update_crc32(uint8_t *data, size_t data_size);

/**
 * @brief Calculate and checks the checksum of the image_blob
 * 
 * @param firmware struct of the firmware
 * 
 * @return true when correct crc, false otherwise
 */
bool tpm_update_check_image_crc(struct TPMFirmwareImageFile *firmware);

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
bool tpm_get_firmware_version_string(struct TPMInformation **info, char *version, char *version_short);

/**
 * @brief Checks if the source version of the firmware image is compatible with the current firmware version of the TPM
 * 
 * @param info struct of the TPM information
 * 
 * @param image struct of the firmware image
 * 
 * @return true when compatible, false otherwise
 */
bool tpm_update_check_source_version(struct TPMInformation **info, struct TPMFirmwareImage *image);

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
bool tpm_update_hash(struct TPMConnection **connection, TPMI_ALG_HASH algorithm, uint8_t *data, size_t data_size, uint8_t *digest, uint16_t *digest_size);

/**
 * @brief Gets a random nonce from the TPM 
 * 
 * @param connection The TPM connection struct
 * 
 * @param nonce The buffer to store the resulting nonce
 * 
 * @return true when successful, otherwise false
 */
bool tpm_update_get_random_nonce(struct TPMConnection **connection, TPM2B_DIGEST *nonce);

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
bool tpm_prepare_firmware_update_policy(struct TPMConnection **connection, ESYS_TR *policy_session, TPM2_HANDLE *policy_handle);

/**
 * @brief Reconnects to the TPM by finalizing the current connection and reinitializing 
 * 
 * @param connection The TPM connection struct
 * 
 * @return true when successful, otherwise false
 */
bool tpm_update_reconnect(struct TPMConnection **connection);

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
bool tpm_update_check_slb9670_policy(struct TPMConnection **connection, struct TPMInformation **info, struct TPMFirmwareImage *image);

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
bool tpm_update_check_slb9670(struct TPMConnection **connection, struct TPMInformation **info, struct TPMFirmwareImage *image);

/**
 * @brief Selects the appropriate firmware manifest from the firmware image based on the TPM information
 * 
 * @param info The TPM information struct
 * 
 * @param image The firmware image struct
 * 
 * @return true when successful, otherwise false
 */
bool tpm_select_firmware_manifest(struct TPMInformation **info, struct TPMFirmwareImage *image);

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
bool tpm_update_check_slb9672_start_hash(struct TPMConnection **connection, struct TPMInformation **info, struct TPMFirmwareImage *image);

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
bool tpm_update_check_slb9672(struct TPMConnection **connection, struct TPMInformation **info, struct TPMFirmwareImage *image);

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
bool tpm_check_firmware_update(struct TPMConnection **connection, struct TPMInformation **info, struct TPMFirmwareImageFile *image);

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
bool tpm_field_upgrade_start_vendor(struct TPMConnection **connection, TPM2_HANDLE policy_handle, uint8_t command, uint8_t *data, uint16_t data_size);

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
bool tpm_slb9670_info_request2(struct TPMConnection **connection, uint16_t *max_block_size, uint16_t *operation_mode);

/**
 * @brief Waits for the SLB9670 TPM to enter bootloader mode by sending the info request command and checking the operation mode
 * 
 * @param connection The TPM connection struct
 * 
 * @param info The TPM information struct
 * 
 * @return true when successful, otherwise false
 */
bool tpm_slb9670_wait_for_bootloader(struct TPMConnection **connection, struct TPMInformation **info);

/**
 * @brief Calculates the LRC for the given data
 * 
 * @param data The data to calculate the LRC for
 * 
 * @param size The size of the data
 * 
 * @return The calculated LRC value
 */
uint8_t tpm_slb9670_calculate_lrc(uint8_t *data, size_t size);

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
bool tpm_slb9670_send_block(struct TPMConnection **connection, uint8_t *block, uint16_t block_size);

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
bool tpm_slb9670_send_firmware(struct TPMConnection **connection, struct TPMInformation **info, struct TPMFirmwareImage *image);

/**
 * @brief Completes the firmware update process by sending the Field_Upgrade_Complete command to the SLB9670
 * 
 * @param connection The TPM connection struct
 * 
 * @return true when successful, otherwise false
 */
bool tpm_slb9670_complete(struct TPMConnection **connection);

/**
 * @brief Waits for the SLB9672 to enter update mode by sending the info request command and checking the operation mode
 * 
 * @param connection The TPM connection struct
 * 
 * @param info The TPM information struct
 * 
 * @return true when successful, otherwise false
 */
bool tpm_slb9672_wait_for_update_mode(struct TPMConnection **connection, struct TPMInformation **info);

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
bool tpm_slb9672_send_manifest_block(struct TPMConnection **connection, uint8_t processing_info, uint8_t *block, uint16_t block_size);

/**
 * @brief Sends the entire firmware manifest to the SLB9672 in blocks using the Field_Upgrade_Manifest_Vendor command
 * 
 * @param connection The TPM connection struct
 * 
 * @param image The firmware image struct
 * 
 * @return true when successful, otherwise false
 */
bool tpm_slb9672_send_manifest(struct TPMConnection **connection, struct TPMFirmwareImage *image);

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
bool tpm_slb9672_send_block(struct TPMConnection **connection, uint8_t *block, uint16_t block_size);

/**
 * @brief Sends the entire firmware image to the SLB9672 in blocks using the Field_Upgrade_Data_Vendor command
 * 
 * @param connection The TPM connection struct
 * 
 * @param image The firmware image struct
 * 
 * @return true when successful, otherwise false
 */
bool tpm_slb9672_send_firmware(struct TPMConnection **connection, struct TPMFirmwareImage *image);

/**
 * @brief Waits for the SLB9672 to enter before finalize mode by sending the info request command and checking the operation mode
 * 
 * @param connection The TPM connection struct
 * 
 * @param info The TPM information struct
 * 
 * @return true when successful, otherwise false
 */
bool tpm_slb9672_wait_before_finalize(struct TPMConnection **connection, struct TPMInformation **info);

/**
 * @brief Finalizes the firmware update process by sending the Field_Upgrade_Finalize_Vendor command to the SLB9672
 * 
 * @param connection The TPM connection struct
 * 
 * @return true when successful, otherwise false
 */
bool tpm_slb9672_finalize(struct TPMConnection **connection);

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
bool tpm_start_firmware_update(struct TPMConnection **connection, struct TPMInformation **info, struct TPMFirmwareImage *image, TPM2_HANDLE policy_handle);

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
bool tpm_transfer_firmware(struct TPMConnection **connection, struct TPMInformation **info, struct TPMFirmwareImage *image);

/**
 * @brief Finalizes the firmware update process by sending the appropriate command to the TPM based on its type
 * 
 * @param connection The TPM connection struct
 * 
 * @param info The TPM information struct
 * 
 * @return true when successful, otherwise false
 */
bool tpm_finalize_firmware_update(struct TPMConnection **connection, struct TPMInformation **info);

/**
 * @brief Abandons the firmware update process for the SLB9672 TPM by sending the Field_Upgrade_Abandon_Vendor command
 * 
 * @param connection The TPM connection struct
 * 
 * @return true when successful, otherwise false
 */
bool tpm_slb9672_abandon_firmware_update(struct TPMConnection **connection);

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
bool tpm_update_firmware(struct TPMConnection **connection, struct TPMInformation **info, struct TPMFirmwareImageFile *image);
