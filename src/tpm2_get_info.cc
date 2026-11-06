/**
 * @brief Prepares and starts the upgrade for the TPM
 * @author Jan Haeussermann
 * 
 */

#include <tss2/tss2_esys.h>
#include <tss2/tss2_sys.h>
#include <tss2/tss2_tpm2_types.h>
#include <tss2_tpm2_types.h>
#include <tss2/tss2_tcti_device.h>

#include <iostream>
#include <cstring>

#include "endian_helper.h"


#include "tpm2_get_info.h"
#include "tpm2_types.h"
#include "tpm2_file_parser.h"

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
bool tpm_get_manufacturer(struct TPMConnection **connection, uint32_t *manufacturer){
	TSS2_RC ret;
	TPMI_YES_NO moreData = TPM2_NO;
	TPMS_CAPABILITY_DATA *data = NULL;

	ret = Esys_GetCapability((*connection)->esys_ctx, ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE, 
				TPM2_CAP_TPM_PROPERTIES, TPM2_PT_MANUFACTURER, 1, &moreData, &data);
	if(ret != TSS2_RC_SUCCESS){
		std::cerr << "Error: TPM2_PT_MANUFACTURER" << std::endl;
		return false;
	}

	*manufacturer = data->data.tpmProperties.tpmProperty[0].value;
	Esys_Free(data);

	if((*connection)->config.verbose_print){
 	   std::cout << "Manufacturer: " << (uint8_t *) manufacturer << std::endl;
	}

	return true;
}

/**
 * @brief Returns the type of the TPM
 * 
 * @param connection information to connect to the TPM
 * 
 * @param type of the TPM
 * 
 * @return returns true when successful, otherwise false
 */
bool tpm_get_type(struct TPMConnection **connection, uint64_t *type){
	TSS2_RC ret;
	TPMI_YES_NO moreData = TPM2_NO;
	TPMS_CAPABILITY_DATA *data = NULL;

	ret = Esys_GetCapability((*connection)->esys_ctx, ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE, 
				TPM2_CAP_TPM_PROPERTIES, TPM2_PT_VENDOR_STRING_1, 1, &moreData, &data);
	if(ret != TSS2_RC_SUCCESS){
		std::cerr << "Error: TPM2_PT_VENDOR_STRING_1" << std::endl;
		return false;
	}
	
	*type = ((uint64_t) data->data.tpmProperties.tpmProperty->value) << 32;
	Esys_Free(data);

	ret = Esys_GetCapability((*connection)->esys_ctx, ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE, 
				TPM2_CAP_TPM_PROPERTIES, TPM2_PT_VENDOR_STRING_2, 1, &moreData, &data);
	if(ret != TSS2_RC_SUCCESS){
		std::cerr << "Error: TPM2_PT_VENDOR_STRING_1" << std::endl;
		return false;
	}

	*type |= ((uint64_t) data->data.tpmProperties.tpmProperty->value);
	Esys_Free(data);

	return true;
}

/**
 * @brief Returns the firmware version of the TPM
 * 
 * @param connection information to connect to the TPM
 * 
 * @param firmware_version of the TPM
 * 
 * @return returns true when successful, otherwise false
 */
bool tpm_get_firmware_version(struct TPMConnection **connection, uint64_t *firmware_version){
	TSS2_RC ret;
	TPMI_YES_NO moreData = TPM2_NO;
	TPMS_CAPABILITY_DATA *data = NULL;

	ret = Esys_GetCapability((*connection)->esys_ctx, ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE, 
				TPM2_CAP_TPM_PROPERTIES, TPM2_PT_FIRMWARE_VERSION_1, 1, &moreData, &data);
	if(ret != TSS2_RC_SUCCESS){
		std::cerr << "Error: Esys_GetCapability TPM2_PT_FIRMWARE_VERSION_1 failed" << std::endl;
		return false;
	}

	*firmware_version = ((uint64_t) data->data.tpmProperties.tpmProperty->value) << 32; 
	Esys_Free(data);

	ret = Esys_GetCapability((*connection)->esys_ctx, ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE, 
				TPM2_CAP_TPM_PROPERTIES, TPM2_PT_FIRMWARE_VERSION_2, 1, &moreData, &data);
	if(ret != TSS2_RC_SUCCESS){
		std::cerr << "Error: Esys_GetCapability TPM2_PT_FIRMWARE_VERSION_2 failed" << std::endl;
		return false;
	}

	*firmware_version |= ((uint64_t) data->data.tpmProperties.tpmProperty->value); 
	Esys_Free(data);

	if((*connection)->config.verbose_print){
		std::cout << "Firmware version: " << (*firmware_version >> 48) << "." << (0xFFFF & (*firmware_version >> 32)) << std::endl;
		std::cout << "Build number: 0x " << (0xFFFF & (*firmware_version >> 8)) << std::endl;
		std::cout << "Certification: " << (0xFF & (*firmware_version >> 24)) << std::endl; // 0x00 CC certified
	}

	return true;
}

/**
 * @brief Returns the upgrade counter of the TPM
 * 
 * @param connection information to connect to the TPM
 * 
 * @param counter of the TPM to update the firmware
 * 
 * @return returns true when successful, otherwise false
 */
