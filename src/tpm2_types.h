/**
 * @brief Declares types and definitons
 * @author Jan Haeussermann
 * 
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <tss2/tss2_tcti.h>
#include <tss2/tss2_esys.h>

/*****************************************************************************************************/
// Constant values
/*****************************************************************************************************/

#define     INFINEON_MANUFACTORY_CODE                       0x49465800            // IFX\0
// #define     INFINEON_SLB9670_TYPE                           0x30373639424c53   // SLB9670\0
#define     INFINEON_SLB9670_TYPE                           0x534C423936373000    // SLB9670\0 
// #define     INFINEON_SLB9672_TYPE                           0x32373639424c53   // SLB9672\0 
#define     INFINEON_SLB9672_TYPE                           0x534C423936373200    // SLB9672\0                           

/*****************************************************************************************************/
// Constant values from Vendor (Infineon) 
/*****************************************************************************************************/

/// The group of fixed vendor specific properties
#define     PT_VENDOR_FIX                                   0x80000000
/// The group of variable vendor specific properties
#define     PT_VENDOR_VAR                                   0xC0000000
/// SLB9665 and SLB9670 properties
#define     TPM_PT_VENDOR_FIX_SMLI2                         (PT_VENDOR_FIX + 1)
/// SLB966X properties
#define     TPM_PT_VENDOR_FIX_TIME_TO_SLEEP                 (PT_VENDOR_FIX + 2)
/// Firmware update properties (new with SLB9672)
#define     TPM_PT_VENDOR_FIX_FU_COUNTER                    (PT_VENDOR_FIX + 3)
#define     TPM_PT_VENDOR_FIX_FU_COUNTER_SAME               (PT_VENDOR_FIX + 4)
#define     TPM_PT_VENDOR_FIX_FU_START_HASH_DIGEST          (PT_VENDOR_FIX + 5)
#define     TPM_PT_VENDOR_FIX_FU_LOADER_FW_VERSION          (PT_VENDOR_FIX + 6)
#define     TPM_PT_VENDOR_FIX_FU_OPERATION_MODE             (PT_VENDOR_FIX + 7)
#define     TPM_PT_VENDOR_FIX_FU_KEYGROUP_ID                (PT_VENDOR_FIX + 8)
#define     TPM_PT_VENDOR_FIX_FU_CHIP_IDENT                 (PT_VENDOR_FIX + 9)
#define     TPM_PT_VENDOR_FIX_FU_CURRENT_TPM_FW_VERSION     (PT_VENDOR_FIX + 10)
#define     TPM_PT_VENDOR_FIX_FU_NEW_TPM_FW_VERSION         (PT_VENDOR_FIX + 11)
#define     TPM_PT_VENDOR_FIX_FU_PROPERTIES                 (PT_VENDOR_FIX + 12)
/// Variable vendor specific properties
#define     TPM_PT_VENDOR_VAR_ENCRYPTDECRYPT2               (PT_VENDOR_VAR + 5)
#define     TPM_PT_VENDOR_VAR_CHANGEEPS                     (PT_VENDOR_VAR + 6)
#define     TPM_PT_VENDOR_VAR_TPMID_NV                      (PT_VENDOR_VAR + 7)

#define 	TSS_MAX_DIGEST_BUFFER                   		1024 // From source code 

// From TPM2_FieldUpgradeTypes.h from the Infineon TPM Updater 
#define     MAX_NUM_FW_PACKAGES_DEVICE                      0x0002
#define     SMS_FWCONFIG_ACTIVE                             0x36A5
#define     SMS_BTLDR_ACTIVE                                0x5A3C
#define     INACTIVE_STALE_VERSION                          0xFFFFEEEE

/*****************************************************************************************************/
// Firmware image parser constants (Infineon)
/*****************************************************************************************************/

#define     TPM_FIRMWARE_MAX_SOURCE_VERSIONS                8
#define     TPM_FIRMWARE_MAX_VERSION_LENGTH                 64
#define     TPM_FIRMWARE_MAX_ARCHIVE_NAME                   256
#define     TPM_FIRMWARE_SIGNATURE_SIZE                     256
#define     TPM_FIRMWARE_MAX_IMAGE_SIZE                     (64U * 1024U * 1024U)

