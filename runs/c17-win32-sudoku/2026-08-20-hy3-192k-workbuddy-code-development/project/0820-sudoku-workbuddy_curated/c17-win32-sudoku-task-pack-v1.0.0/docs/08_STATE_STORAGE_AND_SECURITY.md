# 狀態、存檔與資料安全

## 1. 儲存目標

應用程式必須保存：

- 多個未完成遊戲。
- 完成紀錄。
- 使用者設定。
- vault metadata。
- 每局 undo/redo history。

資料不得以明文保存。

## 2. Vault 模型

首次啟動：

- 使用者建立本機 vault 密碼。
- 密碼為可列印 ASCII 32–126，長度 8–64 bytes；內部空白不得自動 trim。
- 要求輸入兩次確認。
- 不得顯示明文密碼。
- 建立成功前不得建立空的有效 vault。

後續啟動：

- 要求密碼解鎖。
- 密碼錯誤顯示一般錯誤。
- 不得顯示內部 key、tag 或解密細節。
- 本版本不要求密碼復原。

## 3. 密碼衍生

使用 PBKDF2-HMAC-SHA-256：

- SHA-256 自行實作。
- HMAC-SHA-256 自行實作。
- PBKDF2 自行實作。
- salt 至少 16 bytes，使用 `BCryptGenRandom()`。
- iteration count 必須保存於 header。
- iteration count 固定為 200,000；測試模式可使用較小值加速非效能測試，但 production vault 不得降低。
- derived key 不得寫入磁碟。

## 4. Authenticated Encryption

使用 XChaCha20-Poly1305：

- ChaCha20 core 自行實作。
- HChaCha20 自行實作。
- Poly1305 自行實作。
- 每次寫入使用新的 24-byte nonce。
- nonce 使用 `BCryptGenRandom()`。
- authentication tag 必須驗證後才能接受 plaintext。
- 驗證 tag 前不得將解密資料當成有效資料解析。

不得使用：

- XOR cipher。
- Caesar／Vigenère。
- Base64。
- 未驗證的 stream cipher。
- 固定 nonce。
- 密碼直接當 key。

## 5. 檔案格式

加密容器至少包含：

- magic bytes。
- format version。
- KDF identifier。
- iteration count。
- salt。
- cipher identifier。
- nonce。
- ciphertext length。
- ciphertext。
- authentication tag。

header 中非秘密欄位應納入 authenticated additional data，避免被未偵測竄改。

## 6. Plaintext payload

解密後 payload 必須為自行定義的版本化 binary format。
不得直接依賴第三方 serializer。

至少包含：

- payload version。
- settings。
- in-progress game count。
- completed record count。
- 每筆長度或可安全跳過的 framing。
- checksum 可選；AEAD tag 已提供整體完整性，但內部長度仍需嚴格檢查。

## 7. Game record

未完成遊戲至少保存：

- game ID。
- original clues。
- current values。
- notes masks。
- assisted value mask。
- active elapsed time。
- created time。
- last played time。
- pause state。
- undo stack。
- redo stack。
- generator mode／seed metadata。
- dirty/save generation number。

## 8. Completed record

至少保存：

- game ID。
- original clues。
- completed grid。
- completion timestamp。
- active elapsed time。
- player completion flag。
- auto-solve/assisted flag。
- generator mode。

Completed record 不需要保存可繼續操作的 undo history。

## 9. 原子寫入

保存流程：

1. serialize新plaintext snapshot。
2. 以`BCryptGenRandom`產生fresh nonce。
3. encrypt/authenticate。
4. 以`CreateFileW(..., CREATE_NEW)`在同目錄建立temporary file。
5. 完整寫入、檢查byte count並`FlushFileBuffers`。
6. target不存在時以same-volume `MoveFileExW(..., MOVEFILE_WRITE_THROUGH)`建立current。
7. target存在時以`ReplaceFileW`原子替換並保留known-good backup。
8. 只有replacement成功後才更新memory saved generation。