bool tpm_get_upgrade_counter(struct TPMConnection **connection, uint16_t *counter){
	TSS2_RC ret;
	TPMI_YES_NO moreData = TPM2_NO;
	TPMS_CAPABILITY_DATA *data = NULL;


	uint64_t type = 0;
	if(!tpm_get_type(connection, &type)){
		std::cerr << "Error: failed to get type of TPM module" << std::endl;
		return false;
	}

	if(type == INFINEON_SLB9670_TYPE){
		ret = Esys_GetCapability((*connection)->esys_ctx, ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE,
						TPM2_CAP_VENDOR_PROPERTY, TPM_PT_VENDOR_FIX_SMLI2, 1, &moreData, &data);
		if(ret != TSS2_RC_SUCCESS || data == NULL){
			std::cerr << "Error: Failed to get property upgrade counter" << std::endl;
			return false;
		}

		if(data->data.vendor.size < 6){
			std::cerr << "Error: Vendor capability data size is too small" << std::endl;
			Esys_Free(data);
			return false;
		}

		IFX_Vendor_Property *vendor_property = (IFX_Vendor_Property *) data->data.vendor.buffer;;
		
		uint32_t count = big_endian_to_host(vendor_property->count);
		uint16_t size = big_endian_to_host(vendor_property->size);

		if(count != 1){
			std::cerr << "Error: Vendor capability data count is not 1" << std::endl;
			Esys_Free(data);
			return false;
		}

		if(data->data.vendor.size < size + 6){
			std::cerr << "Error: Vendor capability data size is too small for the structure" << std::endl;
			Esys_Free(data);
			return false;
		}

		sSecurityModuleLogicInfo2_d *sml_info = (sSecurityModuleLogicInfo2_d *) (vendor_property->buffer);

		*counter = big_endian_to_host(sml_info->wFieldUpgradeCounter);
		
		Esys_Free(data);
	} else if(type == INFINEON_SLB9672_TYPE){
		ret = Esys_GetCapability((*connection)->esys_ctx, ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE,
				TPM2_CAP_VENDOR_PROPERTY, TPM_PT_VENDOR_FIX_FU_COUNTER, 1, &moreData, &data);
		if(ret != TSS2_RC_SUCCESS || data == NULL){
			std::cerr << "Error: Esys_GetCapability TPM_PT_VENDOR_FIX_FU_COUNTER failed" << std::endl;
			return false;
		}

		if(data->data.vendor.size < 6){
			std::cerr << "Error: Vendor capability data size is too small" << std::endl;
			Esys_Free(data);
			return false;
		}

		IFX_Vendor_Property *vendor_property = (IFX_Vendor_Property *) data->data.vendor.buffer;

		uint32_t count = big_endian_to_host(vendor_property->count);
		uint16_t size = big_endian_to_host(vendor_property->size);

		if(count != 1 || size != 2){
			std::cerr << "Error: Vendor capability data count is not 1 or size is not 2" << std::endl;
			Esys_Free(data);
			return false;
		}

		if(data->data.vendor.size < size + 6){
			std::cerr << "Error: Vendor capability data size is too small for the structure" << std::endl;
			Esys_Free(data);
			return false;
		}

		memcpy(counter, vendor_property->buffer, 2);
		*counter = big_endian_to_host(*counter);

		Esys_Free(data);
	} else{
		std::cerr << "Error: Unkown TPM type in tpm_get_upgrade_counter" << std::endl;
		return false;
	}

	if((*connection)->config.verbose_print){
		std::cout << "Upgrade Counter: " << *counter << std::endl;
	}

	return true;
}

/**
 * @brief Returns the upgrade counter with the same firmware of the TPM
 * 
 * @param connection information to connect to the TPM
 * 
 * @param counter of the TPM to update the firmware
 * 
 * @return returns true when successful, otherwise false
 */
bool tpm_get_upgrade_counter_same(struct TPMConnection **connection, uint16_t *counter){
	TSS2_RC ret;
	TPMI_YES_NO moreData = TPM2_NO;
	TPMS_CAPABILITY_DATA *data = NULL;

	uint64_t type = 0;
	if(!tpm_get_type(connection, &type)){
		std::cerr << "Error: failed to get type of TPM module" << std::endl;
		return false;
	}

	if(type == INFINEON_SLB9670_TYPE){
		*counter = 0; // Not available on SLB9670
		if((*connection)->config.verbose_print){
			std::cout << "Upgrade counter same not available on SLB9670" << std::endl;
		}
	} else if(type == INFINEON_SLB9672_TYPE){
		ret = Esys_GetCapability((*connection)->esys_ctx, ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE,
					TPM2_CAP_VENDOR_PROPERTY, TPM_PT_VENDOR_FIX_FU_COUNTER_SAME, 1, &moreData, &data);

		if(ret != TSS2_RC_SUCCESS || data == NULL){
				std::cerr << "Error: Esys_GetCapability TPM_PT_VENDOR_FIX_FU_COUNTER failed" << std::endl;
				return false;
			}

		if(data->data.vendor.size < 6){
			std::cerr << "Error: Vendor capability data size is too small" << std::endl;
			Esys_Free(data);
			return false;
		}

		IFX_Vendor_Property *vendor_property = (IFX_Vendor_Property *) data->data.vendor.buffer;

		uint32_t count = big_endian_to_host(vendor_property->count);
		uint16_t size = big_endian_to_host(vendor_property->size);

		if(count != 1 || size != 2){
			std::cerr << "Error: Vendor capability data count is not 1 or size is not 2" << std::endl;
			Esys_Free(data);
			return false;
		}

		if(data->data.vendor.size < size + 6){
			std::cerr << "Error: Vendor capability data size is too small for the structure" << std::endl;
			Esys_Free(data);
			return false;
		}

		memcpy(counter, vendor_property->buffer, 2);
		*counter = big_endian_to_host(*counter);

		if((*connection)->config.verbose_print){
			std::cout << "Upgrade counter same: " << *counter << std::endl;
		}
		Esys_Free(data);
	} else{
		std::cerr << "Unkown type of TPM" << std::endl;
	}

	return true;
}

/**
 * @brief Returns the operation mode of the TPM
 * 
 * @param connection information to connect to the TPM
 * 
 * @param mode of the TPM to update the firmware
 * 
 * @return returns true when successful, otherwise false
 */
