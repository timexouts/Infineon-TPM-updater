# Infineon TPM Firmware Updater

A C++ firmware updater for **Infineon SLB9670 and SLB9672 TPM 2.0** devices. It retrieves TPM information, parses Infineon firmware images, checks update compatibility, and implements the device-specific firmware update sequences.

## Features

- Support for Infineon SLB9670 and SLB9672 firmware updates
- Retrieval of standard and Infineon-specific TPM properties
- Parsing of Infineon firmware image formats V1–V3
- Firmware integrity and compatibility checks during the update workflow
- TPM-specific update sequences, state handling, and support for resuming certain interrupted updates
- Selection of an individual firmware image or search for a compatible image in a directory
- Dry-run mode that does not initiate a firmware update

## Build

### Platform support

The firmware updater was originally developed and tested in a Genode-based environment. The implementation uses TPM2-TSS, OpenSSL, and standard C/C++ interfaces.

A standalone Linux build is provided using GNU Make. Porting to other operating systems requires the necessary dependencies and a compatible TPM communication interface.

### Requirements

- C++17 compiler
- GNU Make and `pkg-config`
- TPM2-TSS development libraries (ESYS, SYS, and device TCTI)
- OpenSSL development libraries

Install these dependencies using your Linux distribution's package manager, then run from the repository root:

```bash
make
```

The executable is created at `build/tpm-firmware-updater`. **Building the software does not update the TPM.**


## Usage

Show command-line help without connecting to a TPM:

```bash
./build/tpm-firmware-updater --help
```

The tool requires **one firmware source** (`--file` or `--dir`) and **one action** (`--dry-run` or `--update`).

### Dry run

Parse an individual firmware image without performing an update:

```bash
./build/tpm-firmware-updater --file ./firmware/update.BIN --dry-run --device /dev/tpm0
```

Search a directory for the **first compatible** `.BIN` or `.bin` image, without performing an update:

```bash
./build/tpm-firmware-updater --dir ./firmware --dry-run --device /dev/tpm0
```

**Note:** A dry run still initializes a connection to a TPM. With `--file`, the current implementation parses the image but does not perform the device-compatibility check used in the update function. With `--dir`, it checks candidate compatibility to select an image. Dry run does not flash the TPM and is not a full image authenticity verification.

### Firmware update

Update using a specific firmware image:

```bash
./build/tpm-firmware-updater --file ./firmware/update.BIN --update --device /dev/tpm0
```

Or search a firmware directory and update using the first compatible image found:

```bash
./build/tpm-firmware-updater --dir ./firmware --update --device /dev/tpm0
```

**Caution:** Directory enumeration order is not guaranteed. If the directory contains multiple compatible firmware images, the current implementation selects the first one encountered. For predictable updates, use `--file` with an explicitly selected image.

### Options

| Option | Description |
| --- | --- |
| `--file PATH` | Use a specific firmware image |
| `--dir PATH` | Search a directory for a compatible firmware image |
| `--dry-run` | Parse/select firmware without flashing the TPM |
| `--update` | Perform a firmware update |
| `--device PATH` | TPM device path |
| `--force` | Allow a same-version update where supported |
| `--verbose` | Enable verbose output (currently enabled by default in the source configuration) |
| `--retries N` | Number of additional update attempts |
| `--help` | Display usage information |

## Safety and limitations

**Warning:** Firmware updates modify a security-critical device. An unsuccessful or interrupted update may render the TPM unavailable, require recovery, or result in loss of access to TPM-protected data. Use only firmware images authorized for the specific device. Do not power off the system or interrupt a firmware transfer.

- Firmware images are **not included** in this repository. Obtain them from Infineon or an authorized device supplier.
- TPM firmware update operations may require privileges, platform authorization, exclusive device access, and device-specific recovery procedures.
- The updater performs software-side format, integrity, and compatibility checks as appropriate to the selected workflow. These are **not a substitute** for firmware authenticity checks enforced by the TPM.
- Automatic retry handling is limited; some failures require a reboot or power cycle. For certain SLB9672 failure states, the updater may attempt to abandon an incomplete update.
- Successful Linux compilation does not establish that firmware updates have been validated on every Linux system or TPM configuration.

## Documentation

The [semester project report](docs/report.pdf) describes the firmware update protocol, software design, implementation, and evaluation in more detail.

## Reference implementation and licensing

This project was developed with reference to Infineon's **TPMFactoryUpd for Linux v02.03.4733.00**, including its firmware image formats, data structures, and vendor-specific update protocol.

The project's original source-code contributions are provided under the [BSD 3-Clause License](LICENSE), subject to the applicable copyright permissions. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) and [Infineon's BSD license notices](licenses/Infineon-BSD-3-Clause.txt) for third-party attribution and licensing details. The semester project report is not automatically licensed under the software license.

This project is independent and is not endorsed by Infineon Technologies AG.

## Disclaimer

This software is provided **"AS IS"**, without warranty of any kind. Use it at your own risk. See the [LICENSE](LICENSE) for the full warranty disclaimer and limitation of liability.
