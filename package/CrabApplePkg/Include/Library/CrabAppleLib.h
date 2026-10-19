/** @file
  UniXNU boot manager core.

  Both the UEFI application and the DXE driver builds are thin wrappers around
  CrabAppleRun(); everything else lives in CrabAppleLib.

  Copyright (c) 2026, UniXNU contributors. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef CRABAPPLE_LIB_H_
#define CRABAPPLE_LIB_H_

#include <Uefi.h>

typedef enum {
  ///
  /// Started as an ordinary UEFI application (from a disk, the shell, a
  /// firmware boot option...).
  ///
  CrabAppleHostApplication,
  ///
  /// Started by the platform BDS through the boot option the DXE driver
  /// registered for itself.
  ///
  CrabAppleHostFirmware
} CRABAPPLE_HOST;

/**
  Run the boot manager: locate and parse the configuration, inject drivers,
  show the menu and start the selected entry.

  @param[in] ImageHandle  Image handle of the running UniXNU image.
  @param[in] Host         How this image was started.

  @retval EFI_SUCCESS   The user chose to leave UniXNU.
  @retval EFI_ABORTED   Nothing could be booted.
  @retval other         Error from the boot path.
**/
EFI_STATUS
EFIAPI
CrabAppleRun (
  IN EFI_HANDLE      ImageHandle,
  IN CRABAPPLE_HOST  Host
  );

#endif
