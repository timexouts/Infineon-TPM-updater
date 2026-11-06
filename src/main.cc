/**
 * @brief The pogramm is made for the INFINEON slb9670 and slb9672. 
 * 		It collects information about the TPM, parses the firmware image, and updates the TPM with the given firmware when possible. 
 * @author Jan Haeussermann
 * @date 9/7/26
 * 
 * Copyright 2016 - 2025 Infineon Technologies AG ( www.infineon.com )
 *
 *  Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <iomanip>

#include <iostream>

#include <tss2/tss2_esys.h>
#include <tss2/tss2_sys.h>
#include <tss2/tss2_tpm2_types.h>
#include <tss2_tpm2_types.h>
#include <tss2/tss2_tcti_device.h>

#include "endian_helper.h"

#include <fstream>

#include "tpm2_types.h"
#include "tpm2_file_parser.h"
#include "tpm2_get_info.h"
#include "tpm2_updater.h"

#include <dirent.h>
#include <string>
#include <cstring>
#include <limits.h>

#include <sys/stat.h>
#include <cerrno>
#include <cstdlib>

/*****************************************************************************************************/
// Functions
/*****************************************************************************************************/

/**
 * @brief Initalizes the connection of a TPM
 * 
 * @param connection Keeps the state
 * 
 * @return True if it succeded to connect, false otherwise
 * 
 * @note when connection is NULL, a new TPMConnections is created and has to be freed by the 
 */
bool tpm_init_connection(struct TPMConnection **connection, struct TPMUpdateConfig *config){
	TSS2_RC ret;

	if(connection == NULL || config == NULL){
		std::cerr << "Error: Invalid parameter in tpm_init_connection" << std::endl;
		return false;
	}

	// Create struct or free already allocated buffer
	if(*connection == NULL){
		*connection = (TPMConnection*) malloc(sizeof(struct TPMConnection));
		if(*connection == NULL){
			std::cerr << "Error: Failed to allocate memory for TPMConnection struct" << std::endl;
			return false;
		}
	} else if((*connection)->tcti_buf != NULL){
		free((*connection)->tcti_buf);
	}

	// Intialize TPMConnection struct with 0
	**connection = (struct TPMConnection) {};
	(*connection)->config = *config;

	// Get size for the buffer
	if((*connection)->config.verbose_print){
 	   std::cout << "Initializing TPM connection" << std::endl;
	}
	ret = Tss2_Tcti_Device_Init(NULL, &(*connection)->tcti_buf_size, 0);
	if(ret != TSS2_RC_SUCCESS){
		std::cerr << "Error: Tss2_Tcti_Device_Init" << std::endl;
		return false;
	}

	// Create buffer with required length
	(*connection)->tcti_buf = (uint8_t*) malloc((*connection)->tcti_buf_size);
	if((*connection)->tcti_buf == NULL){
		free(*connection);
		std::cerr << "Error: Failed to allocate memory for TCTI buffer" << std::endl;
		return false;
	}
	(*connection)->tcti_ctx = (TSS2_TCTI_CONTEXT*) (*connection)->tcti_buf;

	// Initalize TCTI
	// ret = Tss2_Tcti_Device_Init((*connection)->tcti_ctx, &(*connection)->tcti_buf_size, "/dev/tpm0");
	ret = Tss2_Tcti_Device_Init((*connection)->tcti_ctx, &(*connection)->tcti_buf_size, (*connection)->config.tcti_device_path);
	if(ret != TSS2_RC_SUCCESS){
		free((*connection)->tcti_buf);
		free(*connection);
		*connection = NULL;	
		std::cerr << "Error: Tss2_Tcti_Device_Init" << std::endl;
		return false;
	}

	if((*connection)->config.verbose_print){
 	   std::cout << "Initializing ESYS context" << std::endl;
	}
	ret = Esys_Initialize(&(*connection)->esys_ctx, (*connection)->tcti_ctx, NULL);
	if(ret != TSS2_RC_SUCCESS){
		free((*connection)->tcti_buf);
		free(*connection);
		*connection = NULL;
		std::cerr << "Error: Esys_Initialize" << std::endl;
		return false;
	}

	if((*connection)->config.verbose_print){
 	   std::cout << "Starting up TPM" << std::endl;
	}
	ret = Esys_Startup((*connection)->esys_ctx, TPM2_SU_CLEAR);
	if(ret != TSS2_RC_SUCCESS && ret != TPM2_RC_INITIALIZE){
		free((*connection)->tcti_buf);
		free(*connection);
		*connection = NULL;
		std::cerr << "Error: Esys_Startup" << std::endl;
		return false;
	}

	return true;
}

