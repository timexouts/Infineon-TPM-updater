/**
 * @brief Parses tar and firmware file to the required structs
 * @author Jan Haeussermann
 */

#include <iostream>
#include <cstring>

#include <tss2/tss2_esys.h>
#include <tss2/tss2_sys.h>
#include <tss2/tss2_tpm2_types.h>
#include <tss2_tpm2_types.h>
#include <tss2/tss2_tcti_device.h>

#include "endian_helper.h"

#include <fstream>

#include "tpm2_types.h"
#include "tpm2_file_parser.h"

/*****************************************************************************************************/
// Tar parser functions
/*****************************************************************************************************/

/**
 * @brief Parses a binary file into a buffer
 * 
 * @param bin_file_path Path to the binary file
 * 
 * @param reader Struct to save the buffer and size
 * 
 * @return true when successful, otherwise false
 */
bool parse_bin(const char* bin_file_path, struct TPMFirmwareReader *reader, bool verbose_print){
	if(bin_file_path == NULL || reader == NULL){
		std::cerr << "Error: Invalid parse_bin argument" << std::endl;
		return false;
	}

	if(verbose_print){
		std::cout << "Parsing binary file: " << bin_file_path << std::endl;
	}
	FILE *file = fopen(bin_file_path, "rb");
	if(file == NULL){
		std::cerr << "Error: File not found" << std::endl;
		return false;
	}

	// Get the size of the file
	fseek(file, 0, SEEK_END);
	size_t size_firmware = ftell(file);
	fseek(file, 0, SEEK_SET);

	if(size_firmware == 0){
		std::cerr << "Error: File size is zero" << std::endl;
		fclose(file);
		return false;
	}

	// Initialize the reader struct
	reader->offset = 0;
	reader->size = size_firmware;
	reader->buffer = (uint8_t *) malloc(size_firmware);
	if(reader->buffer == NULL){
		std::cerr << "Error: Unable to allocate memory for firmware data" << std::endl;
		fclose(file);
		return false;
	}

	// Read the firmware data
	size_t bytes_read = fread(reader->buffer, 1, size_firmware, file);
	if(bytes_read < size_firmware){
		std::cerr << "Error: Unable to read firmware data" << std::endl;
		free(reader->buffer);
		reader->buffer = NULL;
		fclose(file);
		return false;
	}

	fclose(file);
	if(verbose_print){
		std::cout << "Successfully read " << bytes_read << " bytes from " << bin_file_path << std::endl;
	}
	return true;
}
/*****************************************************************************************************/
// Reader helper function
/*****************************************************************************************************/

/**
 * @brief Returns the remaing number of bytes of the reader
 * 
 * @param reader contains the buffer, size and current pointer into the buffer
 * 
 * @return number of remaining bytes
 */
size_t reader_remaining_size(const TPMFirmwareReader *reader){
	if(reader == NULL || reader->buffer == NULL){
		std::cerr << "Error: In reader_remaining_size reader or buffer is NULL" << std::endl;
		return 0;
	}

	if(reader->offset > reader->size){
		return 0;
	}

	return reader->size - reader->offset;
}

/**
 * @brief Reads a uint8 from the reader and returns true when succesful
 * 
 * @param reader where the values is read from
 * 
 * @param value returns the read value
 * 
 * @return true when successful, otherwise false
 */
bool reader_uint8(TPMFirmwareReader *reader, uint8_t *value){
	if(reader == NULL || reader->buffer == NULL || value == NULL){
		std::cerr << "Error: In reader_uint8 reader, reader buffer, or value is NULL" << std::endl;
		return false;
	}

	if(reader_remaining_size(reader) == 0){
		std::cerr << "Error: Reader does not contain enough bytes" << std::endl;
		return false;
	}

	*value = big_endian_to_host(*(reader->buffer + reader->offset));
	reader->offset++;

	return true;
}

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
bool reader_array_uint8(TPMFirmwareReader *reader, size_t size, uint8_t *value){
	if(reader == NULL || reader->buffer == NULL || value == NULL){
		std::cerr << "Error: In reader_array_uint8 reader, reader buffer, or value is NULL" << std::endl;
		return false;
	}

	if(reader_remaining_size(reader) < size){
		std::cerr << "Error: Reader does not contain enough bytes" << std::endl;
		return false;
	}

	for(size_t i = 0; i < size; i++){
		if(!reader_uint8(reader, &value[i])){
			return false;
		}
	}

	return true;
}

