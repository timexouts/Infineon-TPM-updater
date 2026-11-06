# Third-party notices

## Infineon TPMFactoryUpd reference code

This project was developed with reference to Infineon Technologies AG's
**TPMFactoryUpd for Linux, v02.03.4733.00**. Certain firmware image formats,
data structures, constants, and vendor-specific firmware update protocol
definitions are based on the Infineon reference implementation.

Relevant upstream source files include:

- `Source/Common/FirmwareImage.h` and `FirmwareImage.c`
- `Source/Common/FirmwareUpdate.h` and `FirmwareUpdate.c`
- `Source/Common/MicroTss/Tpm_2_0/TPM2_FieldUpgradeTypes.h`
- `Source/Common/MicroTss/Tpm_2_0/TPM2_VendorTypes.h`
- `Source/Common/MicroTss/Tpm_2_0/TPM2_FieldUpgrade*Vendor.c` and `.h`

The identified Infineon files use **BSD 3-Clause-style** source-code notices.
The complete license terms and disclaimer from one representative source file
are reproduced in [`licenses/Infineon-BSD-3-Clause.txt`](licenses/Infineon-BSD-3-Clause.txt).
Original copyright years vary by file, including:

- Copyright 2013 - 2025 Infineon Technologies AG
- Copyright 2014 - 2025 Infineon Technologies AG
- Copyright 2019 - 2025 Infineon Technologies AG

**Preserve the exact applicable copyright and license notices in each file
containing copied or adapted Infineon source code.** This notice is not a
replacement for source-file notices. Use of Infineon's name does not imply
endorsement by Infineon Technologies AG.

## Other upstream licenses

The Infineon distribution also contains some MicroTss files with separate
Trusted Computing Group (TCG) license terms. Those files have not been
included wholesale in this project. If any protected TCG-derived source code
was copied or adapted into this project, retain the corresponding source
license notices and review its redistribution terms separately before release.

## External dependencies

This project uses TPM2-TSS and OpenSSL as external libraries. Their source
code is not included here; if you redistribute these dependencies or their
binaries, follow their applicable licenses and notice requirements.

## Project documents

The root `LICENSE` grants rights for the original software contributions only
where the named copyright holder is authorized to do so. It does not
implicitly relicense third-party code, firmware images, the semester report,
or other separately copyrighted materials.