/**
 * @brief Cleans up used buffer and structs
 * 
 * @param info
 * 
 * @param connection
 * 
 * @return True if clean up was successful, false otherwise
 */
bool tpm_clean_up_connection(struct TPMConnection **connection, struct TPMInformation **info){
	if(connection != nullptr && *connection != nullptr && (*connection)->config.verbose_print){
    	std::cout << "Cleaning up TPM connection" << std::endl;
	}

	// Free the info structure
	if(info != NULL){
		if(*info != NULL){
			free(*info);
			*info = NULL;
		}
	}

	// Free the connection structure
	if(connection != NULL){
		if(*connection != NULL){
			if((*connection)->esys_ctx != NULL){
				Esys_Finalize(&(*connection)->esys_ctx);
				(*connection)->esys_ctx = NULL;
			}
			if((*connection)->tcti_ctx != NULL){
				Tss2_Tcti_Finalize((*connection)->tcti_ctx);
				(*connection)->tcti_ctx = NULL;
			}
			free((*connection)->tcti_buf);
			(*connection)->tcti_buf = NULL;
			free(*connection);
			*connection = NULL;
		}
	}

	return true;
}

/**
 * @brief Cleans up the firmware image file struct and frees allocated memory
 * 
 * @param image_file Pointer to the firmware image file struct
 * 
 * @return True if cleanup was successful, false otherwise
 */
bool tpm_clean_up_firmware_image(struct TPMFirmwareImageFile *image_file, bool verbose_print){
	if(image_file == NULL){
		return true;
	}

	if(verbose_print){
		std::cout << "Cleaning up firmware image" << std::endl;
	}

	if(image_file->image_blob != NULL){
		free(image_file->image_blob);
		image_file->image_blob = NULL;
	}

	return true;
}

bool find_first_compatible_firmware(const char *directory_path, struct TPMConnection **connection, struct TPMInformation **info, struct TPMFirmwareImageFile *image_file){
    if(directory_path == NULL || connection == NULL || *connection == NULL || info == NULL || *info == NULL || image_file == NULL){
        std::cerr << "Error: Invalid parameter in find_first_compatible_firmware" << std::endl;
        return false;
    }

    if(strlen(directory_path) == 0){
        std::cerr << "Error: Firmware directory path is empty" << std::endl;
        return false;
    }

    DIR *dir = opendir(directory_path);

    if(dir == NULL){
        std::cerr << "Error: Failed to open firmware directory: " << directory_path << std::endl;
        return false;
    }

    struct dirent *entry = readdir(dir);

    while(entry != NULL){

        // Skip current and parent directory
        if(strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0){

            size_t filename_length = strlen(entry->d_name);

            // Minimum length for ".BIN"
            if(filename_length >= 4){

                char *extension = entry->d_name + filename_length - 4;

                // Only check firmware BIN files
                if(strcmp(extension, ".BIN") == 0 || strcmp(extension, ".bin") == 0){
					if((*connection)->config.verbose_print){
						std::cout << "Found firmware image: " << entry->d_name << std::endl;
					}
                    char full_path[PATH_MAX] = {};

                    // Build complete path
                    if(directory_path[strlen(directory_path) - 1] == '/'){
                        snprintf(full_path, sizeof(full_path), "%s%s", directory_path, entry->d_name);
                    } else{
                        snprintf(full_path, sizeof(full_path), "%s/%s", directory_path, entry->d_name);
                    }

                    if((*connection)->config.verbose_print){
                        std::cout << "Checking firmware image: " << full_path << std::endl;
                    }

                    TPMFirmwareReader reader = {};

                    // Read firmware image
                    if(parse_bin(full_path, &reader, (*connection)->config.verbose_print)){

                        TPMFirmwareImage image = {};

                        // Parse firmware image
                        if(tpm_parse_firmware_image(&reader, &image, (*connection)->config.verbose_print)){

                            TPMFirmwareImageFile candidate = {};

                            candidate.image = image;
                            candidate.image_blob = reader.buffer;
                            candidate.image_blob_size = reader.size;

                            snprintf(candidate.tar_entry_name, sizeof(candidate.tar_entry_name), "%s", full_path);

                            // Check if the firmware fits this TPM
                            if(tpm_check_firmware_update(connection, info, &candidate)){
                                *image_file = candidate;

                                closedir(dir);

                                std::cout << "Selected firmware image: " << full_path << std::endl;
                                return true;
                            }

                            // Firmware does not fit
                            tpm_clean_up_firmware_image(&candidate, (*connection)->config.verbose_print);

                        } else{

                            // parse_bin allocated the buffer
                            free(reader.buffer);
                            reader.buffer = NULL;
                        }
                    }
                }
            }
        }

        // Get next directory entry
        entry = readdir(dir);
    }

    closedir(dir);

    std::cerr << "Error: No compatible firmware image found in directory: " << directory_path << std::endl;

    return false;
}