bool tpm_get_operation_mode(struct TPMConnection **connection, uint16_t *mode){
    if(connection == NULL || *connection == NULL || mode == NULL){
        std::cerr << "Error: Invalid argument in tpm_get_operation_mode" << std::endl;
        return false;
    }

    uint64_t type = 0;
    if(!tpm_get_type(connection, &type)){
        std::cerr << "Error: failed to get type of TPM module" << std::endl;
        return false;
    }

    TSS2_RC ret;
    TPMI_YES_NO moreData = TPM2_NO;
    TPMS_CAPABILITY_DATA *data = NULL;

    if(type == INFINEON_SLB9670_TYPE){
        ret = Esys_GetCapability((*connection)->esys_ctx, ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE,
                    TPM2_CAP_VENDOR_PROPERTY, TPM_PT_VENDOR_FIX_SMLI2, 1, &moreData, &data);

        if(ret != TSS2_RC_SUCCESS || data == NULL){
            std::cerr << "Error: Esys_GetCapability TPM_PT_VENDOR_FIX_SMLI2 failed" << std::endl;
            return false;
        }

		if(data->data.vendor.size < 6){
			std::cerr << "Error: Vendor capability data size is too small" << std::endl;
			Esys_Free(data);
			return false;
		}

		IFX_Vendor_Property *vendor_property = (IFX_Vendor_Property *) data->data.vendor.buffer;

		uint32_t count = big_endian_to_host(vendor_property->count);
		uint16_t size = big_endian_to_host(vendor_property->size);

		if(count != 1){
			std::cerr << "Error: Vendor capability data count is not 1" << std::endl;
			Esys_Free(data);
			return false;
		}

		if(data->data.vendor.size < size + 6){
			std::cerr << "Error: Vendor capability data size is too small for the structure" << std::endl;
			Esys_Free(data);
			return false;
		}	

		sSecurityModuleLogicInfo2_d *sml_info = (sSecurityModuleLogicInfo2_d *) (vendor_property->buffer);

		*mode = big_endian_to_host(sml_info->SecurityModuleStatus);

        Esys_Free(data);
    } else if(type == INFINEON_SLB9672_TYPE){
        ret = Esys_GetCapability((*connection)->esys_ctx, ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE,
                    TPM2_CAP_VENDOR_PROPERTY, TPM_PT_VENDOR_FIX_FU_OPERATION_MODE, 1, &moreData, &data);

        if(ret != TSS2_RC_SUCCESS || data == NULL){
            std::cerr << "Error: Esys_GetCapability TPM_PT_VENDOR_FIX_FU_OPERATION_MODE failed" << std::endl;
            return false;
        }

        if(data->data.vendor.size < 6){
			std::cerr << "Error: TPM_PT_VENDOR_FIX_FU_OPERATION_MODE returned invalid size" << std::endl;
			Esys_Free(data);
			return false;
		}

		IFX_Vendor_Property *vendor_property = (IFX_Vendor_Property *) data->data.vendor.buffer;

		uint32_t count = big_endian_to_host(vendor_property->count);
		uint16_t size = big_endian_to_host(vendor_property->size);

		if(count != 1 || size != 1){
			std::cerr << "Error: TPM_PT_VENDOR_FIX_FU_OPERATION_MODE returned invalid count or size" << std::endl;
			Esys_Free(data);
			return false;
		}

		if(data->data.vendor.size < size + 6){
			std::cerr << "Error: Vendor capability data size is too small for the structure" << std::endl;
			Esys_Free(data);
			return false;
		}
		*mode = vendor_property->buffer[0];

        Esys_Free(data);
    } else{
        std::cerr << "Unkown type of TPM" << std::endl;
    }

	if((*connection)->config.verbose_print){
		std::cout << "TPM operation mode: " << *mode << std::endl;
	}
	return true;
}

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
bool tpm_get_max_block_size(struct TPMConnection **connection, uint16_t *max_block_size){
	if(connection == NULL || *connection == NULL || max_block_size == NULL){
		std::cerr << "Error: Invalid argument in tpm_get_max_block_size" << std::endl;
		return false;
	}

	uint64_t type = 0;
	if(!tpm_get_type(connection, &type)){
		std::cerr << "Error: failed to get type of TPM module" << std::endl;
		return false;
	}

	if(type == INFINEON_SLB9670_TYPE){
		TSS2_RC ret;
		TPMI_YES_NO moreData = TPM2_NO;
		TPMS_CAPABILITY_DATA *data = NULL;

		ret = Esys_GetCapability((*connection)->esys_ctx, ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE,
					TPM2_CAP_VENDOR_PROPERTY, TPM_PT_VENDOR_FIX_SMLI2, 1, &moreData, &data);

		if(ret != TSS2_RC_SUCCESS || data == NULL){
			std::cerr << "Error: Esys_GetCapability TPM_PT_VENDOR_FIX_SMLI2 failed" << std::endl;
			return false;
		}

		if(data->data.vendor.size < 6){
			std::cerr << "Error: Vendor capability data size is too small" << std::endl;
			Esys_Free(data);
			return false;
		}

		IFX_Vendor_Property *vendor_property = (IFX_Vendor_Property *) data->data.vendor.buffer;

		uint32_t count = big_endian_to_host(vendor_property->count);
		uint16_t size = big_endian_to_host(vendor_property->size);

		if(count != 1){
			std::cerr << "Error: Vendor capability data count is not 1" << std::endl;
			Esys_Free(data);
			return false;
		}

		if(data->data.vendor.size < size + 6){
			std::cerr << "Error: Vendor capability data size is too small for the structure" << std::endl;
			Esys_Free(data);
			return false;
		}

		sSecurityModuleLogicInfo2_d *sml_info = (sSecurityModuleLogicInfo2_d *) (vendor_property->buffer);
		*max_block_size = big_endian_to_host(sml_info->wMaxDataSize);

		Esys_Free(data);
	} else if(type == INFINEON_SLB9672_TYPE){
		*max_block_size = TSS_MAX_DIGEST_BUFFER;
	} else{
		std::cerr << "Unkown type of TPM" << std::endl;
	}

	if((*connection)->config.verbose_print){
		std::cout << "Max block size: " << *max_block_size << std::endl;
	}

	return true;
}

