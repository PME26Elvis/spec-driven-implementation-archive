# Repository format (implementation summary)

Matches task pack format v1:

- FORMAT file identifies DARC format=1, hash=sha256, chunking=buzhash64, compression=lzh1, parity=xor8+1
- Objects under objects/sha256/aa/<62hex>
- Framed as DARCOBJ1 + type + codec + lengths + header_crc32c + payload + payload_crc32c
- CID = SHA256("DARC\0" || type_tag || version_u16_le || semantic_payload)
- Types: 1=CHUNK 2=FILE 3=TREE 4=SNAPSHOT 5=PARITY
- refs/snapshots/<cid>, HEAD, index/chunks.idx, journal/, tmp/, locks/
