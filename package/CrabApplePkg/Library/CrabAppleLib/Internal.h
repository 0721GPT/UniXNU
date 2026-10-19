/** @file
  CrabAppleLib internal definitions.

  Copyright (c) 2026, UniXNU contributors. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef CRABAPPLE_INTERNAL_H_
#define CRABAPPLE_INTERNAL_H_

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/CrabAppleLib.h>
#include <Library/DebugLib.h>
#include <Library/DevicePathLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/CrabAppleXnuLoader.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/SimpleTextIn.h>
#include <Protocol/SimpleTextOut.h>

#define CA_VERSION_STRING  "0.1.0"

//
// Schema version understood by this build (`version = N` in the config,
// `schema = N` in a kit manifest).
//
#define CA_SCHEMA_VERSION  1

// ---------------------------------------------------------------------------
// Logging. Every line goes to DEBUG() and, when the platform exposes one, to
// the first EFI_SERIAL_IO_PROTOCOL instance. Lines are prefixed "CRABAPPLE: "
// so test harnesses can grep for them.
// ---------------------------------------------------------------------------

VOID
CaLogInit (
  VOID
  );

VOID
EFIAPI
CaLog (
  IN CONST CHAR8  *Format,
  ...
  );

// ---------------------------------------------------------------------------
// TOML subset (Toml.c)
// ---------------------------------------------------------------------------

typedef enum {
  TomlString,
  TomlInteger,
  TomlBoolean,
  TomlArray,
  TomlTable
} TOML_TYPE;

typedef struct TOML_NODE TOML_NODE;

struct TOML_NODE {
  TOML_TYPE    Type;
  CHAR8        *Key;        ///< Member name inside a table; NULL for array items.
  UINTN        Line;        ///< Source line, for diagnostics.
  CHAR8        *String;
  INT64        Integer;
  BOOLEAN      Boolean;
  BOOLEAN      Defined;     ///< Table was opened by an explicit [header].
  BOOLEAN      TableArray;  ///< Array created by [[header]].
  TOML_NODE    *Child;      ///< First member / item.
  TOML_NODE    *LastChild;
  TOML_NODE    *Next;
};

/**
  Parse TOML text. On failure *Root is NULL and ErrorBuffer holds a message.
**/
EFI_STATUS
TomlParse (
  IN  CONST CHAR8  *Text,
  IN  UINTN        Length,
  OUT TOML_NODE    **Root,
  OUT CHAR8        *ErrorBuffer,
  IN  UINTN        ErrorBufferSize
  );

VOID
TomlFree (
  IN TOML_NODE  *Node
  );

TOML_NODE *
TomlGet (
  IN TOML_NODE    *Table,
  IN CONST CHAR8  *Key
  );

CONST CHAR8 *
TomlGetString (
  IN TOML_NODE    *Table,
  IN CONST CHAR8  *Key,
  IN CONST CHAR8  *Default
  );

INT64
TomlGetInteger (
  IN TOML_NODE    *Table,
  IN CONST CHAR8  *Key,
  IN INT64        Default
  );

BOOLEAN
TomlGetBoolean (
  IN TOML_NODE    *Table,
  IN CONST CHAR8  *Key,
  IN BOOLEAN      Default
  );

UINTN
TomlCount (
  IN TOML_NODE  *Array
  );

// ---------------------------------------------------------------------------
// Configuration model (Config.c)
// ---------------------------------------------------------------------------

typedef enum {
  CaDtSet,
  CaDtDeleteProp,
  CaDtAddNode,
  CaDtDeleteNode,
  CaDtOverlay
} CA_DT_OP;

typedef struct {
  CA_DT_OP    Op;
  CHAR8       *Node;       ///< Absolute node path.
  CHAR8       *Prop;       ///< Property name (set / delete-prop).
  VOID        *Value;      ///< Encoded property bytes (set), overlay blob (overlay).
  UINTN       ValueSize;
  CHAR8       *Origin;     ///< "config", "entry <id>" or "kit <name>", for logs.
} CA_DT_PATCH;