/*****************************************************************************************************/
// SLB9672 operation-mode values
/*****************************************************************************************************/

#define TPM_OPERATION_MODE_NORMAL                         0x00
#define TPM_OPERATION_MODE_UPDATE_CAN_ABANDON             0x01
#define TPM_OPERATION_MODE_UPDATE_CANNOT_ABANDON          0x02
#define TPM_OPERATION_MODE_BEFORE_FINALIZE                0x03
#define TPM_OPERATION_MODE_RESTART_REQUIRED               0x04

#define TPM_OPERATION_MODE_RECOVERY_CAN_ABANDON           0x81
#define TPM_OPERATION_MODE_RECOVERY_CANNOT_ABANDON        0x82
#define TPM_OPERATION_MODE_RECOVERY_BEFORE_FINALIZE       0x83
#define TPM_OPERATION_MODE_RECOVERY_RESTART_REQUIRED      0x84

/*****************************************************************************************************/
// SLB9670 operation-mode values
/*****************************************************************************************************/

#define SMS_FWCONFIG_ACTIVE                               0x36A5 // Normal operating mode
#define SMS_BTLDR_ACTIVE                                  0x5A3C // In bootloader mode

/*****************************************************************************************************/
// Structs for keeping context of the TPM connection and information
/*****************************************************************************************************/

/**
 * @brief Keeps the config of the updater
 */
struct TPMUpdateConfig{
    const char *tcti_device_path;

    // When NULL, search firmware_directory_path
    const char *firmware_image_path;
    const char *firmware_directory_path;

    bool update_same_firmware;
    bool verbose_print;
    bool dry_run;

    uint8_t retry_count;
};

/**
 * @struct TPMConnection
 * 
 * @brief Keeps the conetext save which are needed for connection and functions
 */
struct TPMConnection{
	uint8_t *tcti_buf;				// Buffer needed for the TCTI context
	size_t tcti_buf_size;			// Size of the buffer
	TSS2_TCTI_CONTEXT *tcti_ctx; 	// Context of the TCTI 

	ESYS_CONTEXT *esys_ctx;			// Context of the ESYS

    TPMUpdateConfig config;
};

/**
 * @struct TPMInformation
 * 
 * @brief Keeps track of the TPM infromations  
 */
struct TPMInformation{
	uint16_t upgrade_counter; 		        // How many upgrades are left
	uint16_t upgrade_counter_same; 	        // How many upgrades are left to the same firmware version (slb9672)

	uint64_t firmware_version; 		        // Current firmare version

	uint32_t manufacturer;			        // manufacturer number
	uint64_t type;                          // SLB9670 or SLB9672

    uint16_t operation_mode;		        // slb9672 only uses 1 byte, slb9670 uses 2 bytes (Security module status)

    uint16_t max_block_size;                // Maxium firmware block size for the firmware update

    uint32_t target_hardware_identifier;    // Hardware identifier (SLB9670)

    uint16_t decrypt_key_count;             // Number of available decrypt keys (SLB9670)
    uint32_t decrypt_key_ids[4];            // Decrypt key ids accepted by the TPM (SLB9670)

    uint32_t active_firmware_version;       // Current firmware version (SLB9670)
    uint8_t process_firmware_unique_id;     // Unique id of the firmware currently processed (SLB9670) 

    uint32_t key_group_id;                  // Key group select the correct firmware manifest (SLB9672)
    uint32_t firmware_update_properties;    // Firmware update capability flags (SLB9672)

    uint16_t start_hash_algorithm;          // Manifest hash used when FieldUpgradeStart (SLB9672) (WHen update is running or resume) => restart
    uint16_t start_hash_size;
    uint8_t start_hash_digest[64];

    uint8_t vendor_string[16];              // Full TPM vendor string
};

/********************************************************************************
 * Field upgrade structure definitions
 ********************************************************************************/
#pragma pack(push, 1)
/**
 *  @brief      Message digest structure
 */