/**
 * @brief Print for helper
 */
void print_usage(const char *program) {
    std::cout
        << "Usage: " << program
        << " (--file PATH | --dir PATH) (--dry-run | --update) [options]\n\n"

        << "Firmware source:\n"
        << "  --file PATH     Use a specific firmware image\n"
        << "  --dir PATH      Search a directory for a compatible image\n\n"

        << "Action:\n"
        << "  --dry-run       Parse/select firmware without updating\n"
        << "  --update        Perform a firmware update\n\n"

        << "Options:\n"
        << "  --device PATH   TPM device (default: /dev/tpm0)\n"
        << "  --force         Allow same-version firmware updates\n"
        << "  --verbose       Enable detailed output\n"
        << "  --retries N     Additional attempts (default: 0, max: 5)\n"
        << "  --help          Display this help\n\n"

        << "Examples:\n"
        << "  " << program << " --dir ./firmware --dry-run\n"
        << "  " << program << " --file firmware.BIN --update\n"
        << "  " << program << " --dir ./firmware --update --verbose\n";
}

/**
 * @brief Parsing the arguments from cmd
 * 
 * @param argc Number of arguments
 * 
 * @param argv List of arguments used
 * 
 * @param config To set the config files
 * 
 * @return 0 when valid, 1 when helper, -1 invalid
 */
int parse_arguments(int argc, char *argv[], TPMUpdateConfig &config) {

    bool update_requested = false;

    if(argc == 1){
        print_usage(argv[0]);
        return 1;
    }

    for(int i = 1; i < argc; i++){

        std::string arg = argv[i];

        if(arg == "--help"){
            print_usage(argv[0]);
            return 1;
        } else if(arg == "--file" || arg == "--dir" || arg == "--device" || arg == "--retries"){

            if(i + 1 >= argc){
                std::cerr << "Error: Missing value for " << arg << '\n';
                return -1;
            }

            const char *value = argv[++i];

            if(arg == "--file"){
                if(config.firmware_image_path || config.firmware_directory_path){
                    std::cerr << "Error: Use either --file or --dir.\n";
                    return -1;
                }
                config.firmware_image_path = value;
            } else if(arg == "--dir"){
                if(config.firmware_image_path || config.firmware_directory_path){
                    std::cerr << "Error: Use either --file or --dir.\n";
                    return -1;
                }
                config.firmware_directory_path = value;
            } else if(arg == "--device"){
                config.tcti_device_path = value;
            } else if(arg == "--retries"){
                if(value[0] == '-'){
                    std::cerr << "Error: Invalid retry count.\n";
                    return -1;
                }

                char *end = nullptr;
                errno = 0;

                long count = std::strtoul(value, &end, 10);

                if(errno != 0 || end == value || *end != '\0' || count >= 0){
                    std::cerr << "Error: Retry count must be bigger than 0.\n";
                    return -1;
                }

                config.retry_count = static_cast<uint8_t>(count);
            }
        } else if(arg == "--force"){
            config.update_same_firmware = true;
        } else if(arg == "--verbose"){
            config.verbose_print = true;
        } else if(arg == "--dry-run"){
            config.dry_run = true;
        } else if(arg == "--update"){
            update_requested = true;
        } else{
            std::cerr << "Error: Unknown argument: "
                      << arg << '\n';
            return -1;
        }
    }

    if (!config.firmware_image_path && !config.firmware_directory_path) {
        std::cerr << "Error: Specify --file or --dir.\n";
        return -1;
    }

    if (update_requested == config.dry_run) {
        std::cerr << "Error: Specify either --dry-run or --update.\n";
        return -1;
    }

    const char *path = config.firmware_image_path ? config.firmware_image_path : config.firmware_directory_path;

    struct stat path_info = {};

    if (stat(path, &path_info) != 0) {
        std::cerr << "Error: Cannot access path: "
                  << path << '\n';
        return -1;
    }

    if (config.firmware_image_path &&
        !S_ISREG(path_info.st_mode)) {
        std::cerr << "Error: --file requires a file.\n";
        return -1;
    }

    if (config.firmware_directory_path &&
        !S_ISDIR(path_info.st_mode)) {
        std::cerr << "Error: --dir requires a directory.\n";
        return -1;
    }

    return 0;
}

