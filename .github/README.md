# shim for iPXE

This is a lightly modified version of the general-purpose [shim][shim]
used to enable UEFI Secure Boot for open source projects.

This fork includes modifications to simplify the use of shim with
[iPXE][ipxe].  In particular, this shim is built to trust the iPXE
project's [EV code signing certificate](../ipxe.der), and includes
logic to automatically determine the iPXE filename based on the name
used for the shim itself, by stripping out the shim portion of the
filename.  For example:

| shim filename         | iPXE filename         |
| :-------------------- | :-------------------- |
| ipxe-shimx64.efi      | ipxe.efi              |
| intel-shimx64.efi     | intel.efi             |
| snponly-shimx64.efi   | snponly.efi           |

The UEFI Secure Boot signed binaries can be downloaded from the
releases listed at https://github.com/ipxe/shim/releases

(Note that only full releases are signed: prereleases will contain
unsigned binaries.)

[shim]: https://github.com/rhboot/shim
[ipxe]: https://ipxe.org