/**
 * @brief Returns the target hardware identifier of the TPM
 * 
 * @param connection information to connect to the TPM
 * 
 * @param target_hardware_identifier of the TPM to update the firmware
 * 
 * @return returns true when successful, otherwise false
 */
bool tpm_get_target_hardware_identifier(struct TPMConnection **connection, uint32_t *target_hardware_identifier){
	if(connection == NULL || *connection == NULL || target_hardware_identifier == NULL){
		std::cerr << "Error: Invalid argument in tpm_get_target_hardware_identifier" << std::endl;
		return false;
	}

	uint64_t type = 0;
	if(!tpm_get_type(connection, &type)){
		std::cerr << "Error: failed to get type of TPM module" << std::endl;
		return false;
	}

	if(type == INFINEON_SLB9670_TYPE){
		TSS2_RC ret;
		TPMI_YES_NO moreData = TPM2_NO;
		TPMS_CAPABILITY_DATA *data = NULL;

		ret = Esys_GetCapability((*connection)->esys_ctx, ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE,
					TPM2_CAP_VENDOR_PROPERTY, TPM_PT_VENDOR_FIX_SMLI2, 1, &moreData, &data);

		if(ret != TSS2_RC_SUCCESS || data == NULL){
			std::cerr << "Error: Esys_GetCapability TPM_PT_VENDOR_FIX_SMLI2 failed" << std::endl;
			return false;
		}

		if(data->data.vendor.size < 6){
			std::cerr << "Error: Vendor capability data size is too small" << std::endl;
			Esys_Free(data);
			return false;
		}

		IFX_Vendor_Property *vendor_property = (IFX_Vendor_Property *) data->data.vendor.buffer;

		uint32_t count = big_endian_to_host(vendor_property->count);
		uint16_t size = big_endian_to_host(vendor_property->size);

		if(count != 1){
			std::cerr << "Error: Vendor capability data count is not 1" << std::endl;
			Esys_Free(data);
			return false;
		}

		if(data->data.vendor.size < size + 6){
			std::cerr << "Error: Vendor capability data size is too small for the structure" << std::endl;
			Esys_Free(data);
			return false;
		}

		sSecurityModuleLogicInfo2_d *sml_info = (sSecurityModuleLogicInfo2_d *) (vendor_property->buffer);
		*target_hardware_identifier = big_endian_to_host(sml_info->sSecurityModuleLogic.unTargetHardwareIdentifier);

		Esys_Free(data);
	} else if(type == INFINEON_SLB9672_TYPE){
		*target_hardware_identifier = 0; // Not available on SLB9672
	} else{
		std::cerr << "Unkown type of TPM" << std::endl;
	}

	if((*connection)->config.verbose_print){
		std::cout << "Target hardware identifier: " << *target_hardware_identifier << std::endl;
	}

	return true;
}

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
bool tpm_get_decrypt_keys(struct TPMConnection **connection, uint16_t *decrypt_key_count, uint32_t decrypt_key_ids[4]){
	if(connection == NULL || *connection == NULL || decrypt_key_count == NULL || decrypt_key_ids == NULL){
		std::cerr << "Error: Invalid argument in tpm_get_decrypt_keys" << std::endl;
		return false;
	}

	uint64_t type = 0;
	if(!tpm_get_type(connection, &type)){
		std::cerr << "Error: failed to get type of TPM module" << std::endl;
		return false;
	}

	if(type == INFINEON_SLB9670_TYPE){
		TSS2_RC ret;
		TPMI_YES_NO moreData = TPM2_NO;
		TPMS_CAPABILITY_DATA *data = NULL;

		ret = Esys_GetCapability((*connection)->esys_ctx, ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE,
					TPM2_CAP_VENDOR_PROPERTY, TPM_PT_VENDOR_FIX_SMLI2, 1, &moreData, &data);

		if(ret != TSS2_RC_SUCCESS || data == NULL){
			std::cerr << "Error: Esys_GetCapability TPM_PT_VENDOR_FIX_SMLI2 failed" << std::endl;
			return false;
		}

		if(data->data.vendor.size < 6){
			std::cerr << "Error: Vendor capability data size is too small" << std::endl;
			Esys_Free(data);
			return false;
		}

		IFX_Vendor_Property *vendor_property = (IFX_Vendor_Property *) data->data.vendor.buffer;

		uint32_t count = big_endian_to_host(vendor_property->count);
		uint16_t size = big_endian_to_host(vendor_property->size);

		if(count != 1){
			std::cerr << "Error: Vendor capability data count is not 1" << std::endl;
			Esys_Free(data);
			return false;
		}

		if(data->data.vendor.size < size + 6){
			std::cerr << "Error: Vendor capability data size is too small for the structure" << std::endl;
			Esys_Free(data);
			return false;
		}

		sSecurityModuleLogicInfo2_d *sml_info = (sSecurityModuleLogicInfo2_d *) (vendor_property->buffer);

		sKeyList_d *key_list = &sml_info->sKeyList;

		*decrypt_key_count = big_endian_to_host(key_list->wEntries);

		if(*decrypt_key_count > 4){
			std::cerr << "Error: Decrypt key count is greater than 4 " << *decrypt_key_count << " " << key_list->wEntries << " " << ~(*decrypt_key_count) << std::endl;
			Esys_Free(data);
			return false;
		}

		for(int i = 0; i < *decrypt_key_count && i < 4; i++){
			decrypt_key_ids[i] = big_endian_to_host(key_list->DecryptKeyId[i]);
		}
		
		Esys_Free(data);
	} else if(type == INFINEON_SLB9672_TYPE){
		*decrypt_key_count = 0; // Not available on SLB9672
		decrypt_key_ids[0] = 0;
		decrypt_key_ids[1] = 0;
		decrypt_key_ids[2] = 0;
		decrypt_key_ids[3] = 0;
	} else{
		std::cerr << "Unkown type of TPM" << std::endl;
	}

	if((*connection)->config.verbose_print){
		std::cout << "Decrypt key count: " << *decrypt_key_count << std::endl;
		for(int i = 0; i < *decrypt_key_count && i < 4; i++){
			std::cout << "Decrypt key ID " << i << ": " << decrypt_key_ids[i] << std::endl;
		}
	}

	return true;
}