/**
 * @brief Reads a uint16 from the reader and returns true when succesful
 * 
 * @param reader where the values is read from
 * 
 * @param value returns the read value
 * 
 * @return true when successful, otherwise false
 */
bool reader_uint16(TPMFirmwareReader *reader, uint16_t *value){
	if(reader == NULL || reader->buffer == NULL || value == NULL){
		std::cerr << "Error: In reader_uint16 reader, reader buffer, or value is NULL" << std::endl;
		return false;
	}

	if(reader_remaining_size(reader) < 2){
		std::cerr << "Error: Reader does not contain enough bytes" << std::endl;
		return false;
	}

	*value = ((uint16_t)(*(reader->buffer + reader->offset))) << 8;
	*value |= (uint16_t)*(reader->buffer + reader->offset + 1);
	reader->offset += 2;

	return true;
}

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
bool reader_array_uint16(TPMFirmwareReader *reader, size_t size, uint16_t *value){
	if(reader == NULL || reader->buffer == NULL || value == NULL){
		std::cerr << "Error: In reader_array_uint16 reader, reader buffer, or value is NULL" << std::endl;
		return false;
	}

	if(reader_remaining_size(reader) < size * 2){ // size * sizeof(uint16_t)
		std::cerr << "Error: Reader does not contain enough bytes" << std::endl;
		return false;
	}

	for(size_t i = 0; i < size; i++){
		if(!reader_uint16(reader, &value[i])){
			return false;
		}
	}

	return true;
}

/**
 * @brief Reads a uint16 from the reader and returns true when succesful
 * 
 * @param reader where the values is read from
 * 
 * @param value returns the read value
 * 
 * @return true when successful, otherwise false
 */
bool reader_uint32(TPMFirmwareReader *reader, uint32_t *value){
	if(reader == NULL || reader->buffer == NULL || value == NULL){
		std::cerr << "Error: In reader_uint32 reader, reader buffer, or value is NULL" << std::endl;
		return false;
	}

	if(reader_remaining_size(reader) < 4){
		std::cerr << "Error: Reader does not contain enough bytes" << std::endl;
		return false;
	}

	*value = ((uint32_t)(*(reader->buffer + reader->offset))) << 24;
	*value |= ((uint32_t)(*(reader->buffer + reader->offset + 1))) << 16;
	*value |= ((uint32_t)(*(reader->buffer + reader->offset + 2))) << 8;
	*value |= (uint32_t)*(reader->buffer + reader->offset + 3);
	reader->offset += 4;

	return true;
}

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
bool reader_array_uint32(TPMFirmwareReader *reader, size_t size, uint32_t *value){
	if(reader == NULL || reader->buffer == NULL || value == NULL){
		std::cerr << "Error: In reader_array_uint32 reader, reader buffer, or value is NULL" << std::endl;
		return false;
	}

	if(reader_remaining_size(reader) < size * 4){ // size * sizeof(uint16_t)
		std::cerr << "Error: Reader does not contain enough bytes" << std::endl;
		return false;
	}

	for(size_t i = 0; i < size; i++){
		if(!reader_uint32(reader, &value[i])){
			return false;
		}
	}

	return true;
}

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
bool reader_pointer(TPMFirmwareReader *reader, size_t size, uint8_t **value){
	if(reader == NULL || reader->buffer == NULL || value == NULL){
		std::cerr << "Error: In reader_pointer invalid argument" << std::endl;
		return false;
	}

	if(reader_remaining_size(reader) < size){
		std::cerr << "Error: Reader does not contain enough bytes" << std::endl;
		return false;
	}

	*value = reader->buffer + reader->offset;
	reader->offset += size;

	return true;
}

/**
 * @brief skips a number of bytes
 * 
 * @param reader to read from
 * 
 * @param number of bytes to skip
 * 
 * @return true when successful, otherwise false
 */