任何階段中斷不得使current與known-good backup同時失效。Windows精確語意見`26` §14。

## 10. Backup 與 recovery

至少保留：

- current vault file。
- previous known-good backup。

載入時：

- current 有效：使用 current。
- current 無效、backup 有效：提示可從 backup recovery。
- 兩者皆無效：顯示無法開啟，不得自動清空覆寫。
- recovery 後保留損毀檔供診斷，不得默默刪除。

## 11. 密碼錯誤與損毀

由於 authenticated encryption 的性質，錯誤密碼與被竄改資料可能同樣造成 tag failure。
UI 可以顯示：

> Password is incorrect or the data file is corrupted.

不得聲稱能在所有情況精確區分兩者。

## 12. 記憶體中的敏感資料

- 密碼輸入buffer使用後應以`SecureZeroMemory`或等效volatile wipe覆寫。
- derived key與plaintext payload結束使用前應以不會被compiler省略的方法覆寫。
- 不得寫入 log。
- crash report／test output 不得包含明文 vault payload。
- password field 不得支援複製出明文；貼上可選。

## 13. 時間資料

- active elapsed 使用 monotonic duration 累積。
- created/updated/completed timestamp 可使用 wall clock。
- wall clock 倒退不得降低 active elapsed。
- timestamp serialization 使用明確整數單位與版本。

## 14. Dirty state

每局維護：

- current generation。
- last saved generation。

以下操作使 dirty：

- 正式值變更。
- notes 變更。
- Clear Answers。
- Undo/Redo 導致棋盤變更。
- Auto Solve。
- elapsed time 累積到需保存的粒度。

純 selection、hover 或頁面切換不必使 dirty。

## 15. 多局存檔一致性

- 一次 vault save 必須形成完整一致 snapshot。
- 不得只更新 index 而漏寫 game record。
- 刪除一局後 reload 不得復活。
- 完成一局時，從 in-progress 移除與加入 completed 必須是同一原子 payload 更新。

## 16. 格式版本

- 每個持久化格式有明確 version。
- 未知較新版本必須拒絕，不得猜測解析。
- 較舊版本若支援 migration，必須有測試。
- 本版本至少需要測試：正常版本、錯誤 magic、截斷 header、截斷 ciphertext、未知版本。

## 17. 密碼學測試

至少包含：

- SHA-256 公開已知向量。
- HMAC-SHA-256 公開已知向量。
- PBKDF2-HMAC-SHA-256 公開已知向量。
- ChaCha20/HChaCha20 公開或自行固定交叉驗證向量。
- Poly1305 公開已知向量。
- XChaCha20-Poly1305 固定向量。
- encrypt/decrypt round trip。
- wrong key failure。
- modified nonce/header/ciphertext/tag failure。
- repeated writes 產生不同 nonce/ciphertext。

Expected values 必須是固定已知值，不得由同一被測函式即時計算。

## 18. 難度與 Hint 格式相容性

- `difficulty_rules_version` 必須保存，避免未來規則變更後誤改既有紀錄的標籤。
- 載入舊資料時不得使用新規則偷偷重分類；若有 migration，需保留原始標籤並另存新評分。
- Hint trace 不需要完整永久保存，但完成紀錄必須保存計數、是否 assisted 與最高 technique。
- Undo transaction 若包含 Apply Hint，必須保存 assisted origin 與 peer-note removals。
- completed record 不得因 UI 重新開啟而增加 hints viewed 或 elapsed time。


## 19. v1.0 Canonical format

Vault outer header、AAD、payload framing、Settings、Game、Undo與Completed records以`19_CANONICAL_FORMATS_AND_LIMITS.md` §16–22為準。

所有parser必須先驗證magic/version/length/count/reserved bits，再配置或迭代。未知enum、trailing bytes、duplicate game ID、given/current/origin不一致均拒絕整個payload，不得猜測修復。

## 20. Save lock與Windows原子替換

同一process同時只允許一個vault save。Canonical流程：

