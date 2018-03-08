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

#include "shim.h"
#include <string.h>
#include "transparent.h"

static EFI_GUID loaded_image_guid = LOADED_IMAGE_PROTOCOL;

static EFI_GUID loaded_image_path_guid = {
	0xbc62157e, 0x3e33, 0x4fec,
	{ 0x99, 0x20, 0x2d, 0x3b, 0x36, 0xd7, 0x50, 0xdf }
};

static EFI_GUID transparent_image_guid = {
	0x915fc1fd, 0x8a37, 0x4a3d,
	{ 0xa4, 0x2e, 0xd1, 0x10, 0xfc, 0x0d, 0x5a, 0xf0 }
};

struct transparent_image {
	EFI_LOADED_IMAGE_PROTOCOL loaded;
	EFI_PHYSICAL_ADDRESS memory;
	UINTN pages;
	entry_point_t entry;
};

static EFI_DEVICE_PATH * devpath_end(EFI_DEVICE_PATH *path) {

	while (path->Type != END_DEVICE_PATH_TYPE) {
		path = (((void *) path) +
			((path->Length[1] << 8) | path->Length[0]));
	}
	return path;
}

struct transparent_image * transparent_get(EFI_HANDLE image) {
	void *interface;
	EFI_STATUS rc;

	rc = uefi_call_wrapper(BS->OpenProtocol, 6, image,
			       &transparent_image_guid, &interface,
			       image, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
	if (EFI_ERROR(rc))
		return NULL;
	return interface;
}

EFI_STATUS transparent_load_image(EFI_SYSTEM_TABLE *systab,
				  EFI_HANDLE parent_image,
				  EFI_DEVICE_PATH *path,
				  VOID *buffer, UINTN size,
				  EFI_HANDLE *image) {
	EFI_DEVICE_PATH *end;
	struct transparent_image *trans;
	UINTN path_len;
	EFI_STATUS rc;

	/* Sanity check */
	if ((parent_image == NULL) || (path == NULL) || (buffer == NULL) ||
	    (image == NULL)) {
		rc = EFI_INVALID_PARAMETER;
		goto err_invalid;
	}

	/* Allocate and initialise structure */
	end = devpath_end(path);
	path_len = (((void *)end - (void *)path) + sizeof(*end));
	trans = AllocatePool(sizeof(*trans) + path_len);
	if (!trans) {
		rc = EFI_OUT_OF_RESOURCES;
		goto err_alloc;
	}
	memset(trans, 0, sizeof(*trans));
	trans->loaded.Revision = EFI_LOADED_IMAGE_PROTOCOL_REVISION;
	trans->loaded.ParentHandle = parent_image;
	trans->loaded.SystemTable = systab;
	trans->loaded.FilePath = (void *)trans + sizeof(*trans);
	memcpy(trans->loaded.FilePath, path, path_len);

	/* Verify and relocate image */
	rc = handle_image(buffer, size, &trans->loaded, &trans->memory,
			  &trans->pages, &trans->entry);
	if (EFI_ERROR(rc))
		goto err_handle_image;

	/* Install protocols onto a new handle */
	*image = NULL;
	rc = uefi_call_wrapper(BS->InstallMultipleProtocolInterfaces, 8,
			       image, &transparent_image_guid, trans,
			       &loaded_image_path_guid, trans->loaded.FilePath,
			       &loaded_image_guid, &trans->loaded,
			       NULL);
	if (EFI_ERROR(rc))
		goto err_install;

	return 0;

 err_install:
	uefi_call_wrapper(BS->FreePages, 2, trans->memory, trans->pages);
 err_handle_image:
	FreePool(trans);
 err_alloc:
 err_invalid:
	return rc;
}

EFI_STATUS transparent_start_image(struct transparent_image *trans,
				   EFI_HANDLE image, UINTN *exit_data_size,
				   CHAR16 **exit_data) {
	EFI_STATUS rc;

	//
	Print(L"Calling entry point %x", trans->entry);

	/* Call entry point */
	rc = uefi_call_wrapper(trans->entry, 2, image,
			       trans->loaded.SystemTable);
	return rc;
}

EFI_STATUS transparent_unload_image(struct transparent_image *trans,
				    EFI_HANDLE image) {

	uefi_call_wrapper(BS->UninstallMultipleProtocolInterfaces, 8,
			  image, &transparent_image_guid, trans,
			  &loaded_image_path_guid, trans->loaded.FilePath,
			  &loaded_image_guid, &trans->loaded,
			  NULL);
	uefi_call_wrapper(BS->FreePages, 2, trans->memory, trans->pages);
	FreePool(trans);
	return 0;
}
