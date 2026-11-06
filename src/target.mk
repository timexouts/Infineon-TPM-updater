TARGET = tpm2_update_firmware
SRC_CC = main.cc
SRC_CC += tpm2_file_parser.cc
SRC_CC += tpm2_get_info.cc
SRC_CC += tpm2_updater.cc

LIBS   = base posix stdcxx tpm2-tss libc libcrypto

CC_CXX_WARN_STRICT_CONVERSION =

