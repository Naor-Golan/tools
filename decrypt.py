# ------------------------------------------------------------
#  decrypt_log.py   –  XOR-decrypt log.enc (key 0xAB)
# ------------------------------------------------------------
#  Place this file next to log.enc and run:
#      python decrypt_log.py
#  Output: log.txt (human-readable) + console preview
# ------------------------------------------------------------

import os
import sys

XOR_KEY = 0xAB                     # <-- must match C++ key
IN_FILE = "log.enc"                # input (encrypted)
OUT_FILE = "log.txt"               # output (decrypted)

def xor_decrypt(data: bytes, key: int) -> bytes:
    return bytes(b ^ key for b in data)

def main():
    path = os.path.join(os.path.dirname(__file__), IN_FILE)
    if not os.path.isfile(path):
        print(f"[!] {IN_FILE} not found in the current directory.")
        sys.exit(1)

    # Read encrypted file
    with open(path, "rb") as f:
        encrypted = f.read()

    # Decrypt
    decrypted = xor_decrypt(encrypted, XOR_KEY)

    # Write plain-text log
    out_path = os.path.join(os.path.dirname(__file__), OUT_FILE)
    with open(out_path, "wb") as f:
        f.write(decrypted)

    # Show preview (first 10 lines)
    try:
        preview = decrypted.decode("utf-8", errors="replace").splitlines()[:10]
        print(f"[+] Decrypted {len(encrypted)} bytes to {OUT_FILE}")
        print("\n--- Preview (first 10 lines) ---")
        for line in preview:
            print(line)
        if len(decrypted.decode("utf-8", errors="replace").splitlines()) > 10:
            print("...")
        print("--- End Preview ---\n")
    except Exception:
        print(f"[+] Decrypted to {OUT_FILE} (binary or encoding issue)")

if __name__ == "__main__":
    main()