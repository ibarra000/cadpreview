# Containers

Generic container formats that several CAD formats are expected to use. Which CAD formats use
which container is still unconfirmed; it gets recorded in each format's notes once checked
against real samples.

## compound-file-signature

**Claim:** An OLE compound file (Microsoft Compound File Binary format) begins with the 8-byte
header signature `D0 CF 11 E0 A1 B1 1A E1`. The full header is 512 bytes.
**Status:** confirmed (published spec)
**Evidence:** [MS-CFB] section 2.2 "Compound File Header", field "Header Signature";
`corpus/synthetic/cfb-signature-only.bin`.
**Method:** read from the published specification. The synthetic fixture holds only the
signature plus zero bytes, so it tests our detector, not real files.
**Versions:** [MS-CFB], current revision on learn.microsoft.com (checked 2026-11-14).

Detection in `src/formats/detect.cpp` checks only these 8 bytes. A full reader (header
validation, FAT and directory walking) is planned with the first format that needs it.

## zip-local-file-header

**Claim:** A ZIP archive normally begins with a local file header, whose signature is the
little-endian value `0x04034b50`, stored as bytes `50 4B 03 04` ("PK\x03\x04").
**Status:** confirmed (published spec)
**Evidence:** PKWARE APPNOTE.TXT section 4.3.7 "Local file header";
`corpus/synthetic/minimal-stored.zip`.
**Method:** read from the published specification; the fixture was generated with Python's
`zipfile` module.
**Versions:** APPNOTE.TXT 6.3.10.

Caveat: archives may begin with other data (self-extracting stubs, prepended headers), and an
empty archive holds only the end-of-central-directory record (APPNOTE 4.3.16). Robust detection
should locate that record from the end of the file; revisit before relying on ZIP detection for
a CAD format.