/**
 * @brief Returns the active firmware version of the TPM
 * 
 * @param connection information to connect to the TPM
 * 
 * @param active_firmware_version of the TPM to update the firmware
 * 
 * @return returns true when successful, otherwise false
 */
bool tpm_get_active_firmware_version(struct TPMConnection **connection, uint32_t *active_firmware_version){
	if(connection == NULL || *connection == NULL || active_firmware_version == NULL){
		std::cerr << "Error: Invalid argument in tpm_get_active_firmware_version" << std::endl;
		return false;
	}

	uint64_t type = 0;
	if(!tpm_get_type(connection, &type)){
		std::cerr << "Error: failed to get type of TPM module" << std::endl;
		return false;
	}

	if(type == INFINEON_SLB9670_TYPE){
		TSS2_RC ret;
		TPMI_YES_NO moreData = TPM2_NO;
		TPMS_CAPABILITY_DATA *data = NULL;

		ret = Esys_GetCapability((*connection)->esys_ctx, ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE,
					TPM2_CAP_VENDOR_PROPERTY, TPM_PT_VENDOR_FIX_SMLI2, 1, &moreData, &data);

		if(ret != TSS2_RC_SUCCESS || data == NULL){
			std::cerr << "Error: Esys_GetCapability TPM_PT_VENDOR_FIX_SMLI2 failed" << std::endl;
			return false;
		}

		if(data->data.vendor.size < 6){
			std::cerr << "Error: Vendor capability data size is too small" << std::endl;
			Esys_Free(data);
			return false;
		}

		IFX_Vendor_Property *vendor_property = (IFX_Vendor_Property *) data->data.vendor.buffer;

		uint32_t count = big_endian_to_host(vendor_property->count);
		uint16_t size = big_endian_to_host(vendor_property->size);

		if(count != 1){
			std::cerr << "Error: Vendor capability data count is not 1" << std::endl;
			Esys_Free(data);
			return false;
		}

		if(data->data.vendor.size < size + 6){
			std::cerr << "Error: Vendor capability data size is too small for the structure" << std::endl;
			Esys_Free(data);
			return false;
		}

		sSecurityModuleLogicInfo2_d *sml_info = (sSecurityModuleLogicInfo2_d *) (vendor_property->buffer);

		sFirmwarePackages_d *firmware_packages = &sml_info->sSecurityModuleLogic.sFirmwareConfiguration;

		uint32_t stale_version = big_endian_to_host(firmware_packages->FirmwarePackage[0].StaleVersion);

		if(stale_version == 0xFFFFEEEE){ // Active firmware version is inactive from INACTIVE STALE VERSION
			*active_firmware_version = big_endian_to_host(firmware_packages->FirmwarePackage[1].Version);
		} else{
			*active_firmware_version = big_endian_to_host(firmware_packages->FirmwarePackage[0].Version);
		}
		
		Esys_Free(data);
	} else if(type == INFINEON_SLB9672_TYPE){
		*active_firmware_version = 0; // Not available on SLB9672
	} else{
		std::cerr << "Unkown type of TPM" << std::endl;
	}

	if((*connection)->config.verbose_print){
		std::cout << "Active firmware version: " << *active_firmware_version << std::endl;
	}

	return true;
}

/**
 * @brief Returns the unique id of the firmware currently processed
 * 
 * @param connection information to connect to the TPM
 * 
 * @param unique_id of the firmware currently processed
 * 
 * @return returns true when successful, otherwise false
 */