typedef struct {
  EFI_HANDLE    Volume;    ///< Volume holding File.
  CHAR16        *File;
  CHAR16        *Args;
  BOOLEAN       Connect;
  CHAR8         *Origin;
} CA_DRIVER;

typedef struct {
  EFI_HANDLE    Volume;
  CHAR16        *File;
  CHAR8         *Origin;
} CA_ACPI_TABLE;

typedef struct {
  EFI_HANDLE  Volume;
  CHAR8       *EntryId;        ///< Required for kit kexts; NULL for entry kexts.
  CHAR16      *BundlePath;
  CHAR16      *PlistPath;
  CHAR16      *ExecutablePath;
  BOOLEAN     Invalid;
} CA_KEXT;

typedef struct {
  EFI_GUID    Guid;
  CHAR16      *Name;
  VOID        *Data;
  UINTN       DataSize;
  UINT32      Attributes;
} CA_VARIABLE;

typedef struct {
  CHAR16     *Partition;   ///< GPT partition name.
  CHAR16     *Name;        ///< Label, also stored in the device path.
  UINT64     Offset;       ///< Bytes from the start of the partition.
  UINT64     Length;       ///< Bytes; 0 = to the end of the partition.
  BOOLEAN    Writable;
} CA_BLOCK_SLICE;

typedef enum {
  CaEntryEfi,
  CaEntryLinux,
  CaEntryAction,
  CaEntryXnu
} CA_ENTRY_TYPE;

typedef enum {
  CaActionNone,
  CaActionReset,
  CaActionShutdown,
  CaActionExit
} CA_ACTION;

typedef struct {
  CHAR8            *Id;
  CHAR16           *Title;
  CA_ENTRY_TYPE    Type;
  CA_ACTION        Action;
  EFI_HANDLE       Volume;      ///< Volume for Path/Dtb/Initrd.
  CHAR16           *Path;       ///< "\\EFI\\x.efi" or "fv:<GUID>".
  CHAR16           *Args;
  CHAR16           *Dtb;        ///< NULL, L"firmware" or a file path.
  CHAR16           *Initrd;
  CA_DT_PATCH      *Patches;    ///< Entry patches, chosen properties last.
  UINTN            PatchCount;
  CA_VARIABLE      *Variables;
  UINTN            VariableCount;
  CA_KEXT          *Kexts;
  UINTN            KextCount;
  BOOLEAN          KextConfigError;
} CA_ENTRY;

typedef enum {
  CaConsoleAuto,
  CaConsoleGraphics,
  CaConsoleText,
  CaConsoleNone
} CA_CONSOLE;

typedef struct {
  //
  // [boot]
  //
  INTN           Timeout;          ///< Seconds; <0 waits for input; 0 boots at once.
  UINTN          DefaultIndex;
  UINTN          RotateSeconds;    ///< Idle highlight rotation step, 0 = off.
  UINTN          FallbackTimeout;  ///< Used when Timeout < 0 and no input exists.
  BOOLEAN        ConnectAll;       ///< ConnectController all handles after drivers.
  //
  // [ui]
  //
  CA_CONSOLE     Console;
  CHAR16         *Title;
  UINTN          Scale;            ///< 0 = automatic.
  BOOLEAN        ShowLogo;
  //
  // Injection, after kits are merged in.
  //
  CA_DRIVER      *Drivers;
  UINTN          DriverCount;
  CA_DT_PATCH    *Patches;         ///< Global patches (config + kits).
  UINTN          PatchCount;
  CA_ACPI_TABLE  *AcpiTables;
  UINTN          AcpiCount;
  CA_BLOCK_SLICE *Slices;
  UINTN          SliceCount;
  CA_KEXT        *Kexts;           ///< Kit kexts, selected by entry id.
  UINTN          KextCount;
  BOOLEAN        KextConfigError;
  CA_ENTRY       *Entries;
  UINTN          EntryCount;
  //
  // Provenance and diagnostics shown on screen.
  //
  EFI_HANDLE     Volume;           ///< Volume the config came from (NULL = built-in).
  CHAR16         *SourceLabel;
  CHAR8          Error[160];       ///< First config/kit error, empty if none.
  CHAR8          Notice[96];       ///< Note for the menu that is not an error, empty if none.
  UINTN          KitsLoaded;
  UINTN          KitsSkipped;
} CA_CONFIG;

