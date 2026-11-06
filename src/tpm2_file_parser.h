/**
 * @brief Declares types and definitons for tar and firmware parser
 * @author Jan Haeussermann
 * 
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

/*****************************************************************************************************/
// Reader structs
/*****************************************************************************************************/

/**
 * @brief Struct for the tar reader 
 */
struct TPMFirmwareReader{
    uint8_t *buffer;
    size_t size;
    size_t offset;
};

/*****************************************************************************************************/
// Reader functions
/*****************************************************************************************************/

/**
 * @brief Returns the remaing number of bytes of the reader
 * 
 * @param reader contains the buffer, size and current pointer into the buffer
 * 
 * @return number of remaining bytes
 */
size_t reader_remaining_size(const TPMFirmwareReader *reader);

/**
 * @brief Reads a uint8 from the reader and returns true when succesful
 * 
 * @param reader where the values is read from
 * 
 * @param value returns the read value
 * 
 * @return true when successful, otherwise false
 */
bool reader_uint8(TPMFirmwareReader *reader, uint8_t *value);

/**
 * @brief Reads a uint8 from the reader and returns true when succesful
 * 
 * @param reader where the values is read from
 * 
 * @param size size of the array
 * 
 * @param value returns the read value
 * 
 * @return true when successful, otherwise false
 */
bool reader_array_uint8(TPMFirmwareReader *reader, size_t size, uint8_t *value);

/**
 * @brief Reads a uint16 from the reader and returns true when succesful
 * 
 * @param reader where the values is read from
 * 
 * @param value returns the read value
 * 
 * @return true when successful, otherwise false
 */
bool reader_uint16(TPMFirmwareReader *reader, uint16_t *value);

/**
 * @brief Reads a uint16 from the reader and returns true when succesful
 * 
 * @param reader where the values is read from
 * 
 * @param size size of the array
 * 
 * @param value returns the read value
 * 
 * @return true when successful, otherwise false
 */
bool reader_array_uint16(TPMFirmwareReader *reader, size_t size, uint16_t *value);

/**
 * @brief Reads a uint16 from the reader and returns true when succesful
 * 
 * @param reader where the values is read from
 * 
 * @param value returns the read value
 * 
 * @return true when successful, otherwise false
 */
bool reader_uint32(TPMFirmwareReader *reader, uint32_t *value);

/**
 * @brief Reads a uint32 from the reader and returns true when succesful
 * 
 * @param reader where the values is read from
 * 
 * @param size size of the array
 * 
 * @param value returns the read value
 * 
 * @return true when successful, otherwise false
 */
bool reader_array_uint32(TPMFirmwareReader *reader, size_t size, uint32_t *value);

/**
 * @brief Returns a pointer to a number of bytes and advances the reader
 * 
 * @param reader Reader to read from
 * 
 * @param size Number of bytes
 * 
 * @param value Receives a pointer into the buffer
 * 
 * @return true when successful, otherwise false
 */
bool reader_pointer(TPMFirmwareReader *reader, size_t size, uint8_t **value);

/**
 * @brief skips a number of bytes
 * 
 * @param reader to read from
 * 
 * @param number of bytes to skip
 * 
 * @return true when successful, otherwise false
 */
bool reader_skip(TPMFirmwareReader *reader, size_t number);

/**
 * @brief Checks if a uint16_t has the correct value
 * 
 * @param c Value to check
 * 
 * @return true when correct, otherwise false
 * 
 * @note From Infineon "Common/FirmwareImage.c" changed some types to be more compatible with the parser
 */
bool verify_valid_char(uint16_t c);

/**
 * @brief Reads a wchar and returns it as a uint16_t
 * 
 * @param reader to read from
 * 
 * @param value resulting value from teh read
 * 
 * @return true when successful, otherwise false
 * 
 * @note from Source/Common/Platform/Linux/Platform.c
 */
bool reader_utf16(TPMFirmwareReader *reader, uint16_t *value);