typedef struct sMessageDigest_d{
    uint16_t    wSize;
    uint8_t     rgbMessageDigest[32];
} sMessageDigest_d;

/**
 *  @brief      Firmware package structure
 */
typedef struct sFirmwarePackage_d{
    uint32_t                FwPackageIdentifier;
    uint32_t                Version;
    uint32_t                StaleVersion;
} sFirmwarePackage_d;

/**
 *  @brief      Firmware packages structure
 */
typedef struct sFirmwarePackages_d{
    uint16_t            wEntries;
    sFirmwarePackage_d  FirmwarePackage[MAX_NUM_FW_PACKAGES_DEVICE];
} sFirmwarePackages_d;

/**
 *  @brief      Versions structure
 */
typedef struct sVersions_d{
    uint32_t                internal1;
    uint16_t                wEntries;
    uint32_t                Version[8];
} sVersions_d;

/**
 *  @brief      Signed Attributes structure
 */
typedef struct sSignedAttributes_d{
    uint16_t                        internal1;
    uint8_t                         internal2[6];
    uint16_t                        internal3;
    sMessageDigest_d                sMessageDigest;
    sFirmwarePackage_d              sFirmwarePackage;
    uint8_t                         internal6[34];
    uint8_t                         internal7[34];
    uint8_t                         internal8[16];
    uint32_t                        DecryptKeyId;
    uint8_t                         internal10[20];
    sVersions_d                     sVersions;
    uint8_t                         internal12[66];
    uint8_t                         internal13[42];
} sSignedAttributes_d;

/**
 *  @brief      Signer Info structure
 */
typedef struct sSignerInfo_d{
    uint16_t                internal1;
    uint32_t                internal2;
    uint16_t                internal3;
    uint16_t                internal4;
    uint16_t                internal5;
    sSignedAttributes_d     sSignedAttributes;
    uint16_t                internal7;
    uint8_t                 internal8[256];
} sSignerInfo_d;

/**
 *  @brief      Signed Firmware Data Info structure
 */
typedef struct sSignedData_d{
    uint16_t                internal1;
    uint16_t                internal2;
    uint16_t                wSignerInfoSize;
    sSignerInfo_d           sSignerInfo;
} sSignedData_d;

/**
 *  @brief      Security Module Logic structure
 */
typedef struct sSecurityModuleLogic_d{
    uint16_t                    internal1;
    uint32_t                    unTargetHardwareIdentifier;
    uint8_t                     internal3[34];
    sFirmwarePackage_d          sBootloaderFirmwarePackage;
    sFirmwarePackages_d         sFirmwareConfiguration;
} sSecurityModuleLogic_d;

/**
 *  @brief      List of Key Identifier IDs structure
 */
typedef struct sKeyList_d{
    uint16_t            wEntries;
    uint32_t            internal2[4];
    uint32_t            DecryptKeyId[4];
} sKeyList_d;

/**
 *  @brief      Security Module Logic Info structure
 */
typedef struct sSecurityModuleLogicInfo_d{
    uint16_t                internal1;
    uint16_t                wMaxDataSize;
    sSecurityModuleLogic_d  sSecurityModuleLogic;
    uint16_t                SecurityModuleStatus;
    sFirmwarePackage_d      sProcessFirmwarePackage;
    uint16_t                internal6;
    uint8_t                 internal7[6];
    uint16_t                wFieldUpgradeCounter;
} sSecurityModuleLogicInfo_d;

/**
 *  @brief      Security Module Logic Info2 structure
 */
typedef struct sSecurityModuleLogicInfo2_d{
    uint16_t                internal1;                  
    uint16_t                wMaxDataSize;              
    sSecurityModuleLogic_d  sSecurityModuleLogic;       
    uint16_t                SecurityModuleStatus;       
    sFirmwarePackage_d      sProcessFirmwarePackage;    
    uint16_t                internal6;                  
    uint8_t                 internal7[6];               
    uint16_t                wFieldUpgradeCounter;       
    sKeyList_d              sKeyList;                   
} sSecurityModuleLogicInfo2_d;
#pragma pack(pop)