/**
  Find a config file on the boot volume or any other volume and parse it;
  fall back to the built-in configuration. Never fails.
**/
VOID
CaConfigLoad (
  IN  EFI_HANDLE  ImageHandle,
  OUT CA_CONFIG   *Config
  );

// ---------------------------------------------------------------------------
// Volumes and files (Volume.c)
// ---------------------------------------------------------------------------

EFI_STATUS
CaReadFile (
  IN  EFI_HANDLE    Volume,
  IN  CONST CHAR16  *Path,
  OUT VOID          **Buffer,
  OUT UINTN         *Size
  );

BOOLEAN
CaFileExists (
  IN EFI_HANDLE    Volume,
  IN CONST CHAR16  *Path
  );

/**
  List the names of the entries of a directory. Caller frees each name and
  the array.
**/
EFI_STATUS
CaListDirectory (
  IN  EFI_HANDLE    Volume,
  IN  CONST CHAR16  *Path,
  OUT CHAR16        ***Names,
  OUT UINTN         *Count
  );

/**
  Turn "a/b", "\\a\\b" or "/a/b" into a newly allocated "\\a\\b". When Base is
  given and Path is relative, the result is Base\\Path.
**/
CHAR16 *
CaNormalizePath (
  IN CONST CHAR16  *Base OPTIONAL,
  IN CONST CHAR8   *Path
  );

/**
  ASCII case-insensitive CHAR16 comparison (FAT names, fallback paths).
**/
INTN
CaStriCmp (
  IN CONST CHAR16  *A,
  IN CONST CHAR16  *B
  );

CHAR16 *
CaAsciiToUnicode (
  IN CONST CHAR8  *String
  );

/**
  Build the device path for "\\file" on Volume or "fv:<GUID>" in any FV.
**/
EFI_DEVICE_PATH_PROTOCOL *
CaResolveImagePath (
  IN EFI_HANDLE    Volume,
  IN CONST CHAR16  *Path
  );

// ---------------------------------------------------------------------------
// Injection (Inject.c)
// ---------------------------------------------------------------------------

typedef struct {
  UINTN    DriversLoaded;
  UINTN    DriversFailed;
  UINTN    AcpiInstalled;
  UINTN    AcpiFailed;
} CA_INJECT_STATS;

/**
  ConnectController() every handle, recursively.
**/
VOID
CaConnectAll (
  VOID
  );

VOID
CaInjectDrivers (
  IN  EFI_HANDLE       ImageHandle,
  IN  CA_CONFIG        *Config,
  OUT CA_INJECT_STATS  *Stats
  );

VOID
CaInjectAcpi (
  IN  CA_CONFIG        *Config,
  OUT CA_INJECT_STATS  *Stats
  );

EFI_STATUS
CaStageKexts (
  IN  CA_CONFIG           *Config,
  IN  CA_ENTRY            *Entry,
  OUT CRABAPPLE_XNU_KEXT  **Kexts,
  OUT UINTN               *Count
  );

VOID
CaFreeKexts (
  IN CRABAPPLE_XNU_KEXT  *Kexts OPTIONAL,
  IN UINTN               Count
  );

/**
  Apply DT patches to a writable FDT that has room to grow.
**/
EFI_STATUS
CaApplyDtPatches (
  IN OUT VOID         *Fdt,
  IN     CA_DT_PATCH  *Patches,
  IN     UINTN        Count
  );