bool tpm_get_process_firmware_unique_id(struct TPMConnection **connection, uint8_t *unique_id){
	if(connection == NULL || *connection == NULL || unique_id == NULL){
		std::cerr << "Error: Invalid argument in tpm_get_process_firmware_unique_id" << std::endl;
		return false;
	}

	uint64_t type = 0;
	if(!tpm_get_type(connection, &type)){
		std::cerr << "Error: failed to get type of TPM module" << std::endl;
		return false;
	}

	if(type == INFINEON_SLB9670_TYPE){
		TSS2_RC ret;
		TPMI_YES_NO moreData = TPM2_NO;
		TPMS_CAPABILITY_DATA *data = NULL;

		ret = Esys_GetCapability((*connection)->esys_ctx, ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE,
					TPM2_CAP_VENDOR_PROPERTY, TPM_PT_VENDOR_FIX_SMLI2, 1, &moreData, &data);

		if(ret != TSS2_RC_SUCCESS || data == NULL){
			std::cerr << "Error: Esys_GetCapability TPM_PT_VENDOR_FIX_SMLI2 failed" << std::endl;
			return false;
		}

		if(data->data.vendor.size < 6){
			std::cerr << "Error: Vendor capability data size is too small" << std::endl;
			Esys_Free(data);
			return false;
		}

		IFX_Vendor_Property *vendor_property = (IFX_Vendor_Property *) data->data.vendor.buffer;

		uint32_t count = big_endian_to_host(vendor_property->count);
		uint16_t size = big_endian_to_host(vendor_property->size);

		if(count != 1){
			std::cerr << "Error: Vendor capability data count is not 1" << std::endl;
			Esys_Free(data);
			return false;
		}

		if(data->data.vendor.size < size + 6){
			std::cerr << "Error: Vendor capability data size is too small for the structure" << std::endl;
			Esys_Free(data);
			return false;
		}

		sSecurityModuleLogicInfo2_d *sml_info = (sSecurityModuleLogicInfo2_d *) (vendor_property->buffer);

		*unique_id = (uint8_t) (big_endian_to_host(sml_info->sProcessFirmwarePackage.StaleVersion) & 0x0F);
		
		Esys_Free(data);
	} else if(type == INFINEON_SLB9672_TYPE){
		*unique_id = 0; // Not available on SLB9672
	} else{
		std::cerr << "Unkown type of TPM" << std::endl;
		return false;
	}

	if((*connection)->config.verbose_print){
		std::cout << "Process firmware unique ID: " << static_cast<int>(*unique_id) << std::endl;
	}

	return true;
}

/**
 * @brief Returns the key group id of the TPM
 * 
 * @param connection information to connect to the TPM
 * 
 * @param key_group_id of the TPM to update the firmware
 * 
 * @return returns true when successful, otherwise false
 */
bool tpm_get_key_group_id(struct TPMConnection **connection, uint32_t *key_group_id){
	if(connection == NULL || *connection == NULL || key_group_id == NULL){
		std::cerr << "Error: Invalid argument in tpm_get_key_group_id" << std::endl;
		return false;
	}

	uint64_t type = 0;
	if(!tpm_get_type(connection, &type)){
		std::cerr << "Error: failed to get type of TPM module" << std::endl;
		return false;
	}

	if(type == INFINEON_SLB9670_TYPE){
		*key_group_id = 0; // Not available on SLB9670
	} else if(type == INFINEON_SLB9672_TYPE){
		TSS2_RC ret;
		TPMI_YES_NO moreData = TPM2_NO;
		TPMS_CAPABILITY_DATA *data = NULL;

		ret = Esys_GetCapability((*connection)->esys_ctx, ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE,
					TPM2_CAP_VENDOR_PROPERTY, TPM_PT_VENDOR_FIX_FU_KEYGROUP_ID, 1, &moreData, &data);

		if(ret != TSS2_RC_SUCCESS || data == NULL){
			std::cerr << "Error: Esys_GetCapability TPM_PT_VENDOR_FIX_FU_KEYGROUP_ID failed" << std::endl;
			return false;
		}

		if(data->data.vendor.size < 6){
			std::cerr << "Error: Esys_GetCapability TPM_PT_VENDOR_FIX_FU_KEYGROUP_ID returned invalid size" << std::endl;
			Esys_Free(data);
			return false;
		}

		IFX_Vendor_Property *vendor_property = (IFX_Vendor_Property *) data->data.vendor.buffer;

		uint32_t count = big_endian_to_host(vendor_property->count);
		uint16_t size = big_endian_to_host(vendor_property->size);

		if(count != 1 || size != 4){
			std::cerr << "Error: Esys_GetCapability TPM_PT_VENDOR_FIX_FU_KEYGROUP_ID returned invalid count or size" << std::endl;
			Esys_Free(data);
			return false;
		}

		if(data->data.vendor.size < size + 6){
			std::cerr << "Error: Vendor capability data size is too small for the structure" << std::endl;
			Esys_Free(data);
			return false;
		}

		memcpy(key_group_id, vendor_property->buffer, 4);
		*key_group_id = big_endian_to_host(*key_group_id);
		
		Esys_Free(data);
	} else{
		std::cerr << "Unkown type of TPM" << std::endl;
		return false;

	}

	if((*connection)->config.verbose_print){
		std::cout << "Key group ID: " << *key_group_id << std::endl;
	}

	return true;
}

/**
 * @brief Returns the firmware update properties of the TPM
 * 
 * @param connection information to connect to the TPM
 * 
 * @param firmware_update_properties of the TPM to update the firmware
 * 
 * @return returns true when successful, otherwise false
 */