/**
 * @brief Reads an utf16 string converts it to char with the allowed values. It stops when the value buffer is full or the terminator is read
 * 
 * @param reader to read from
 * 
 * @param str_len the length of the string to read
 * 
 * @param value_buf_len the length of the value buffer
 * 
 * @param value the buffer where the string (char) is saved
 * 
 * @param value_len the lenght of the saved string
 * 
 * @return true when successful, otherwise false
 * 
 * @note In the FirmwareImage they only allow [a..z], [A..Z], [0..9], [_.] charters. For that reason it is casted into a char
 */
bool reader_utf16_string(TPMFirmwareReader *reader, size_t str_len, size_t value_buf_len, char *value, size_t *value_len);

/*****************************************************************************************************/
// Parser function
/*****************************************************************************************************/

/**
 * @brief Parses a binary file into a buffer
 * 
 * @param bin_file_path Path to the binary file
 * 
 * @param reader Struct to save the buffer and size
 * 
 * @param verbose_print Enables verbose output when set to true
 * 
 * @return true when successful, otherwise false
 */
bool parse_bin(const char* bin_file_path, struct TPMFirmwareReader *reader, bool verbose_print);

/*****************************************************************************************************/
// Infineon blob parser structs
/*****************************************************************************************************/

/**
 * @struct TPMFirmwareImage
 * 
 * @note Is the from Infineon document "/TPMFactoryUpd/StdInclude.h" and used for the firmware image parser
 */
struct TPMFirmwareGuid{ 
    uint32_t data1;
    uint16_t data2;
    uint16_t data3;
    uint8_t data4[8];
};

/**
 * Struct from Infineon document "Common/FirmwareImage.h" changed some types to be more compatible with the parser
 */
struct TPMFirmwareImage{
    /// Unique identifier: Must be EFI_IFXTPM_FIRMWARE_IMAGE_GUID or higher.
    /// A different GUID indicates a breaking change the in firmware image file structure and a newer version of the tool is required
    /// to process the firmware image.
    TPMFirmwareGuid unique;
    /// Firmware image version (e.g. V1...V3) derived from GUID
    uint8_t bImageVersion;
    /// Allowed TPM family where the firmware image can be installed on. Either DEVICE_TYPE_TPM_12 or DEVICE_TYPE_TPM_20.
    uint8_t bSourceTpmFamily;
    /// Count of the allowed source versions
    uint16_t usSourceVersionsCount;
    /// Size in bytes of rgwszSourceVersions. Uses big-endian format.
    uint16_t usSourceVersionsSize;
    /// Allowed versions where the firmware update can be applied to. Array of version names (e.g. "5.12.3456.0") with additional null termination (multi string).
    /// Size of multi string array is usSourceVersionsSize.
    uint8_t* prgwszSourceVersions;
    /// Target TPM family. Either DEVICE_TYPE_TPM_12 or DEVICE_TYPE_TPM_20.
    uint8_t bTargetTpmFamily;
    /// Size in bytes of wszTargetVersion. Uses big-endian format.
    uint16_t usTargetVersionSize;
    /// Target version name (e.g. "5.12.3456.0") wszTargetVersion includes a terminating NULL character.
    /// Size of wszTargetVersion is usTargetVersionSize.
    char wszTargetVersion[65]; 
    /// Size in bytes of the policy parameter block in rgbPolicyParameterBlock. Uses big-endian format.
    uint32_t unManifestDataSize;
    /// Manifest data. Includes one or more manifest data elements.
    /// Size is unManifestDataSize.
    uint8_t* rgbManifestData;
    /// Number of manifests included within rgbManifestData. Uses big-endian format.
    uint16_t usNumManifests;
    /// Key group id. Uses big-endian format.
    uint32_t unKeyGroupId;
    /// Size in bytes of the parameter block in rgbPolicyParameterBlock. Uses big-endian format.
    uint16_t usPolicyParameterBlockSize;
    /// Parameter block. Either points to a manifest block or to a legacy policy parameter block.
    /// Size is usParameterBlockSize.
    uint8_t* rgbPolicyParameterBlock;
    /// Size in bytes of the firmware block in rgbFirmware. Uses big-endian format.
    uint32_t unFirmwareSize;
    /// Firmware block. Size is unFirmwareSize.
    uint8_t* rgbFirmware;
    /// Version number indicating the structure version of the firmware image. Will be increased with every non-breaking firmware image file structure change.
    uint16_t usImageStructureVersion;
    /// Target state
    uint32_t bfTargetState;
    /// Firmware image capabilities
    uint32_t bfCapabilities;
    /// Count of the allowed source versions (unsigned integer representation)
    uint16_t usIntSourceVersionCount;
    /// Allowed source versions (unsigned integer representation) only used for V1 and V2
    uint32_t rgunIntSourceVersions[8];
    /// Checksum over the IfxFirmwareImage structure excluding the unChecksum field. Uses big-endian format.
    uint32_t unChecksum;
    /// Key identifier for signature
    uint16_t usSignatureKeyId;
    /// Size in bytes of rgbSalesCode
    uint16_t usSalesCodeSize;
    /// TPM sales code maximum 16 bytes (added with image struct version 5; maybe not set at all).
    /// Depending on the sales code size the complete sales code or less bytes are checked (starts with).
    uint8_t rgbSalesCode[16];
    /// Byte array for signature
    uint8_t rgbSignature[256];
};