bool reader_skip(TPMFirmwareReader *reader, size_t number){
	if(reader == NULL || reader->buffer == NULL){
		std::cerr << "Error: In reader_skip invalid argument" << std::endl;
		return false;
	}

	if(reader_remaining_size(reader) < number){
		std::cerr << "Error: Reader does not contain enough bytes" << std::endl;
		return false;
	}

	reader->offset += number;
	
	return true;
}

/**
 * @brief Checks if a uint16_t has the correct value
 * 
 * @param c Value to check
 * 
 * @return true when correct, otherwise false
 * 
 * @note From Infineon "Common/FirmwareImage.c" changed some types to be more compatible with the parser
 */
bool verify_valid_char(uint16_t c){
	return ((c >= (uint16_t) 'A' && c <= (uint16_t) 'Z') ||
		(c >= (uint16_t) 'a' && c <= (uint16_t) 'z') ||
		(c >= (uint16_t) '0' && c <= (uint16_t) '9') ||
		(c == (uint16_t) '_') || (c == (uint16_t) '.'));
}

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
bool reader_utf16(TPMFirmwareReader *reader, uint16_t *value){
	if(reader == NULL || reader->buffer == NULL || value == NULL){
		std::cerr << "Error: In reader_utf16 reader, buffer, or value is NULL" << std::endl;
		return false;
	}

	if(reader_remaining_size(reader) < 2){
		std::cerr << "Error: Buffer does not contain enough bytes" << std::endl;
		return false;
	} 

	uint8_t *temp = reader->buffer + reader->offset;
	*value = ((uint16_t) temp[0]) | (((uint16_t) temp[1]) << 8);
	reader->offset += 2;

	return true;
}

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
bool reader_utf16_string(TPMFirmwareReader *reader, size_t str_len, size_t value_buf_len, char *value, size_t *value_len){
	if(reader == NULL || reader->buffer == NULL || value == NULL || value_len == NULL){
		std::cerr << "Error: In reader_utf16_string reader, buffer, value, or value_len is NULL" << std::endl;
		return false;
	}

	if(value_buf_len == 0){
		std::cerr << "value array is of size 0" << std::endl;
		return false;
	}

	if(str_len == 0 || str_len % 2 != 0){
		std::cerr << "Error: Invalid UTF-16 field size" << std::endl;
		return false;
	}

	if(reader_remaining_size(reader) < str_len){
		std::cerr << "Error: Reader does not contain enough bytes for string size" << std::endl;
		return false;
	}

	uint16_t wchar = 0;
	size_t chars_written = 0;
	size_t utf16_count = str_len / 2;
	for(size_t i = 0; i < utf16_count; i++){
		if(!reader_utf16(reader, &wchar)){
			return false;
		}

		if(wchar != 0 && !verify_valid_char(wchar)){
			std::cerr << "Error: Not valid char" << std::endl;
			return false;
		}

		if(wchar == 0){
			break;
		}

		if(chars_written >= value_buf_len -1){
			std::cerr << "Error: Destination string buffer is too small" << std::endl;
			return false;
		}

		value[chars_written++] = static_cast<char>(wchar);
	}

	value[chars_written] = '\0';
	*value_len = chars_written;

	return true;
}

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
bool tpm_parse_signeddata(TPMFirmwareReader *policy_reader, sSignedData_d *signed_data){
	// Unmarshal sSignedData
		if(!reader_uint16(policy_reader, &signed_data->internal1)){
			std::cerr << "Error: Failed to unmarshal signed_data->internal1" << std::endl;
			return false;
		}

		if(!reader_uint16(policy_reader, &signed_data->internal2)){
			std::cerr << "Error: Failed to unmarshal signed_data->internal2" << std::endl;
			return false;
		}

		if(!reader_uint16(policy_reader, &signed_data->wSignerInfoSize)){
			std::cerr << "Error: Failed to unmarshal signed_data->wSignerInfoSize" << std::endl;
			return false;
		}

		// unmarshal SignerInfo
		sSignerInfo_d *signer_info = &signed_data->sSignerInfo;

		if(!reader_uint16(policy_reader, &signer_info->internal1)){
			std::cerr << "Error: Failed to unmarshal signer_info->internal1" << std::endl;
			return false;
		}

		if(!reader_uint32(policy_reader, &signer_info->internal2)){
			std::cerr << "Error: Failed to unmarshal signer_info->internal2" << std::endl;
			return false;
		}

		if(!reader_uint16(policy_reader, &signer_info->internal3)){
			std::cerr << "Error: Failed to unmarshal signer_info->internal3" << std::endl;
			return false;
		}

		if(!reader_uint16(policy_reader, &signer_info->internal4)){
			std::cerr << "Error: Failed to unmarshal signer_info->internal4" << std::endl;
			return false;
		}

		if(!reader_uint16(policy_reader, &signer_info->internal5)){
			std::cerr << "Error: Failed to unmarshal signer_info->internal5" << std::endl;
			return false;
		}

		// unmarshal sSignedAttributes
		sSignedAttributes_d *signed_attributes = &signer_info->sSignedAttributes;

		if(!reader_uint16(policy_reader, &signed_attributes->internal1)){
			std::cerr << "Error: Failed to unmarshal signed_attributes->internal1" << std::endl;
			return false;
		}

		if(!reader_array_uint8(policy_reader, sizeof(signed_attributes->internal2), signed_attributes->internal2)){
			std::cerr << "Error: Failed to unmarshal signed_attributes->internal2" << std::endl;
			return false;
		}

		if(!reader_uint16(policy_reader, &signed_attributes->internal3)){
			std::cerr << "Error: Failed to unmarshal signed_attributes->internal3" << std::endl;
			return false;
		}

		// Unmarshal sMessageDigest
		sMessageDigest_d *message_digest = &signed_attributes->sMessageDigest;

		if(!reader_uint16(policy_reader, &message_digest->wSize)){
			std::cerr << "Error: Failed to unmarshal message_digest->wSize" << std::endl;
			return false;
		}

		if(message_digest->wSize > sizeof(message_digest->rgbMessageDigest)){
			std::cerr << "Error: message_digest->wSize is too large" << std::endl;
			return false;
		}

		if(!reader_array_uint8(policy_reader, message_digest->wSize, message_digest->rgbMessageDigest)){
			std::cerr << "Error: Failed to unmarshal message_digest->rgbMessageDigest" << std::endl;
			return false;
		}

		// Done sMessageDigest

		// Unmarshal sFirmwarePackage
		sFirmwarePackage_d *firmware_package = &signed_attributes->sFirmwarePackage;

		if(!reader_uint32(policy_reader, &firmware_package->FwPackageIdentifier)){
			std::cerr << "Error: Failed to unmarshal firmware_package->FwPackageIdentifier" << std::endl;
			return false;
		}

		if(!reader_uint32(policy_reader, &firmware_package->Version)){
			std::cerr << "Error: Failed to unmarshal firmware_package->Version" << std::endl;
			return false;
		}

		if(!reader_uint32(policy_reader, &firmware_package->StaleVersion)){
			std::cerr << "Error: Failed to unmarshal firmware_package->StaleVersion" << std::endl;
			return false;
		}

		// Done sFirmwarePackage

		if(!reader_array_uint8(policy_reader, sizeof(signed_attributes->internal6), signed_attributes->internal6)){
			std::cerr << "Error: Failed to unmarshal signed_attributes->internal6" << std::endl;
			return false;
		}

		if(!reader_array_uint8(policy_reader, sizeof(signed_attributes->internal7), signed_attributes->internal7)){
			std::cerr << "Error: Failed to unmarshal signed_attributes->internal7" << std::endl;
			return false;
		}

		if(!reader_array_uint8(policy_reader, sizeof(signed_attributes->internal8), signed_attributes->internal8)){
			std::cerr << "Error: Failed to unmarshal signed_attributes->internal8" << std::endl;
			return false;
		}

		if(!reader_uint32(policy_reader, &signed_attributes->DecryptKeyId)){
			std::cerr << "Error: Failed to unmarshal signed_attributes->internal7" << std::endl;
			return false;
		}

		if(!reader_array_uint8(policy_reader, sizeof(signed_attributes->internal10), signed_attributes->internal10)){
			std::cerr << "Error: Failed to unmarshal signed_attributes->internal10" << std::endl;
			return false;
		}

		// Unmarshal sVersions
		sVersions_d *version = &signed_attributes->sVersions;

		if(!reader_uint32(policy_reader, &version->internal1)){
			std::cerr << "Error: Failed to unmarshal version->internal1" << std::endl;
			return false;
		}

		if(!reader_uint16(policy_reader, &version->wEntries)){
			std::cerr << "Error: Failed to unmarshal version->wEntries" << std::endl;
			return false;
		}

		if(!reader_array_uint32(policy_reader, sizeof(version->Version)/sizeof(uint32_t), version->Version)){
			std::cerr << "Error: Failed to unmarshal version->wEntries" << std::endl;
			return false;
		}

		// Done sVersions

		if(!reader_array_uint8(policy_reader, sizeof(signed_attributes->internal12), signed_attributes->internal12)){
			std::cerr << "Error: Failed to unmarshal signed_attributes->internal12" << std::endl;
			return false;
		}

		if(!reader_array_uint8(policy_reader, sizeof(signed_attributes->internal13), signed_attributes->internal13)){
			std::cerr << "Error: Failed to unmarshal signed_attributes->internal13" << std::endl;
			return false;
		}

		// Done sSignedAttributes

		if(!reader_uint16(policy_reader, &signer_info->internal7)){
			std::cerr << "Error: Failed to unmarshal signer_info->internal7" << std::endl;
			return false;
		}

		if(!reader_array_uint8(policy_reader, sizeof(signer_info->internal8), signer_info->internal8)){
			std::cerr << reader_remaining_size(policy_reader) << std::endl;
			std::cerr << "Error: Failed to unmarshal signer_info->internal8" << std::endl;
			return false;
		}
		// Done SignerInfo
		// Done SignerData
		return true;
}

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
bool tpm_parse_firmware_image(TPMFirmwareReader *reader, TPMFirmwareImage *image, bool verbose_print){
	if(reader == NULL || image == NULL){
		std::cerr << "Error: reader or image pointer is NULL" << std::endl;
		return false;
	}

	if(verbose_print){
		std::cout << "Parsing firmware image" << std::endl;
	}

	memset(image, 0, sizeof(TPMFirmwareImage));

	// TPMFirmwareGuid data 
	TPMFirmwareGuid *guid = &image->unique;
	if(!reader_uint32(reader, &guid->data1)){
		std::cerr << "Error: Failed to read unique.data1" << std::endl;
		return false;
	}

	if(!reader_uint16(reader, &guid->data2)){
		std::cerr << "Error: Failed to read unique.data2" << std::endl;
		return false;
	}

	if(!reader_uint16(reader, &guid->data3)){
		std::cerr << "Error: Failed to read unique.data3" << std::endl;
		return false;
	}

	if(!reader_array_uint8(reader, sizeof(guid->data4), guid->data4)){
		std::cerr << "Error: Failed to read unique.data4" << std::endl;
		return false;
	}
	
	// Check GUID is supported
	TPMFirmwareGuid guid_temp = image->unique;
	guid_temp.data4[7] = EFI_IFXTPM_FIRMWARE_IMAGE_GUID.data4[7]; // Ignore the last byte for comparison
	if(memcmp(&guid_temp, &EFI_IFXTPM_FIRMWARE_IMAGE_GUID, sizeof(TPMFirmwareGuid)) != 0){
		std::cerr << "Error: Firmware image GUID, RC_E_CORRUPT_FW_IMAGE" << std::endl;
		return false;
	}

	// Check the firmware image version
	image->bImageVersion = static_cast<uint8_t>(image->unique.data4[7] - EFI_IFXTPM_FIRMWARE_IMAGE_GUID.data4[7] + 1);
	if(image->bImageVersion < FIRMWARE_IMAGE_V1 || image->bImageVersion > FIRMWARE_IMAGE_V3){
		std::cerr << "Error: Not supported image version detected" << std::endl;
		return false;
	}

	// Check for preliminary V2
	if(image->bImageVersion == FIRMWARE_IMAGE_V2 && reader->buffer[reader->offset] == 0){
		image->bImageVersion = FIRMWARE_IMAGE_V3;
	}

	// Get image struct version from V3
	if(image->bImageVersion == FIRMWARE_IMAGE_V3){
		if(!reader_uint16(reader, &image->usImageStructureVersion)){
			std::cerr << "Error: Failed to read image structure version" << std::endl;
			return false;
		}
	}

	if(!reader_uint8(reader, &image->bSourceTpmFamily)){
		std::cerr << "Error: Failed to read source TPM family" << std::endl;
		return false;
	}

	// single source version strings
	uint16_t usSourceVersionsSize = 0;
	int nBufferSize = 0;

	if(!reader_uint16(reader, &usSourceVersionsSize)){
		std::cerr << "Error: Failed to read source versions count" << std::endl;
		return false;
	}

	if(usSourceVersionsSize > reader_remaining_size(reader)){
		std::cerr << "Error: Buffer to small for source versions" << std::endl;
		return false;
	}

	// Get maximum allowed source version
	uint16_t usSourceVersionsCountMax = MAX_SOURCE_VERSIONS_COUNT;
	if(image->bImageVersion > FIRMWARE_IMAGE_V2){
		usSourceVersionsCountMax = MAX_SOURCE_VERSIONS_COUNT_V3;
	}

	// Source versionss
	image->usSourceVersionsSize = usSourceVersionsSize;
	image->prgwszSourceVersions = reader->buffer + reader->offset; // Is NULL terminated multi string, so no need to copy it
	nBufferSize = (int)usSourceVersionsSize;

	image->usSourceVersionsCount = 0;
	TPMFirmwareReader source_reader{
		reader->buffer + reader->offset,
		(size_t) usSourceVersionsSize,
		0
	};
	
	do{
		char wsz_buffer[256] = {0};
		size_t chars_written = 0;
		memset(wsz_buffer, 0, sizeof(wsz_buffer));

		if(!reader_utf16_string(&source_reader, nBufferSize, sizeof(wsz_buffer), wsz_buffer, &chars_written)){
			std::cerr << "Error: Failed to read utf16 string" << std::endl;
			return false;
		}

		// Check the unmarshalled string
		if(chars_written != 0){
			if(chars_written > 64){
				std::cerr << "Error: size of the source version is to large" << std::endl;
				return false;
			}

			image->usSourceVersionsCount++;
			nBufferSize -= static_cast<int>((chars_written + 1) * sizeof(uint16_t)); // From the Common/FirmwareImage.c
		}
	
	}while((source_reader.buffer[source_reader.offset] != 0x00 || source_reader.buffer[source_reader.offset + 1] != 0x00) && reader_remaining_size(&source_reader) > 0 && 
			image->usSourceVersionsCount < usSourceVersionsCountMax);

	if(reader_remaining_size(&source_reader) != 2){
		std::cerr << "Error: Should be two bytes left of source version string" << std::endl;
		return false;
	}

	uint8_t terminator1;
	uint8_t terminator2;
	if(!reader_uint8(&source_reader, &terminator1) || !reader_uint8(&source_reader, &terminator2)){
		std::cerr << "Error: Could not read 2 bytes" << std::endl;
		return false;
	}

	if(terminator1 != 0x00 || terminator2 != 0x00){
		std::cerr << "Error: Last two bytes should be 0" << std::endl;
		return false;
	}

	// Skip the source version string
	if(!reader_skip(reader, usSourceVersionsSize)){
		std::cerr << "Error: Could not skip source version string" << std::endl;
		return false;
	}


	// Unmarshal target TPM family and target version
	if(!reader_uint8(reader, &image->bTargetTpmFamily)){
		std::cerr << "Error: Failed to read bTargetTpmFamily" << std::endl;
		return false;
	}

	if(!reader_uint16(reader, &image->usTargetVersionSize)){
		std::cerr << "Error: Failed to read usTargetVersionSize" << std::endl;
		return false;
	}

	if(image->usTargetVersionSize > reader_remaining_size(reader)){
		std::cerr << "Error: buffer for usTargetVersionSize to small" << std::endl;
		return false;
	}

	if(image->usTargetVersionSize > 64){
		std::cerr << "Error: TargetVersionSize is to large" << std::endl;
		return false;
	}

	// Unmarshal target version
	char wsz_buffer[256] = {0};
	size_t chars_written = 0;
	TPMFirmwareReader target_reader{
		reader->buffer + reader->offset,
		image->usTargetVersionSize,
		0
	};

	if(!reader_utf16_string(&target_reader, image->usTargetVersionSize, sizeof(wsz_buffer), wsz_buffer, &chars_written)){
		std::cerr << "Error: Target string could not be read correct" << std::endl;
		return false;
	}

	memcpy(image->wszTargetVersion, wsz_buffer, chars_written + 1);

	if(!reader_skip(reader, image->usTargetVersionSize)){
		return false;
	}

	// unmarshal parameter block size (uint32 or uint16)
	uint32_t parameter_block = 0;
	if(image->bImageVersion >= FIRMWARE_IMAGE_V3){
		if(!reader_uint32(reader, &parameter_block)){
			std::cerr << "Error: could not read parameter block size" << std::endl;
			return false;
		}
	} else{
		if(!reader_uint16(reader, &image->usPolicyParameterBlockSize)){
			std::cerr << "Error: Could not read parameter block size" << std::endl;
			return false;
		}
		parameter_block = (uint32_t) image->usPolicyParameterBlockSize;
	}

	if(parameter_block > (uint32_t) reader_remaining_size(reader)){
		std::cerr << "Error: Buffer is to small for parameterblock" << std::endl;
		return false;
	}

	if(!reader_pointer(reader, parameter_block, &image->rgbPolicyParameterBlock)){
		std::cerr << "Error: Could not read parameter block"
				<< std::endl;
		return false;
	}	

	// Get manifest information (not V1)
	if(image->bImageVersion >= FIRMWARE_IMAGE_V2){

		image->unManifestDataSize = parameter_block;
		image->rgbManifestData = image->rgbPolicyParameterBlock;
		
		if(image->bImageVersion == FIRMWARE_IMAGE_V2){
			uint16_t padding_bytes = 0;
			TPMFirmwareReader manifest_reader{
				image->rgbManifestData,
				image->unManifestDataSize,
				0
			};
			
			if(!reader_uint16(&manifest_reader, &padding_bytes)){
				std::cerr << "Error: reading padding size" << std::endl;
				return false;
			}

			if(padding_bytes >= reader_remaining_size(&manifest_reader)){
				std::cerr << "Error: Padding is equal to or larger than manifest data" << std::endl;
				return false;
			}

			if(!reader_skip(&manifest_reader, padding_bytes)){
				std::cerr << "Error: Could not skip padding bytes" << std::endl;
				return false;
			}

			image->rgbManifestData = manifest_reader.buffer + manifest_reader.offset;
			image->unManifestDataSize = static_cast<uint32_t>(reader_remaining_size(&manifest_reader));
		}
		image->usPolicyParameterBlockSize = 0;
		image->rgbPolicyParameterBlock = NULL;
		std::cerr << "Warning: Policy parameter block is not used for >= VERSION_3 images" << std::endl;
	} else{
		TPMFirmwareReader policy_reader{
			image->rgbPolicyParameterBlock,
			image->usPolicyParameterBlockSize,
			0
		};
		// Get allowed versions from parameter block and store them in rgunIntSourceVersions
		// For >= VERSION_3 images the data will be overwritten with section retrieved from metadata.
		sSignedData_d signed_data = {};
		if(!tpm_parse_signeddata(&policy_reader, &signed_data)){
			std::cerr << "Error: Could not parse the signed data" << std::endl;
			return false;
		}

		image->usIntSourceVersionCount = signed_data.sSignerInfo.sSignedAttributes.sVersions.wEntries;
		for(size_t i = 0; i < MAX_SOURCE_VERSIONS_COUNT; i++){
			if(i < image->usIntSourceVersionCount){
				image->rgunIntSourceVersions[i] = signed_data.sSignerInfo.sSignedAttributes.sVersions.Version[i];
			} else{
				image->rgunIntSourceVersions[i] = 0;
			}
		}

	}

	if(!reader_uint32(reader, &image->unFirmwareSize)){
		std::cerr << "Error: Could not read FirmwareSize" << std::endl;
		return false;
	}

	if((size_t) image->unFirmwareSize > reader_remaining_size(reader)){
		std::cerr << "Error: FirmwareSize is larger then buffer" << std::endl;
		return false;
	}

	if(!reader_pointer(reader, image->unFirmwareSize, &image->rgbFirmware)){
		std::cerr << "Error: Could not give pointer to rgbFirmware" << std::endl;
		return false;
	}

	if(reader_remaining_size(reader) > sizeof(image->unChecksum)){
		uint16_t additional_data_size = 0;
		if(!reader_uint16(reader, &additional_data_size)){
			std::cerr << "Error: Could not get the additional_data_size" << std::endl;
			return false;
		}

		if(additional_data_size > 1024 && image->bImageVersion <= FIRMWARE_IMAGE_V2){
			std::cerr << "Error: Firmware Image is corrupted" << std::endl;
			return false;
		}

		if(image->bImageVersion <= FIRMWARE_IMAGE_V2){
			if(!reader_uint16(reader, &image->usImageStructureVersion)){
				std::cerr << "Error: Could not read usImageStructureVersion" << std::endl;
				return false;
			}
		}

		if(image->usImageStructureVersion >= FIRMWARE_IMAGE_STRUCT_V1){
			if(!reader_uint32(reader, &image->bfTargetState)){
				std::cerr << "Error: Could not read bfTargetState" << std::endl;
				return false;
			}

			if(image->usImageStructureVersion >= FIRMWARE_IMAGE_STRUCT_V3){
				if(!reader_uint32(reader, &image->bfCapabilities)){
					std::cerr << "Error: Could not read bfCapabilities" << std::endl;
					return false;
				}

				if(image->bImageVersion <= FIRMWARE_IMAGE_V2){
					if(!reader_uint16(reader, &image->usIntSourceVersionCount)){
						std::cerr << "Error: Could not read bfCapabilities" << std::endl;
						return false;
					}

					if(image->usIntSourceVersionCount > MAX_SOURCE_VERSIONS_COUNT){
						std::cerr << "Error: Firmware Image is corrupted" << std::endl;
						return false;
					}

					if(!reader_array_uint32(reader, MAX_SOURCE_VERSIONS_COUNT, image->rgunIntSourceVersions)){
						std::cerr << "Error: Could not read rgunIntSourceVersion" << std::endl;
						return false;
					}

				}
			}

			image->usSalesCodeSize = 0;
			if(image->usImageStructureVersion >= FIRMWARE_IMAGE_STRUCT_V5){
				if(!reader_uint16(reader, &image->usSalesCodeSize)){
					std::cerr << "Error: Could not load usSalesCodeSize" << std::endl;
					return false;
				}

				if(image->usSalesCodeSize > sizeof(image->rgbSalesCode)){
					std::cerr << "Error: Firmware Image is corrupted" << std::endl;
					return false;
				}

				if(image->usSalesCodeSize != 0){
					if(!reader_array_uint8(reader, image->usSalesCodeSize, image->rgbSalesCode)){
						return false;
					}
				}
			}

			int remaining = static_cast<int>(reader_remaining_size(reader) - sizeof(image->usSignatureKeyId) - sizeof(image->rgbSignature) - sizeof(image->unChecksum));
			if(remaining < 0){
				std::cerr << "Error: Buffer is to small" << std::endl;
				return false;
			}


			if(remaining > 0){
				if(!reader_skip(reader, remaining)){
					std::cerr << "Error: Could not skip the pointer" << std::endl;
					return false;
				}
			}

			if(image->usImageStructureVersion >= FIRMWARE_IMAGE_STRUCT_V2){
				if(!reader_uint16(reader, &image->usSignatureKeyId)){
					std::cerr << "Error: Could not load usSignatureKeyId" << std::endl;
					return false;
				}

				if(!reader_array_uint8(reader, sizeof(image->rgbSignature), image->rgbSignature)){
					std::cerr << "Error: Could not load rgbSignature" << std::endl;
					return false;
				}
			}
		}

		if(!reader_uint32(reader, &image->unChecksum)){
			std::cerr << "Error: Could not load unChecksum" << std::endl;
			return false;
		}

		if(reader_remaining_size(reader) != 0){
			std::cerr << "Error: Firmware Image is corrupted" << std::endl;
			return false;
		}
	}

	if(verbose_print){
		std::cout << "Firmware image parsed successfully" << std::endl;
	}

	return true;
}
