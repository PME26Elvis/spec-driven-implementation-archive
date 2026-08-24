# On-disk format (summary)

- Page size: 4096 bytes
- Page 0: database header (magic EDB1, page size, feature flags, KDF, freelist_root, last_page, schema root)
- Schema catalog: page 1 region / catalog save
- B+ tree pages: leaf/internal after header; cell directory from end
- Overflow chains: type OVERFLOW, next pointer, payload
- Freelist pages: next + count + page numbers
- Optional AEAD: XChaCha20-Poly1305 per page when encrypted
- Row payload: optional RV xmin/xmax header then typed columns

See source `src/pager/pager.c`, `src/btree/btree.c`, `src/record/overflow.c`.