/**
  Build the entry's DTB (base + global patches + entry patches) and install it
  as the EFI_DTB_TABLE configuration table. No-op when the entry has no dtb.
**/
EFI_STATUS
CaPrepareDeviceTree (
  IN  CA_CONFIG  *Config,
  IN  CA_ENTRY   *Entry,
  OUT VOID       **Fdt OPTIONAL,
  OUT UINTN      *FdtSize OPTIONAL
  );

EFI_STATUS
CaInstallInitrd (
  IN  VOID        *Data,
  IN  UINTN       Size,
  OUT EFI_HANDLE  *Handle
  );

VOID
CaUninstallInitrd (
  IN EFI_HANDLE  Handle
  );

EFI_STATUS
CaSetEntryVariables (
  IN CA_ENTRY  *Entry
  );

/**
  Return the root "compatible" list of the firmware's DTB table, or NULL.
**/
CONST CHAR8 *
CaFirmwareCompatible (
  OUT UINTN  *Length
  );

// ---------------------------------------------------------------------------
// Block slices (Slice.c)
// ---------------------------------------------------------------------------

/**
  Publish every configured [[block_slice]] as a block device.
**/
VOID
CaPublishSlices (
  IN CA_CONFIG  *Config
  );

// ---------------------------------------------------------------------------
// Boot (Boot.c)
// ---------------------------------------------------------------------------

EFI_STATUS
CaBootEntry (
  IN EFI_HANDLE  ImageHandle,
  IN CA_CONFIG   *Config,
  IN CA_ENTRY    *Entry
  );

// ---------------------------------------------------------------------------
// User interface (Ui.c, Font.c, Logo.c)
// ---------------------------------------------------------------------------

#define CA_GLYPH_WIDTH   8
#define CA_GLYPH_HEIGHT  19

extern CONST UINT8  gCaFontGlyphs[95][CA_GLYPH_HEIGHT];

typedef enum {
  CaUiGraphics,
  CaUiText,
  CaUiHeadless
} CA_UI_MODE;

typedef struct {
  CA_UI_MODE                      Mode;
  EFI_GRAPHICS_OUTPUT_PROTOCOL    *Gop;
  UINT32                          Width;
  UINT32                          Height;
  UINTN                           Scale;
  UINTN                           MenuTop;
  UINTN                           RowHeight;
  UINTN                           MenuLeft;
  UINTN                           MenuWidth;
  UINTN                           FooterTop;
  UINTN                           VisibleRows;
  UINTN                           TextColumns;      ///< Text mode only.
  UINTN                           TextRows;
  CHAR16                          Info[96];         ///< Injection summary line.
  CHAR16                          Notice[96];       ///< Result of the last boot attempt.
} CA_UI;

VOID
CaUiInit (
  IN  CA_CONFIG  *Config,
  OUT CA_UI      *Ui
  );

VOID
CaUiDrawFrame (
  IN CA_UI      *Ui,
  IN CA_CONFIG  *Config
  );

VOID
CaUiDrawEntry (
  IN CA_UI      *Ui,
  IN CA_CONFIG  *Config,
  IN UINTN      Index,
  IN BOOLEAN    Selected
  );

VOID
CaUiDrawStatus (
  IN CA_UI         *Ui,
  IN UINTN         Line,
  IN CONST CHAR16  *Text
  );

VOID
CaLogoRender (
  OUT EFI_GRAPHICS_OUTPUT_BLT_PIXEL  *Buffer,
  IN  UINTN                          Size,
  IN  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Background
  );

// ---------------------------------------------------------------------------
// Menu (Menu.c)
// ---------------------------------------------------------------------------

/**
  Show the menu and return the chosen entry index.

  @param[in] Countdown  FALSE disables the timeout (after a failed boot).
**/
UINTN
CaMenuRun (
  IN CA_UI      *Ui,
  IN CA_CONFIG  *Config,
  IN BOOLEAN    Countdown
  );

#endif
