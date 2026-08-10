# Requirement-to-Evidence Matrix Template

版本：1.0.0

## 1. 使用要求

實作者必須複製本模板建立完成版 matrix。不得只填 catalog 短句；每列需指向實際模組、測試案例與結果檔。

## 2. 欄位

| 欄位 | 必填內容 |
|---|---|
| Requirement ID | `17_ACCEPTANCE_REQUIREMENT_CATALOG.md` 的穩定 ID |
| Normative source | 精確文件與章節 |
| Implementation module | source path與主要symbol/command |
| Unit evidence | test case名稱與result path；不適用要說明 |
| Integration evidence | scenario/test與result path |
| E2E evidence | scenario ID與result path |
| Visual evidence | screenshot/recording/UI probe path |
| Failure evidence | corruption/failure case；不適用要說明 |
| Manual checklist | checklist item |
| Final status | PASS/FAIL/BLOCKED/N/A |
| Notes | assertion對應、限制或缺陷ID |

## 3. 狀態規則

- PASS：所有必要 evidence存在且通過。
- FAIL：任一必要行為缺少、錯誤或證據失敗。
- BLOCKED：驗收無法執行；不算完成。
- N/A：只有catalog或normative source明確允許時可用。

## 4. Evidence adequacy

### 可接受

- solver requirement：fixed fixture unit tests + unique/multi/no-solution integration + batch。
- Hint requirement：structured step unit + real UI preview/apply E2E + screenshot。
- blur requirement：blur primitive unit + modal/nav E2E + recording。
- vault requirement：known-answer + tamper/failure + restart E2E。

### 不足

- 只有source path，無執行證據。
- 只有screenshot證明演算法。
- 只有unit test證明整個UI流程。
- 只有文字宣稱。
- 結果檔不存在或source commit不同。

## 5. Matrix completeness checks

完成版至少滿足：

- Catalog所有ID至少出現一次。
- 無未知ID。
- 所有PASS列至少一個自動化證據。
- UI-*中動畫／視覺項至少一個visual evidence。
- SEC-*至少unit+failure/integration。
- SDK solver/generator/difficulty至少unit+batch。
- STA-*至少table-driven state test與相關E2E。
- DEL-*至少release gate report。

## 6. Row template

```text
| SDK-02 | 05 §10; 07 §3–5 | src/sudoku/validator.c:board_validate | unit.board_conflicts.* | integration.submit_rules | SUBMIT-E2E-01..04 | submit_multi_conflict.png | n/a | H.* | PASS | all conflict masks asserted |
```

## 7. Summary

完成版末尾需列：

- catalog ID total。
- PASS/FAIL/BLOCKED/N/A。
- requirements with no unit evidence。
- requirements with no E2E evidence。
- requirements with no visual evidence where required。
- source commit與matrix generation time。
