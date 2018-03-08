/*
 * transparent - UEFI shim transparent LoadImage()/StartImage() support
 *
 * Copyright (C) 2018 Michael Brown <mbrown@fensystems.co.uk>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef _TRANSPARENT_H
#define _TRANSPARENT_H

#include <efi.h>
#include <efiapi.h>

struct transparent_image;

extern struct transparent_image * transparent_get(EFI_HANDLE image);
extern EFI_STATUS transparent_load_image(EFI_SYSTEM_TABLE *systab,
					 EFI_HANDLE parent_image,
					 EFI_DEVICE_PATH *path,
					 VOID *buffer, UINTN size,
					 EFI_HANDLE *image);
extern EFI_STATUS transparent_start_image(struct transparent_image *trans,
					  EFI_HANDLE image,
					  UINTN *exit_data_size,
					  CHAR16 **exit_data);
extern EFI_STATUS transparent_unload_image(struct transparent_image *trans,
					   EFI_HANDLE image);

#endif /* _TRANSPARENT_H */