bool tpm_get_firmware_update_properties(struct TPMConnection **connection, uint32_t *firmware_update_properties){
	if(connection == NULL || *connection == NULL || firmware_update_properties == NULL){
		std::cerr << "Error: Invalid argument in tpm_get_firmware_update_properties" << std::endl;
		return false;
	}

	uint64_t type = 0;
	if(!tpm_get_type(connection, &type)){
		std::cerr << "Error: failed to get type of TPM module" << std::endl;
		return false;
	}

	if(type == INFINEON_SLB9670_TYPE){
		*firmware_update_properties = 0; // Not available on SLB9670
	} else if(type == INFINEON_SLB9672_TYPE){
		TSS2_RC ret;
		TPMI_YES_NO moreData = TPM2_NO;
		TPMS_CAPABILITY_DATA *data = NULL;

		ret = Esys_GetCapability((*connection)->esys_ctx, ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE,
					TPM2_CAP_VENDOR_PROPERTY, TPM_PT_VENDOR_FIX_FU_PROPERTIES, 1, &moreData, &data);

		if(ret != TSS2_RC_SUCCESS || data == NULL){
			std::cerr << "Error: Esys_GetCapability TPM_PT_VENDOR_FIX_FU_PROPERTIES failed" << std::endl;
			return false;
		}
		
		if(data->data.vendor.size < 6){
			std::cerr << "Error: Vendor capability data too small" << std::endl;
			Esys_Free(data);
			return false;
		}

		IFX_Vendor_Property *vendor_property = (IFX_Vendor_Property *) data->data.vendor.buffer;

		uint32_t count = big_endian_to_host(vendor_property->count);
		uint16_t size = big_endian_to_host(vendor_property->size);

		if(count != 1 || size != 4){
			std::cerr << "Error: Esys_GetCapability TPM_PT_VENDOR_FIX_FU_PROPERTIES returned invalid count or size" << std::endl;
			Esys_Free(data);
			return false;
		}

		if(data->data.vendor.size < size + 6){
			std::cerr << "Error: Vendor capability data size is too small for the structure" << std::endl;
			Esys_Free(data);
			return false;
		}

		memcpy(firmware_update_properties, vendor_property->buffer, 4);
		*firmware_update_properties = big_endian_to_host(*firmware_update_properties);
		
		Esys_Free(data);
	} else{
		std::cerr << "Unkown type of TPM" << std::endl;
		return false;
	}

	if((*connection)->config.verbose_print){
		std::cout << "Firmware update properties: " << *firmware_update_properties << std::endl;
	}

	return true;
}

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
bool tpm_get_start_hash(struct TPMConnection **connection, uint16_t *algorithm, uint16_t *hash_size, uint8_t hash_digest[64]){
	if(connection == NULL || *connection == NULL || algorithm == NULL || hash_size == NULL || hash_digest == NULL){
		std::cerr << "Error: Invalid argument in tpm_get_start_hash" << std::endl;
		return false;
	}
	
	uint64_t type = 0;
	if(!tpm_get_type(connection, &type)){
		std::cerr << "Error: failed to get type of TPM module" << std::endl;
		return false;
	}

	*algorithm = 0;
	*hash_size = 0;
	memset(hash_digest, 0, 64);

	if(type == INFINEON_SLB9670_TYPE){
		return true; // No needed to get the start hash
	} else if(type == INFINEON_SLB9672_TYPE){
		uint16_t operation_mode = 0;
		if(!tpm_get_operation_mode(connection, &operation_mode)){
			std::cerr << "Error: failed to get operation mode of TPM module" << std::endl;
			return false;
		}

		if(operation_mode == 0x00 || operation_mode == 0x03 || operation_mode == 0x83){
			return true; // No needed to get the start hash
		}

		TSS2_RC ret;
		TPMI_YES_NO moreData = TPM2_NO;
		TPMS_CAPABILITY_DATA *data = NULL;

		ret = Esys_GetCapability((*connection)->esys_ctx, ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE,
					TPM2_CAP_VENDOR_PROPERTY, TPM_PT_VENDOR_FIX_FU_START_HASH_DIGEST, 1, &moreData, &data);

		if(ret != TSS2_RC_SUCCESS || data == NULL){
			std::cerr << "Error: Esys_GetCapability TPM_PT_VENDOR_FIX_FU_START_HASH failed" << std::endl;
			return false;
		}

		if(data->data.vendor.size < 6){
			std::cerr << "Error: Vendor capability data too small" << std::endl;
			Esys_Free(data);
			return false;
		}

		IFX_Vendor_Property *vendor_property = (IFX_Vendor_Property *) data->data.vendor.buffer;

		uint32_t count = big_endian_to_host(vendor_property->count);
		uint16_t size = big_endian_to_host(vendor_property->size);

		if(count != 1 || size < 2){
			std::cerr << "Error: Esys_GetCapability TPM_PT_VENDOR_FIX_FU_START_HASH returned invalid count or size" << std::endl;
			Esys_Free(data);
			return false;
		}

		if(data->data.vendor.size < size + 6){
			std::cerr << "Error: Vendor capability data size is too small for the structure" << std::endl;
			Esys_Free(data);
			return false;
		}

		memcpy(algorithm, vendor_property->buffer, 2);
		*algorithm = big_endian_to_host(*algorithm);

		if(*algorithm == TPM2_ALG_SHA384){
			*hash_size = 48;
		} else if(*algorithm == TPM2_ALG_SHA512){
			*hash_size = 64;
		} else{
			std::cerr << "Error: Unknown algorithm in tpm_get_start_hash" << std::endl;
			Esys_Free(data);
			return false;
		}

		if(size != 2 + *hash_size){
			std::cerr << "Error: Vendor capability data size does not match expected hash size" << std::endl;
			Esys_Free(data);
			return false;
		}
		
		memcpy(hash_digest, vendor_property->buffer + 2, *hash_size);
		
		Esys_Free(data);
	} else{
		std::cerr << "Unkown type of TPM" << std::endl;
		return false;
	}

	if((*connection)->config.verbose_print){
		std::cout << "Start hash algorithm: " << *algorithm << std::endl;
		std::cout << "Start hash size: " << *hash_size << std::endl;
	}
	
	return true;
}

/**
 * @brief Returns the vendor string of the TPM
 * 
 * @param connection information to connect to the TPM
 * 
 * @param vendor_string of the TPM to update the firmware
 * 
 * @return returns true when successful, otherwise false
 */