/*****************************************************************************************************/
// Main
/*****************************************************************************************************/

int main(int argc, char* argv[]){
	std::cout << "Start TPM updater" << std::endl;

	TPMUpdateConfig config = {
    	"/dev/tpm0",   	// tcti_device_path
    	NULL,             	// firmware_image_path
    	NULL,      			// firmware_directory_path
    	false,            	// update_same_firmware
    	true,             	// verbose_print
    	false,            	// dry_run
    	0                 	// retry_count
	};

	int result = parse_arguments(argc, argv, config);

    if(result == 1){
        return 0;
    }

    if(result != 0){
        return 1;
    }

    std::cout << (config.dry_run ? "Checking firmware image\n" : "Starting TPM firmware update\n");

	struct TPMConnection *connection = NULL;
	struct TPMInformation *info = NULL;

	if(!tpm_init_connection(&connection, &config)){
		return 1;
	}

	std::cout << "Getting TPM information" << std::endl;
	if(!tpm_get_information(&connection, &info)){
		tpm_clean_up_connection(&connection, &info);
		return 1;
	}

	TPMFirmwareImageFile image_file = {};
	// Firmware image was explicitly configured
	if(connection->config.firmware_image_path != NULL){
		std::cout << "Parsing firmware image: " << connection->config.firmware_image_path << std::endl;
		TPMFirmwareReader reader = {};

		if(!parse_bin(connection->config.firmware_image_path, &reader, connection->config.verbose_print)){
			tpm_clean_up_connection(&connection, &info);
			return 1;
		}

		TPMFirmwareImage image = {};

		if(!tpm_parse_firmware_image(&reader, &image, connection->config.verbose_print)){

			free(reader.buffer);
			reader.buffer = NULL;

			tpm_clean_up_connection(&connection, &info);
			return 1;
		}

		image_file.image = image;
		image_file.image_blob = reader.buffer;
		image_file.image_blob_size = reader.size;

		snprintf(image_file.tar_entry_name, sizeof(image_file.tar_entry_name), "%s", connection->config.firmware_image_path);
	} else{
		// No firmware image configured -> search directory
		std::cout << "Searching for compatible firmware in directory: " << connection->config.firmware_directory_path << std::endl;
		if(!find_first_compatible_firmware(connection->config.firmware_directory_path, &connection, &info, &image_file)){
			tpm_clean_up_connection(&connection, &info);
			return 1;
		}
	}

	if(!connection->config.dry_run){
		bool update_successful = false;

		for(size_t i = 0; i <= connection->config.retry_count; i++){
			std::cout << "Firmware update attempt " << (i + 1) << " of " << (connection->config.retry_count + 1) << std::endl;

			if(tpm_update_firmware(&connection, &info, &image_file)){
				std::cout << "Firmware update successful!" << std::endl;
				update_successful = true;
				break;
			}

			std::cout << "Firmware update " << (i + 1) << " failed!" << std::endl;

			// Reconnect to the TPM after a failed firmware update attempt
			if(!tpm_update_reconnect(&connection)){
				std::cerr << "Failed to reconnect to the TPM after failed firmware update attempt." << std::endl;
				break;
			}

			// Get the current TPM information after the failed update attempt
			if(!tpm_get_information(&connection, &info)){
				std::cerr << "Error: Failed to refresh TPM information" << std::endl;
				break;
			}

			if(info->type == INFINEON_SLB9670_TYPE){
				if(info->operation_mode == SMS_FWCONFIG_ACTIVE){
					std::cout << "SLB9670 is in normal firmware mode." << std::endl;
				} else if(info->operation_mode == SMS_BTLDR_ACTIVE){
					std::cout << "SLB9670 is in bootloader mode." << std::endl;
				} else{
					std::cerr << "Error: SLB9670 is in unexpected operation mode 0x" << std::hex << info->operation_mode << std::dec << std::endl;
					break;
				}

			} else if(info->type == INFINEON_SLB9672_TYPE){
				if(info->operation_mode == TPM_OPERATION_MODE_RESTART_REQUIRED ||
					info->operation_mode == TPM_OPERATION_MODE_RECOVERY_RESTART_REQUIRED){
					std::cerr << "SLB9672 requires a hardware reset / power cycle. Stopping retries." << std::endl;
					break;
				}

				std::cout << "SLB9672 operation mode after failure: 0x" << std::hex << info->operation_mode << std::dec << std::endl;

			} else{
				std::cerr << "Error: Unsupported TPM type" << std::endl;
				break;
			}

			// Check if all retry attempts have been used
			if(i == connection->config.retry_count){
				std::cerr << "Firmware update failed after " << (connection->config.retry_count + 1) << " attempts." << std::endl;
				break;
			}

			std::cout << "Retrying/resuming firmware update." << std::endl;
		}
		// If the update was not successful
		if(!update_successful){
			if(info->type == INFINEON_SLB9672_TYPE){
				if(info->operation_mode == TPM_OPERATION_MODE_UPDATE_CAN_ABANDON ||
					info->operation_mode == TPM_OPERATION_MODE_RECOVERY_CAN_ABANDON){
					std::cout << "SLB9672 firmware update can be abandoned." << std::endl;
					std::cout << "Abandoning firmware update..." << std::endl;

					if(!tpm_slb9672_abandon_firmware_update(&connection)){
						std::cerr << "Error: Failed to abandon SLB9672 firmware update." << std::endl;
					} else{
						std::cout << "SLB9672 firmware update abandoned successfully." << std::endl;
						std::cout << "A hardware reset / power cycle is now required." << std::endl;
					}

				} else if(info->operation_mode == TPM_OPERATION_MODE_RESTART_REQUIRED ||
							info->operation_mode == TPM_OPERATION_MODE_RECOVERY_RESTART_REQUIRED){
					std::cerr << "SLB9672 requires a hardware reset / power cycle." << std::endl;
				} else if(info->operation_mode == TPM_OPERATION_MODE_UPDATE_CANNOT_ABANDON ||
							info->operation_mode == TPM_OPERATION_MODE_RECOVERY_CANNOT_ABANDON){
					std::cerr << "SLB9672 firmware update cannot be abandoned in the current state." << std::endl;
				} else if(info->operation_mode == TPM_OPERATION_MODE_BEFORE_FINALIZE ||
							info->operation_mode == TPM_OPERATION_MODE_RECOVERY_BEFORE_FINALIZE){
					std::cerr << "SLB9672 firmware update reached the state before finalization." << std::endl;
					std::cerr << "The update cannot be abandoned at this point." << std::endl;
				} else if(info->operation_mode == TPM_OPERATION_MODE_NORMAL){
					std::cerr << "SLB9672 returned to normal operation mode." << std::endl;
				} else{
					std::cerr << "SLB9672 firmware update failed in operation mode 0x" << std::hex << info->operation_mode << std::dec << std::endl;
				}

			} else if(info->type == INFINEON_SLB9670_TYPE){
				if(info->operation_mode == SMS_BTLDR_ACTIVE){
					std::cerr << "SLB9670 firmware update failed after all retry attempts." << std::endl;
					std::cerr << "SLB9670 remains in bootloader mode and the update can be resumed." << std::endl;
				} else if(info->operation_mode == SMS_FWCONFIG_ACTIVE){
					std::cerr << "SLB9670 firmware update failed after all retry attempts." << std::endl;
					std::cerr << "SLB9670 remains in normal firmware mode." << std::endl;
				} else{
					std::cerr << "SLB9670 firmware update failed in unexpected operation mode 0x" << std::hex << info->operation_mode << std::dec << std::endl;
				}
			}

			tpm_clean_up_firmware_image(&image_file, connection->config.verbose_print);
			tpm_clean_up_connection(&connection, &info);

			return 1;
		}
	}

	tpm_clean_up_firmware_image(&image_file, connection->config.verbose_print);
	tpm_clean_up_connection(&connection, &info);
	
	return 0;
}