1. 取得in-process save lock，並以`CreateFileW(CREATE_NEW, share=0)`建立跨process lock file。
2. 以memory snapshot序列化，不在寫入途中讀取會變動的live structures。
3. 產生fresh nonce並加密。
4. 在與current相同directory/volume建立exclusive temporary file。
5. 寫完整內容、檢查所有byte count、`FlushFileBuffers`並close temporary handle。
6. current不存在：以`MoveFileExW(temp,current,MOVEFILE_WRITE_THROUGH)`建立；若競爭出現則失敗。
7. current存在且已知有效：以`ReplaceFileW(current,temp,backup,REPLACEFILE_WRITE_THROUGH,...)`原子替換並更新backup。
8. current已知損毀但backup有效：不得以損毀current覆蓋backup；先將損毀current移到唯一diagnostic name，再依initial-create流程建立current。
9. 成功後才更新memory saved generation、移除自己建立的lock並釋放in-process lock。

任何write、flush、close、move、replace、backup、attribute或rollback失敗都不得宣稱成功。Loader必須仍可從current或known-good backup恢復；temporary與diagnostic path需列入failure report。Windows無通用directory fsync要求，完成標準以`26` §14的same-volume replacement、write-through、handle flush與故障注入為準。

## 21. Snapshot concurrency

若使用background save：

- worker取得不可變serialized snapshot或深拷貝，不直接遍歷會被UI修改的container。
- save request記錄vault generation。
- worker成功時只有current memory generation仍等於snapshot generation，才能把該generation標為saved。
- 若保存期間又有新操作，舊snapshot仍可寫入，但current game保持dirty。

## 22. Reset Application Data

成功Reset：

- 驗證當前vault密碼。
- 清除所有games、completed與custom settings。
- 保留salt/KDF policy可重新生成；每次寫入仍用fresh nonce。
- 保留相同vault password可解鎖的新空payload。
- theme回Dark、motion回Full、peer-note removal On、Confirm Auto Solve On、last difficulty Easy。
- 原子寫入成功前原資料保持。

## 23. Undo/Redo validation

載入history時：

- transaction/change counts不得超過`19`上限。
- cell index嚴格ascending且不重複。
- before/after value、origin、notes均在合法範圍。
- replay undo stack從original snapshot至current board必須一致，否則game record視為損毀。
- redo stack需可從current state按pop順序合法套用；不一致時拒絕該game或整個payload，v1.0 canonical行為為拒絕整個payload並提供backup recovery。

## 24. Crypto constant-time minimum

- authentication tag比較必須以固定長度累積差異，不得在第一個不同byte提前返回。
- password comparison不應存在；只依KDF+tag結果。
- Poly1305與key material運算不得以秘密內容作array越界或變長loop termination。
- v1.0不要求完整side-channel certification，但明顯early-exit tag comparison視為缺陷。

## 25. Plaintext lifetime

- decrypted payload解析完成後，暫存plaintext buffer應覆寫再free。
- serialized plaintext保存完成或失敗後應覆寫。
- screenshots/UI probe不得包含password field的真實內容。
- failure fixture可包含非敏感固定payload，但正式使用者資料不得複製到results。


## 26. Persisted store與current memory game分離

應用程式必須能區分：

- 最後成功解密／保存的persisted store。
- current game的live memory state。

手動Game Save以live current game替換persisted store中同ID record。
Global settings save、刪除其他game或其他metadata-only operation必須從persisted store建snapshot，不得隱含寫入current dirty memory state或unsaved draft。

完成Submit是明確例外：它以live current game建立Completed record並在同一snapshot移除In Progress。

## 27. Settings save sequencing

- 每個global save有request generation。
- 若較舊save晚於較新意圖完成，較舊結果不得回覆/覆蓋UI persisted setting。
- 可序列化save queue，或在worker結果套用時檢查generation。
- 每次正式寫入仍使用fresh nonce。
- 失敗時UI回復最後persisted setting，current game dirty狀態不變。