bool tpm_get_vendor_string(struct TPMConnection **connection, uint8_t vendor_string[16]){
	if(connection == NULL || *connection == NULL || vendor_string == NULL){
		std::cerr << "Error: Invalid argument in tpm_get_vendor_string" << std::endl;
		return false;
	}

	uint64_t type = 0;
	if(!tpm_get_type(connection, &type)){
		std::cerr << "Error: failed to get type of TPM module" << std::endl;
		return false;
	}

	TSS2_RC ret;
	TPMI_YES_NO moreData = TPM2_NO;
	TPMS_CAPABILITY_DATA *data = NULL;

	ret = Esys_GetCapability((*connection)->esys_ctx, ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE, 
				TPM2_CAP_TPM_PROPERTIES, TPM2_PT_VENDOR_STRING_1, 4, &moreData, &data);
			
	if(ret != TSS2_RC_SUCCESS || data == NULL){
		std::cerr << "Error: Esys_GetCapability TPM2_PT_VENDOR_STRING_1 failed" << std::endl;
		return false;
	}

	if(data->data.tpmProperties.count != 4){
		std::cerr << data->data.tpmProperties.count << std::endl;
		std::cerr << "Error: Esys_GetCapability TPM2_PT_VENDOR_STRING_1 returned less than 4 properties" << std::endl;
		Esys_Free(data);
		return false;
	}

	for(size_t i = 0; i < 4; i++){
		uint32_t value = big_endian_to_host(data->data.tpmProperties.tpmProperty[i].value);
		memcpy(&vendor_string[i * 4], &value, 4);
	}

	Esys_Free(data);

	if((*connection)->config.verbose_print){
		std::cout << "Vendor string: ";
		for(int i = 0; i < 16; i++){
			std::cout << vendor_string[i];
		}
		std::cout << std::endl;
	}

	return true;
}

/*****************************************************************************************************/
// Summarize function
/*****************************************************************************************************/

/**
 * @brief Get information of the TPM
 * 
 * @param info 
 * 
 * @return True if it succeded to get the information, false otherwise
 */
bool tpm_get_information(struct TPMConnection **connection, struct TPMInformation **info){

	// Check for NULL values
	if(info == NULL || connection == NULL){
		std::cerr << "Error: connection or info pointers are NULL" << std::endl;
		return false;
	}

	// Allocate memory for TPMInformation
	if(*info == NULL){
		*info = (struct TPMInformation*) malloc(sizeof(struct TPMInformation));
		if(*info == NULL){
			std::cerr << "Error: failed to allocate TPMInformation struct" << std::endl;
			return false;
		}
	}
	
	// Manufacturer id
	uint32_t manufacturer = 0;
	if(!tpm_get_manufacturer(connection, &manufacturer)){
		return false;
	}
	(*info)->manufacturer = manufacturer;

	// TPM type
	uint64_t type = 0;
	if(!tpm_get_type(connection, &type)){
		return false;
	}
	(*info)->type = type;
	uint64_t type_char = (host_to_big_endian(type));
	if((*connection)->config.verbose_print){
		std::cout << "TPM type: " << std::hex << ((uint8_t *)&type_char) << std::dec << std::endl;
	}

	// Firmware version
	uint64_t firmware_version = 0;
	if(!tpm_get_firmware_version(connection, &firmware_version)){
		return false;
	}
	(*info)->firmware_version = firmware_version;

	// Upgrade counter
	uint16_t counter = 0;
	if(!tpm_get_upgrade_counter(connection, &counter)){
		return false;
	}
	(*info)->upgrade_counter = counter;
	
	// Upgrade counter with the same firmware version
	uint16_t counter_same = 0;
	if(!tpm_get_upgrade_counter_same(connection, &counter_same)){
		return false;
	}
	(*info)->upgrade_counter_same = counter_same;

	// Operation mode
	uint16_t operation_mode = 0;
	if(!tpm_get_operation_mode(connection, &operation_mode)){
		return false;
	}
	(*info)->operation_mode = operation_mode;

	uint16_t max_block_size = 0;
	if(!tpm_get_max_block_size(connection, &max_block_size)){
		return false;
	}
	(*info)->max_block_size = max_block_size;

	uint32_t target_hardware_identifier = 0;
	if(!tpm_get_target_hardware_identifier(connection, &target_hardware_identifier)){
		return false;
	}
	(*info)->target_hardware_identifier = target_hardware_identifier;

	uint32_t decrypt_key_ids[4] = {};
	uint16_t decrypt_key_count = 0;
	if(!tpm_get_decrypt_keys(connection, &decrypt_key_count, decrypt_key_ids)){
		return false;
	}
	(*info)->decrypt_key_count = decrypt_key_count;
	memcpy((*info)->decrypt_key_ids, decrypt_key_ids, sizeof(decrypt_key_ids));

	uint32_t active_firmware_version = 0;
	if(!tpm_get_active_firmware_version(connection, &active_firmware_version)){
		return false;
	}
	(*info)->active_firmware_version = active_firmware_version;

	uint8_t process_firmware_unique_id = 0;
	if(!tpm_get_process_firmware_unique_id(connection, &process_firmware_unique_id)){
		return false;
	}
	(*info)->process_firmware_unique_id = process_firmware_unique_id;

	uint32_t key_group_id = 0;
	if(!tpm_get_key_group_id(connection, &key_group_id)){
		return false;
	}
	(*info)->key_group_id = key_group_id;

	uint32_t firmware_update_properties = 0;
	if(!tpm_get_firmware_update_properties(connection, &firmware_update_properties)){
		return false;
	}
	(*info)->firmware_update_properties = firmware_update_properties;

	uint16_t start_hash_algorithm = 0;
	uint16_t start_hash_size = 0;
	uint8_t start_hash_digest[64] = {};
	if(!tpm_get_start_hash(connection, &start_hash_algorithm, &start_hash_size, start_hash_digest)){
		return false;
	}
	(*info)->start_hash_algorithm = start_hash_algorithm;
	(*info)->start_hash_size = start_hash_size;
	memcpy((*info)->start_hash_digest, start_hash_digest, 64);

	uint8_t vendor_string[16] = {};
	if(!tpm_get_vendor_string(connection, vendor_string)){
		return false;
	}
	memcpy((*info)->vendor_string, vendor_string, 16);

	return true;
}