/** @file
  Menu rendering on EFI_GRAPHICS_OUTPUT_PROTOCOL, with a text console
  fallback and a headless mode (log only).

  Graphics mode never prints through ConOut, so firmware consoles that mirror
  ConOut onto the same framebuffer cannot scribble over the menu.

  Copyright (c) 2026, UniXNU contributors. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "Internal.h"

#define PIXEL(r, g, b)  { (b), (g), (r), 0 }

STATIC CONST EFI_GRAPHICS_OUTPUT_BLT_PIXEL  mBackground  = PIXEL (0x0B, 0x0C, 0x0F);
STATIC CONST EFI_GRAPHICS_OUTPUT_BLT_PIXEL  mText        = PIXEL (0xF2, 0xF3, 0xF5);
STATIC CONST EFI_GRAPHICS_OUTPUT_BLT_PIXEL  mDim         = PIXEL (0x7C, 0x83, 0x8D);
STATIC CONST EFI_GRAPHICS_OUTPUT_BLT_PIXEL  mAccent      = PIXEL (0x2A, 0x5B, 0xD7);
STATIC CONST EFI_GRAPHICS_OUTPUT_BLT_PIXEL  mRow         = PIXEL (0x17, 0x1A, 0x20);
STATIC CONST EFI_GRAPHICS_OUTPUT_BLT_PIXEL  mError       = PIXEL (0xFF, 0x8A, 0x80);
STATIC CONST EFI_GRAPHICS_OUTPUT_BLT_PIXEL  mRule        = PIXEL (0x26, 0x2A, 0x31);
STATIC CONST EFI_GRAPHICS_OUTPUT_BLT_PIXEL  mMarker      = PIXEL (0xFF, 0xFF, 0xFF);

//
// Rows of the status area (UniXNUUiDrawStatus Line numbers).
//
#define STATUS_LINES  3

STATIC
VOID
FillRect (
  IN UNIXNU_UI                          *Ui,
  IN UINTN                          X,
  IN UINTN                          Y,
  IN UINTN                          W,
  IN UINTN                          H,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Color
  )
{
  if ((X >= Ui->Width) || (Y >= Ui->Height)) {
    return;
  }

  W = MIN (W, Ui->Width - X);
  H = MIN (H, Ui->Height - Y);
  Ui->Gop->Blt (Ui->Gop, &Color, EfiBltVideoFill, 0, 0, X, Y, W, H, 0);
}

//
// Draw Text with its top-left at (X, Y) on a Background box. Returns the
// width in pixels. Characters outside printable ASCII render as '?'.
//
STATIC
UINTN
DrawText (
  IN UNIXNU_UI                          *Ui,
  IN UINTN                          X,
  IN UINTN                          Y,
  IN CONST CHAR16                   *Text,
  IN UINTN                          Scale,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Foreground,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Background,
  IN UINTN                          MaxWidth
  )
{
  UINTN                          Len;
  UINTN                          Cw;
  UINTN                          Ch;
  UINTN                          W;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  *Buffer;
  UINTN                          Index;
  UINTN                          Row;
  UINTN                          Col;
  UINTN                          Sy;
  UINTN                          Sx;
  CHAR16                         C;
  CONST UINT8                    *Glyph;
  UINTN                          Base;

  Len = StrLen (Text);
  Cw  = UNIXNU_GLYPH_WIDTH * Scale;
  Ch  = UNIXNU_GLYPH_HEIGHT * Scale;
  if ((MaxWidth > 0) && (Len * Cw > MaxWidth)) {
    Len = MaxWidth / Cw;
  }

  if ((Len == 0) || (X >= Ui->Width) || (Y + Ch > Ui->Height)) {
    return 0;
  }

  Len = MIN (Len, (Ui->Width - X) / Cw);
  W   = Len * Cw;
  if (W == 0) {
    return 0;
  }

  Buffer = AllocatePool (W * Ch * sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL));
  if (Buffer == NULL) {
    return 0;
  }

  for (Index = 0; Index < W * Ch; Index++) {
    Buffer[Index] = Background;
  }

  for (Index = 0; Index < Len; Index++) {
    C     = Text[Index];
    Glyph = gCaFontGlyphs[((C >= 0x20) && (C <= 0x7E)) ? (C - 0x20) : ('?' - 0x20)];
    for (Row = 0; Row < UNIXNU_GLYPH_HEIGHT; Row++) {
      for (Col = 0; Col < UNIXNU_GLYPH_WIDTH; Col++) {
        if ((Glyph[Row] & (0x80 >> Col)) == 0) {
          continue;
        }

        Base = (Row * Scale) * W + Index * Cw + Col * Scale;
        for (Sy = 0; Sy < Scale; Sy++) {
          for (Sx = 0; Sx < Scale; Sx++) {
            Buffer[Base + Sy * W + Sx] = Foreground;
          }
        }
      }
    }
  }

  Ui->Gop->Blt (Ui->Gop, Buffer, EfiBltBufferToVideo, 0, 0, X, Y, W, Ch, 0);
  FreePool (Buffer);
  return W;
}

STATIC
VOID
DrawCentered (
  IN UNIXNU_UI                          *Ui,
  IN UINTN                          Y,
  IN CONST CHAR16                   *Text,
  IN UINTN                          Scale,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Foreground
  )
{
  UINTN  W;

  W = MIN (StrLen (Text) * UNIXNU_GLYPH_WIDTH * Scale, Ui->Width);
  DrawText (Ui, (Ui->Width - W) / 2, Y, Text, Scale, Foreground, mBackground, Ui->Width);
}

// ---------------------------------------------------------------------------
// Text console
// ---------------------------------------------------------------------------

STATIC
VOID
TextPrintAt (
  IN UNIXNU_UI         *Ui,
  IN UINTN         Row,
  IN UINTN         Attribute,
  IN CONST CHAR16  *Text
  )
{
  EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL  *Out;
  CHAR16                           Line[160];
  UINTN                            Width;
  UINTN                            Len;

  Out = gST->ConOut;
  if ((Out == NULL) || (Row >= Ui->TextRows)) {
    return;
  }

  //
  // Pad to the full width (minus the last column, which would scroll).
  //
  Width = MIN (Ui->TextColumns - 1, ARRAY_SIZE (Line) - 1);
  Len   = MIN (StrLen (Text), Width);
  CopyMem (Line, Text, Len * sizeof (CHAR16));
  while (Len < Width) {
    Line[Len++] = L' ';
  }

  Line[Len] = L'\0';
  Out->SetAttribute (Out, Attribute);
  Out->SetCursorPosition (Out, 0, Row);
  Out->OutputString (Out, Line);
  Out->SetAttribute (Out, EFI_LIGHTGRAY | EFI_BACKGROUND_BLACK);
}

// ---------------------------------------------------------------------------
// Public
// ---------------------------------------------------------------------------

VOID
UniXNUUiInit (
  IN  UNIXNU_CONFIG  *Config,
  OUT UNIXNU_UI      *Ui
  )
{
  EFI_STATUS  Status;

  ZeroMem (Ui, sizeof (*Ui));
  Ui->Mode = UniXNUUiHeadless;

  if ((Config->Console == UniXNUConsoleAuto) || (Config->Console == UniXNUConsoleGraphics)) {
    Status = EFI_NOT_FOUND;
    if (gST->ConsoleOutHandle != NULL) {
      Status = gBS->HandleProtocol (gST->ConsoleOutHandle, &gEfiGraphicsOutputProtocolGuid, (VOID **)&Ui->Gop);
    }

    if (EFI_ERROR (Status)) {
      Status = gBS->LocateProtocol (&gEfiGraphicsOutputProtocolGuid, NULL, (VOID **)&Ui->Gop);
    }

    if (!EFI_ERROR (Status) && (Ui->Gop->Mode != NULL) && (Ui->Gop->Mode->Info != NULL)) {
      Ui->Mode   = UniXNUUiGraphics;
      Ui->Width  = Ui->Gop->Mode->Info->HorizontalResolution;
      Ui->Height = Ui->Gop->Mode->Info->VerticalResolution;
      Ui->Scale  = (Config->Scale > 0) ? Config->Scale : MAX (1, Ui->Height / 540);
    }
  }

  if ((Ui->Mode == UniXNUUiHeadless) && (Config->Console != UniXNUConsoleNone) && (gST->ConOut != NULL)) {
    if (!EFI_ERROR (gST->ConOut->QueryMode (gST->ConOut, gST->ConOut->Mode->Mode, &Ui->TextColumns, &Ui->TextRows))) {
      Ui->Mode = UniXNUUiText;
    }
  }

  if (Ui->Mode == UniXNUUiGraphics) {
    UniXNULog ("ui: graphics %ux%u, scale %u", Ui->Width, Ui->Height, (UINT32)Ui->Scale);
  } else if (Ui->Mode == UniXNUUiText) {
    UniXNULog ("ui: text console %ux%u", (UINT32)Ui->TextColumns, (UINT32)Ui->TextRows);
  } else {
    UniXNULog ("ui: headless (no GOP, no text console)");
  }
}

VOID
UniXNUUiDrawFrame (
  IN UNIXNU_UI      *Ui,
  IN UNIXNU_CONFIG  *Config
  )
{
  UINTN                          Logo;
  UINTN                          Y;
  UINTN                          Cw;
  UINTN                          Ch;
  UINTN                          Index;
  UINTN                          Longest;
  UINTN                          MaxRows;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  *Buffer;
  CHAR16                         Line[128];

  if (Ui->Mode == UniXNUUiText) {
    gST->ConOut->EnableCursor (gST->ConOut, FALSE);
    gST->ConOut->ClearScreen (gST->ConOut);
    UnicodeSPrint (Line, sizeof (Line), L"%s  (UniXNU %a)", Config->Title, UNIXNU_VERSION_STRING);
    TextPrintAt (Ui, 0, EFI_WHITE | EFI_BACKGROUND_BLACK, Line);
    TextPrintAt (Ui, 1, EFI_DARKGRAY | EFI_BACKGROUND_BLACK, Config->SourceLabel);
    Ui->MenuTop     = 3;
    Ui->VisibleRows = MIN (Config->EntryCount, Ui->TextRows > 8 ? Ui->TextRows - 8 : 1);
    Ui->FooterTop   = Ui->TextRows - STATUS_LINES - 2;
    if (Config->Error[0] != '\0') {
      UnicodeSPrint (Line, sizeof (Line), L"config error: %a", Config->Error);
      TextPrintAt (Ui, Ui->FooterTop + STATUS_LINES, EFI_LIGHTRED | EFI_BACKGROUND_BLACK, Line);
    } else if (Config->Notice[0] != '\0') {
      UnicodeSPrint (Line, sizeof (Line), L"%a", Config->Notice);
      TextPrintAt (Ui, Ui->FooterTop + STATUS_LINES, EFI_YELLOW | EFI_BACKGROUND_BLACK, Line);
    }

    if (Ui->Info[0] != L'\0') {
      TextPrintAt (Ui, Ui->FooterTop + STATUS_LINES + 1, EFI_DARKGRAY | EFI_BACKGROUND_BLACK, Ui->Info);
    }

    return;
  }

  if (Ui->Mode != UniXNUUiGraphics) {
    return;
  }

  Cw = UNIXNU_GLYPH_WIDTH * Ui->Scale;
  Ch = UNIXNU_GLYPH_HEIGHT * Ui->Scale;
  FillRect (Ui, 0, 0, Ui->Width, Ui->Height, mBackground);

  //
  // Logo (it carries the UniXNU wordmark), title and provenance.
  //
  Y = Ui->Height / 18;
  if (Config->ShowLogo) {
    Logo   = MAX (96, MIN (Ui->Width, Ui->Height) * 3 / 10);
    Buffer = AllocatePool (Logo * Logo * sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL));
    if (Buffer != NULL) {
      UniXNULogoRender (Buffer, Logo, mBackground);
      Ui->Gop->Blt (Ui->Gop, Buffer, EfiBltBufferToVideo, 0, 0, (Ui->Width - Logo) / 2, Y, Logo, Logo, 0);
      FreePool (Buffer);
    }

    Y += Logo + Ch / 2;
  }

  DrawCentered (Ui, Y, Config->Title, Ui->Scale * 2, mText);
  Y += Ch * 2 + Ch / 2;
  UnicodeSPrint (Line, sizeof (Line), L"UniXNU %a  |  %s", UNIXNU_VERSION_STRING, Config->SourceLabel);
  DrawCentered (Ui, Y, Line, Ui->Scale, mDim);
  Y += Ch + Ch / 2;

  //
  // Menu box: wide enough for the longest title plus the type tag.
  //
  Longest = 24;
  for (Index = 0; Index < Config->EntryCount; Index++) {
    Longest = MAX (Longest, StrLen (Config->Entries[Index].Title) + 12);
  }

  Ui->RowHeight = Ch + Ch / 2;
  Ui->MenuWidth = MIN (Longest * Cw, Ui->Width - 2 * Cw);
  Ui->MenuLeft  = (Ui->Width - Ui->MenuWidth) / 2;
  FillRect (Ui, Ui->MenuLeft, Y, Ui->MenuWidth, MAX (1, Ui->Scale / 2), mRule);
  Y            += Ch;
  Ui->MenuTop   = Y;
  Ui->FooterTop = Ui->Height - (STATUS_LINES + 2) * (Ch + Ch / 3);
  MaxRows       = (Ui->FooterTop > Y + Ui->RowHeight) ? (Ui->FooterTop - Y) / Ui->RowHeight : 1;
  Ui->VisibleRows = MIN (Config->EntryCount, MaxRows);

  if (Config->Error[0] != '\0') {
    UnicodeSPrint (Line, sizeof (Line), L"config error: %a", Config->Error);
    DrawCentered (Ui, Ui->FooterTop + STATUS_LINES * (Ch + Ch / 3), Line, Ui->Scale, mError);
  } else if (Config->Notice[0] != '\0') {
    UnicodeSPrint (Line, sizeof (Line), L"%a", Config->Notice);
    DrawCentered (Ui, Ui->FooterTop + STATUS_LINES * (Ch + Ch / 3), Line, Ui->Scale, mError);
  }

  if (Ui->Info[0] != L'\0') {
    DrawCentered (Ui, Ui->FooterTop + (STATUS_LINES + 1) * (Ch + Ch / 3), Ui->Info, Ui->Scale, mDim);
  }
}

STATIC
CONST CHAR16 *
TypeTag (
  IN UNIXNU_ENTRY  *Entry
  )
{
  switch (Entry->Type) {
    case UniXNUEntryLinux:
      return L"linux";
    case UniXNUEntryXnu:
      return L"xnu";
    case UniXNUEntryAction:
      return L"action";
    default:
      return L"efi";
  }
}

VOID
UniXNUUiDrawEntry (
  IN UNIXNU_UI      *Ui,
  IN UNIXNU_CONFIG  *Config,
  IN UINTN      Index,
  IN BOOLEAN    Selected
  )
{
  UNIXNU_ENTRY                       *Entry;
  UINTN                          Y;
  UINTN                          Pad;
  UINTN                          Cw;
  UINTN                          Ch;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Bg;
  CONST CHAR16                   *Tag;
  CHAR16                         Line[160];

  if (Index >= Ui->VisibleRows) {
    return;
  }

  Entry = &Config->Entries[Index];
  if (Ui->Mode == UniXNUUiText) {
    UnicodeSPrint (Line, sizeof (Line), L" %c %u. %s  [%s]", Selected ? L'>' : L' ', (UINT32)Index + 1, Entry->Title, TypeTag (Entry));
    TextPrintAt (
      Ui,
      Ui->MenuTop + Index,
      Selected ? (EFI_WHITE | EFI_BACKGROUND_RED) : (EFI_LIGHTGRAY | EFI_BACKGROUND_BLACK),
      Line
      );
    return;
  }

  if (Ui->Mode != UniXNUUiGraphics) {
    return;
  }

  Cw  = UNIXNU_GLYPH_WIDTH * Ui->Scale;
  Ch  = UNIXNU_GLYPH_HEIGHT * Ui->Scale;
  Y   = Ui->MenuTop + Index * Ui->RowHeight;
  Pad = (Ui->RowHeight - Ch) / 2;
  Bg  = Selected ? mAccent : mRow;
  Tag = TypeTag (Entry);

  //
  // Leave a one-scale-unit gap between rows.
  //
  FillRect (Ui, Ui->MenuLeft, Y, Ui->MenuWidth, Ui->RowHeight - Ui->Scale, Bg);
  if (Selected) {
    FillRect (Ui, Ui->MenuLeft, Y, Ui->Scale * 3, Ui->RowHeight - Ui->Scale, mMarker);
  }
  UnicodeSPrint (Line, sizeof (Line), L"%u  %s", (UINT32)Index + 1, Entry->Title);
  DrawText (Ui, Ui->MenuLeft + Cw, Y + Pad, Line, Ui->Scale, mText, Bg, Ui->MenuWidth - (StrLen (Tag) + 3) * Cw);
  DrawText (
    Ui,
    Ui->MenuLeft + Ui->MenuWidth - (StrLen (Tag) + 1) * Cw,
    Y + Pad,
    Tag,
    Ui->Scale,
    Selected ? mText : mDim,
    Bg,
    0
    );
}

VOID
UniXNUUiDrawStatus (
  IN UNIXNU_UI         *Ui,
  IN UINTN         Line,
  IN CONST CHAR16  *Text
  )
{
  UINTN  Ch;
  UINTN  Y;

  if (Line >= STATUS_LINES) {
    return;
  }

  if (Ui->Mode == UniXNUUiText) {
    TextPrintAt (Ui, Ui->FooterTop + Line, EFI_LIGHTGRAY | EFI_BACKGROUND_BLACK, Text);
    return;
  }

  if (Ui->Mode != UniXNUUiGraphics) {
    return;
  }

  Ch = UNIXNU_GLYPH_HEIGHT * Ui->Scale;
  Y  = Ui->FooterTop + Line * (Ch + Ch / 3);
  FillRect (Ui, 0, Y, Ui->Width, Ch, mBackground);
  DrawCentered (Ui, Y, Text, Ui->Scale, (Line == 0) ? mText : mDim);
}
