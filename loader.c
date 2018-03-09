/*
 * loader - UEFI shim transparent LoadImage()/StartImage() support
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

#include "shim.h"
#include <string.h>
#include <efi.h>
#include <efisetjmp.h>
#include "loader.h"

static EFI_GUID loaded_image_path_guid = {
	0xbc62157e, 0x3e33, 0x4fec,
	{ 0x99, 0x20, 0x2d, 0x3b, 0x36, 0xd7, 0x50, 0xdf }
};

static EFI_GUID loaded_image_guid = LOADED_IMAGE_PROTOCOL;

static EFI_GUID loader_guid = LOADER_PROTOCOL_GUID;

struct image {
	LOADER_PROTOCOL loader; /* Must be first */
	EFI_LOADED_IMAGE_PROTOCOL li;

	EFI_HANDLE handle;
	EFI_PHYSICAL_ADDRESS memory;
	UINTN pages;
	EFI_IMAGE_ENTRY_POINT entry;

	BOOLEAN running;
	EFI_STATUS exit_status;
	UINTN exit_data_size;
	CHAR16 *exit_data;

	jmp_buf jmp_buf;
};

static EFI_DEVICE_PATH * devpath_end(EFI_DEVICE_PATH *path) {

	while (path->Type != END_DEVICE_PATH_TYPE) {
		path = (((void *) path) +
			((path->Length[1] << 8) | path->Length[0]));
	}
	return path;
}

LOADER_PROTOCOL *loader_protocol(EFI_HANDLE image) {
	void *interface;
	EFI_STATUS rc;

	rc = uefi_call_wrapper(BS->OpenProtocol, 6, image, &loader_guid,
			       &interface, image, NULL,
			       EFI_OPEN_PROTOCOL_GET_PROTOCOL);
	if (EFI_ERROR(rc))
		return NULL;
	return interface;
}

static EFI_STATUS EFIAPI loader_start_image(LOADER_PROTOCOL *This,
					    UINTN *ExitDataSize,
					    CHAR16 **ExitData) {
	struct image *image = (struct image *) This;
	EFI_STATUS rc;

	if (image->running)
		return EFI_ALREADY_STARTED;

	image->running = TRUE;
	image->exit_data_size = 0;
	image->exit_data = NULL;
	if (setjmp(&image->jmp_buf) == 0 ) {
		rc = uefi_call_wrapper(image->entry, 2, image->handle,
				       image->li.SystemTable);
	} else {
		rc = image->exit_status;
	}
	image->running = FALSE;
	if (ExitDataSize)
		*ExitDataSize = image->exit_data_size;
	if (ExitData)
		*ExitData = image->exit_data;

	return rc;
}

static EFI_STATUS EFIAPI loader_unload_image(LOADER_PROTOCOL *This) {
	struct image *image = (struct image *) This;

	uefi_call_wrapper(BS->UninstallMultipleProtocolInterfaces, 8,
			  image->handle, &loader_guid, &image->loader,
			  &loaded_image_path_guid, &image->li.FilePath,
			  &loaded_image_guid, &image->li,
			  NULL);
	uefi_call_wrapper(BS->FreePages, 2, image->memory, image->pages);
	FreePool(image);
	return 0;
}

static EFI_STATUS EFIAPI loader_exit(LOADER_PROTOCOL *This,
				     EFI_STATUS ExitStatus, UINTN ExitDataSize,
				     CHAR16 *ExitData) {
	struct image *image = (struct image *) This;

	if (!image->running)
		return EFI_NOT_STARTED;
	image->exit_status = ExitStatus;
	image->exit_data_size = ExitDataSize;
	image->exit_data = ExitData;
	longjmp(&image->jmp_buf, 1);
}

EFI_STATUS loader_install(EFI_SYSTEM_TABLE *systab, EFI_HANDLE parent_image,
			  EFI_DEVICE_PATH *path, VOID *buffer, UINTN size,
			  EFI_HANDLE *handle) {
	struct image *image;
	EFI_DEVICE_PATH *end;
	UINTN path_len;
	EFI_STATUS rc;

	if ((parent_image == NULL) || (path == NULL) || (buffer == NULL) ||
	    (handle == NULL)) {
		rc = EFI_INVALID_PARAMETER;
		goto err_invalid;
	}

	end = devpath_end(path);
	path_len = (((void *)end - (void *)path) + sizeof(*end));
	image = AllocatePool(sizeof(*image) + path_len);
	if (!image) {
		rc = EFI_OUT_OF_RESOURCES;
		goto err_alloc;
	}
	memset(image, 0, sizeof(*image));
	image->loader.StartImage = loader_start_image;
	image->loader.UnloadImage = loader_unload_image;
	image->loader.Exit = loader_exit;
	image->li.Revision = EFI_LOADED_IMAGE_PROTOCOL_REVISION;
	image->li.ParentHandle = parent_image;
	image->li.SystemTable = systab;
	image->li.FilePath = (void *)image + sizeof(*image);
	memcpy(image->li.FilePath, path, path_len);

	rc = handle_image(buffer, size, &image->li, &image->memory,
			  &image->pages, &image->entry);
	if (EFI_ERROR(rc))
		goto err_handle_image;

	rc = uefi_call_wrapper(BS->InstallMultipleProtocolInterfaces, 8,
			       &image->handle, &loader_guid, &image->loader,
			       &loaded_image_path_guid, image->li.FilePath,
			       &loaded_image_guid, &image->li,
			       NULL);
	if (EFI_ERROR(rc))
		goto err_install;

	*handle = image->handle;
	return 0;

 err_install:
	uefi_call_wrapper(BS->FreePages, 2, image->memory, image->pages);
 err_handle_image:
	FreePool(image);
 err_alloc:
 err_invalid:
	return rc;
}

