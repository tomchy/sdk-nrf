#!/usr/bin/env python3
#
# Copyright (c) 2026 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#
# Pack the manufacturing-app key blob from individual key files. The blob is
# embedded in the signed image as an MCUboot protected TLV. The fixed key
# layout MUST stay in sync with src/mfg_key_blob.h.

import argparse
import struct
import sys
from hashlib import sha256
from pathlib import Path
from typing import Optional

try:
    from cryptography.hazmat.primitives import serialization
    from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PublicKey
    from cryptography.hazmat.primitives.asymmetric.ec import EllipticCurvePublicKey
except ImportError:
    sys.exit("ERROR: 'cryptography' package required (pip install cryptography)")

try:
    from intelhex import IntelHex
except ImportError:
    sys.exit("ERROR: 'intelhex' package required (pip install intelhex)")


MFG_KEY_BLOB_MAGIC   = 0x4B47464D
MFG_KEY_BLOB_VERSION = 1

# (field name, length). Order MUST match struct mfg_key_blob (fixed fields only).
KEY_FIELDS = [
    ("urot_pubkey_gen0",     32),
    ("urot_pubkey_gen1",     32),
    ("mfg_app_pubkey",       32),
    ("ikg_seed",             48),
    ("keyram_random0",       16),
    ("keyram_random1",       16),
    ("urot_pubkey_gen0_sig", 64),
    ("urot_pubkey_gen1_sig", 64),
]

AREA_DIGEST_FORMAT = "<II"
AREA_DIGEST_SIZE = struct.calcsize(AREA_DIGEST_FORMAT) + 32

HEADER_FORMAT = "<IHHI"
HEADER_LEN    = struct.calcsize(HEADER_FORMAT)


def pem_to_psa_bytes(pem_path: Path) -> bytes:
    """Return the PSA raw byte form of a PEM public key (Ed25519 -> 32 B,
    P-256 -> 65 B uncompressed)."""
    text = pem_path.read_text()
    if "-----BEGIN" not in text:
        raise ValueError(f"{pem_path.name}: not in PEM format")

    key = serialization.load_pem_public_key(text.encode())
    if isinstance(key, Ed25519PublicKey):
        return key.public_bytes(serialization.Encoding.Raw,
                                serialization.PublicFormat.Raw)
    if isinstance(key, EllipticCurvePublicKey):
        return key.public_bytes(serialization.Encoding.X962,
                                serialization.PublicFormat.UncompressedPoint)
    raise ValueError(f"{pem_path.name}: unsupported key type {type(key).__name__}")


def get_digest(path: Path) -> Optional[tuple[int, int, bytes]]:
    """Return (address, size, SHA256 digest) for an IntelHex file."""
    if not path.exists():
        return None

    if path.suffix.lower() != ".hex":
        raise ValueError(f"{path.name}: unsupported file type (only .hex supported)")
    ih = IntelHex()
    ih.loadhex(str(path))
    start = ih.minaddr()
    end = ih.maxaddr()
    return start, end - start + 1, sha256(ih.tobinstr()).digest()


def field_bytes(name: str, length: int, keys_dir: Path) -> bytes:
    """Look up the bytes for a single struct field."""
    if name in ("urot_pubkey_gen0", "urot_pubkey_gen1", "mfg_app_pubkey"):
        pem = keys_dir / f"{name}.pem"
        if pem.exists():
            data = pem_to_psa_bytes(pem)
            if len(data) != length:
                raise ValueError(
                    f"{pem.name}: PSA byte length {len(data)} != {length}")
            return data

    if name in ("ikg_seed", "keyram_random0", "keyram_random1"):
        bin_path = keys_dir / f"{name}.bin"
        if bin_path.exists():
            data = bin_path.read_bytes()
            if len(data) != length:
                raise ValueError(
                    f"{bin_path.name}: length {len(data)} != {length}")
            return data

    if name.endswith("_sig"):
        # urot_pubkey_gen0_sig -> key_verification_msgs/urot_pubkey_gen0_signed.msg
        base = name[:-len("_sig")]
        msg = keys_dir / "key_verification_msgs" / f"{base}_signed.msg"
        if msg.exists():
            data = msg.read_bytes()
            if len(data) != length:
                raise ValueError(
                    f"{msg.name}: length {len(data)} != {length}")
            return data

    return b"\x00" * length


def build_area_digests(area_digests: list[Path]) -> bytes:
    """Pack (address, size, digest) entries for each --area-digest hex file."""
    all_digests = bytes()
    for file in area_digests:
        result = get_digest(file)
        if result is None:
            continue
        address, size, digest = result
        all_digests += struct.pack(AREA_DIGEST_FORMAT, address, size) + digest
    return all_digests


def build_blob(keys_dir: Path, area_digests: list[Path]) -> bytes:
    payload = b"".join(field_bytes(n, l, keys_dir) for n, l in KEY_FIELDS)
    payload += build_area_digests(area_digests)
    blob_len = HEADER_LEN + len(payload)
    header = struct.pack(HEADER_FORMAT,
                         MFG_KEY_BLOB_MAGIC,
                         MFG_KEY_BLOB_VERSION,
                         0,
                         blob_len)
    return header + payload


def main() -> None:
    p = argparse.ArgumentParser(description="Pack manufacturing key blob.")
    p.add_argument("--keys-dir", required=True, type=Path)
    p.add_argument("--out-bin", required=True, type=Path)
    p.add_argument("--area-digest", required=False, type=Path,
                   action="append", default=[],
                   help="HEX file to verify through digest checks.")
    args = p.parse_args()

    if not args.keys_dir.is_dir():
        sys.exit(f"--keys-dir not a directory: {args.keys_dir}")

    blob = build_blob(args.keys_dir, args.area_digest)

    args.out_bin.parent.mkdir(parents=True, exist_ok=True)
    args.out_bin.write_bytes(blob)

if __name__ == "__main__":
    main()
