/** @file
  UniXNU UEFI application entry point.

  Copyright (c) 2026, UniXNU contributors. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Library/BaseMemoryLib.h>
#include <Library/CrabAppleLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/LoadedImage.h>

//
// Optional data of the boot option CrabAppleDxe registers. Keep in sync with
// CrabAppleDxe.c.
//
STATIC CONST CHAR16  mFirmwareMarker[] = L"crabapple:firmware";

EFI_STATUS
EFIAPI
CrabAppleApplicationEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_LOADED_IMAGE_PROTOCOL  *Loaded;
  CRABAPPLE_HOST             Host;
  EFI_STATUS                 Status;

  Host = CrabAppleHostApplication;
  if (!EFI_ERROR (gBS->HandleProtocol (ImageHandle, &gEfiLoadedImageProtocolGuid, (VOID **)&Loaded)) &&
      (Loaded->LoadOptions != NULL) && (Loaded->LoadOptionsSize >= sizeof (mFirmwareMarker)) &&
      (CompareMem (Loaded->LoadOptions, mFirmwareMarker, sizeof (mFirmwareMarker)) == 0))
  {
    Host = CrabAppleHostFirmware;
  }

  Status = CrabAppleRun (ImageHandle, Host);

  //
  // "Return to firmware" from a firmware boot option: report failure so BDS
  // moves on to its next option instead of restarting UniXNU.
  //
  if ((Host == CrabAppleHostFirmware) && !EFI_ERROR (Status)) {
    return EFI_ABORTED;
  }

  return Status;
}
