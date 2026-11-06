/**
 * @brief Declares types and definitons
 * @author Jan Haeussermann
 * 
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "tpm2_types.h"

/*****************************************************************************************************/
// Vendor specific values needed
// Copied from the Infineon TPMUpdater from TPM2_VendorTypes
/*****************************************************************************************************/
typedef uint32_t                              	TSS_TPM_RC;
#define TSS_RC_FMT1                             (TSS_TPM_RC)(0x080)
#define RC_TPM_MASK 							0xE0280000U
#define TSS_TPM_RC_HIERARCHY 					(TSS_TPM_RC)(TSS_RC_FMT1 + 0x005)
#define TSS_TPM_RC_1 							(TSS_TPM_RC)(0x100)

/**
 * Table 78 - Definition of TPM2B_MAX_BUFFER Structure
 */
/**
 *  @brief      TPM2B_MAX_BUFFER structure
 *  @details    TPM2B_MAX_BUFFER structure definition
 */
struct TSS_TPM2B_MAX_BUFFER {
    /// Size of the buffer
    uint16_t size;
    /// The operand
    /// NOTE    MAX_DIGEST_BUFFER is TPM-dependent but is required to be at least 1,024.
    uint8_t buffer[TSS_MAX_DIGEST_BUFFER];
};

#pragma pack(push, 1)
struct IFX_Vendor_Property{
	uint32_t count;
	uint16_t size;
	uint8_t buffer[TSS_MAX_DIGEST_BUFFER];
};
#pragma pack(pop)

/*****************************************************************************************************/
// Getter functions
/*****************************************************************************************************/

/**
 * @brief Returns the manufacturer of the TPM
 * 
 * @param connection information to connect to the TPM
 * 
 * @param manufacturer of the TPM
 * 
 * @return returns true when successful, otherwise false
 */
bool tpm_get_manufacturer(struct TPMConnection **connection, uint32_t *manufacturer);

/**
 * @brief Returns the type of the TPM
 * 
 * @param connection information to connect to the TPM
 * 
 * @param type of the TPM
 * 
 * @return returns true when successful, otherwise false
 */
bool tpm_get_type(struct TPMConnection **connection, uint64_t *type);

/**
 * @brief Returns the firmware version of the TPM
 * 
 * @param connection information to connect to the TPM
 * 
 * @param firmware_version of the TPM
 * 
 * @return returns true when successful, otherwise false
 */
bool tpm_get_firmware_version(struct TPMConnection **connection, uint64_t *firmware_version);

/**
 * @brief Returns the upgrade counter of the TPM
 * 
 * @param connection information to connect to the TPM
 * 
 * @param counter of the TPM to update the firmware
 * 
 * @return returns true when successful, otherwise false
 */
bool tpm_get_upgrade_counter(struct TPMConnection **connection, uint16_t *counter);

/**
 * @brief Returns the upgrade counter with the same firmware of the TPM
 * 
 * @param connection information to connect to the TPM
 * 
 * @param counter of the TPM to update the firmware
 * 
 * @return returns true when successful, otherwise false
 */
bool tpm_get_upgrade_counter_same(struct TPMConnection **connection, uint16_t *counter);

/**
 * @brief Returns the operation mode of the TPM
 * 
 * @param connection information to connect to the TPM
 * 
 * @param mode of the TPM to update the firmware
 * 
 * @return returns true when successful, otherwise false
 */
bool tpm_get_operation_mode(struct TPMConnection **connection, uint16_t *mode);

/**
 * @brief Returns the max block size of the TPM
 * 
 * @param connection information to connect to the TPM
 * 
 * @param max_block_size of the TPM to update the firmware
 * 
 * @return returns true when successful, otherwise false
 * 
 */
bool tpm_get_max_block_size(struct TPMConnection **connection, uint16_t *max_block_size);

/**
 * @brief Returns the target hardware identifier of the TPM
 * 
 * @param connection information to connect to the TPM
 * 
 * @param target_hardware_identifier of the TPM to update the firmware
 * 
 * @return returns true when successful, otherwise false
 */
bool tpm_get_target_hardware_identifier(struct TPMConnection **connection, uint32_t *target_hardware_identifier);

/**
 * @brief Returns the decrypt keys of the TPM
 * 
 * @param connection information to connect to the TPM
 * 
 * @param decrypt_key_count of the TPM to update the firmware
 * 
 * @param decrypt_key_ids of the TPM to update the firmware
 * 
 * @return returns true when successful, otherwise false
 */
bool tpm_get_decrypt_keys(struct TPMConnection **connection, uint16_t *decrypt_key_count, uint32_t *decrypt_key_ids);

/**
 * @brief Returns the active firmware version of the TPM
 * 
 * @param connection information to connect to the TPM
 * 
 * @param active_firmware_version of the TPM to update the firmware
 * 
 * @return returns true when successful, otherwise false
 */
bool tpm_get_active_firmware_version(struct TPMConnection **connection, uint32_t *active_firmware_version);

/**
 * @brief Returns the unique id of the firmware currently processed
 * 
 * @param connection information to connect to the TPM
 * 
 * @param unique_id of the firmware currently processed
 * 
 * @return returns true when successful, otherwise false
 */
bool tpm_get_process_firmware_unique_id(struct TPMConnection **connection, uint8_t *unique_id);

/**
 * @brief Returns the key group id of the TPM
 * 
 * @param connection information to connect to the TPM
 * 
 * @param key_group_id of the TPM to update the firmware
 * 
 * @return returns true when successful, otherwise false
 */
bool tpm_get_key_group_id(struct TPMConnection **connection, uint32_t *key_group_id);

/**
 * @brief Returns the firmware update properties of the TPM
 * 
 * @param connection information to connect to the TPM
 * 
 * @param firmware_update_properties of the TPM to update the firmware
 * 
 * @return returns true when successful, otherwise false
 */
bool tpm_get_firmware_update_properties(struct TPMConnection **connection, uint32_t *firmware_update_properties);

/**
 * @brief Returns the start hash of the TPM
 * 
 * @param connection information to connect to the TPM
 * 
 * @param algorithm of the TPM to update the firmware
 * 
 * @param hash_size of the TPM to update the firmware
 * 
 * @param hash_digest of the TPM to update the firmware
 * 
 * @return returns true when successful, otherwise false
 */
bool tpm_get_start_hash(struct TPMConnection **connection, uint16_t *algorithm, uint16_t *hash_size, uint8_t hash_digest[64]);

/**
 * @brief Returns the vendor string of the TPM
 * 
 * @param connection information to connect to the TPM
 * 
 * @param vendor_string of the TPM to update the firmware
 * 
 * @return returns true when successful, otherwise false
 */
bool tpm_get_vendor_string(struct TPMConnection **connection, uint8_t vendor_string[16]);

/*****************************************************************************************************/
// Summarize function
/*****************************************************************************************************/

bool tpm_get_information(struct TPMConnection **connection, struct TPMInformation **info);