/**
 * @brief Struct for the firmware image file. It contains the raw image blob, its size, the tar entry name, and the parsed firmware image.
 */
struct TPMFirmwareImageFile{
    uint8_t                 *image_blob;
    size_t                  image_blob_size;
    char                    tar_entry_name[257];
    TPMFirmwareImage        image;
};

/*****************************************************************************************************/
// Infineon blob parser constants
/*****************************************************************************************************/

static const TPMFirmwareGuid EFI_IFXTPM_FIRMWARE_IMAGE_GUID =
{ 0x1a53667a, 0xfb12, 0x479e, { 0xac, 0x58, 0xec, 0x99, 0x58, 0x86, 0x10, 0x93 } }; // Common/FirmwareImage.h

// Definitions for firmware image version
#define FIRMWARE_IMAGE_V1 0x01
#define FIRMWARE_IMAGE_V2 0x02
#define FIRMWARE_IMAGE_V3 0x03

// Definitions for firmware image struct version
#define FIRMWARE_IMAGE_STRUCT_V1 0x01
#define FIRMWARE_IMAGE_STRUCT_V2 0x02
#define FIRMWARE_IMAGE_STRUCT_V3 0x03
#define FIRMWARE_IMAGE_STRUCT_V4 0x04
#define FIRMWARE_IMAGE_STRUCT_V5 0x05

// The maximum supported number of source TPM firmware versions in an V1 and V2 image file
#define MAX_SOURCE_VERSIONS_COUNT 8
// The maximum supported number of source TPM firmware versions in V3 image file
#define MAX_SOURCE_VERSIONS_COUNT_V3 32

/*****************************************************************************************************/
// Infineon blob parser functions
/*****************************************************************************************************/

/**
 * @brief Parses the signed data from the firmware blob
 * 
 * @param policy_reader Reader to read from
 * 
 * @param signed_data Struct to save the parsed data
 * 
 * @return true when successful, otherwise false
 */
bool tpm_parse_signeddata(TPMFirmwareReader *policy_reader, sSignedData_d *signed_data);

/**
 * @brief Parses a Infineon firmware image into a structure
 * 
 * @param reader the buffer to read from 
 * 
 * @param image the parsed data in the image
 * 
 * @param verbose_print flag to enable verbose output
 * 
 * @return true if no errors, false otherwise
 * 
 * @note from Source/Common/FirmwareImage.c
 */
bool tpm_parse_firmware_image(TPMFirmwareReader *reader, TPMFirmwareImage *image, bool verbose_print);