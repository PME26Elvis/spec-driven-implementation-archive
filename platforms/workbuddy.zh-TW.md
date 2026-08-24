# Tencent WorkBuddy 平台介紹與比較分析

**研究日期：** 2026-08-24  
**用途：** `spec-driven-implementation-archive` 的 execution-surface 參考文件  
**文件性質：** 繁體中文分析版，非騰訊官方文件  
**主要對象：** Tencent WorkBuddy 桌面 Agent、Hy3，以及與主流 Agent 平台的定位比較  
**English version：** [workbuddy.md](workbuddy.md)

> [!IMPORTANT]
> WorkBuddy 是快速演進中的產品。模型清單、方案價格、Credits、Skills 數量、UI 名稱、Connector/MCP 支援、企業部署方式都可能在短期內改變。本文件將資訊區分成三層：**官方可確認事實**、**廠商自述／內部指標**、以及**分析與推論**。若某次 benchmark run 的實際設定與本文不同，應以該 run 自己的 README、截圖、conversation export 與最終 project snapshot 為準。

---

# 1. 先用一句話理解 WorkBuddy

**WorkBuddy 不是「一個聊天機器人換成桌面 App」，也不是「CodeBuddy 加上一點 Office 功能」。它更接近一個以自然語言驅動的通用型 AI 工作台：使用者提出目標，平台負責規劃、讀寫本機檔案、呼叫模型與工具、連接外部服務，最後交付可檢查的實體成果。**

這個定位和純聊天產品、純 IDE Copilot、純 terminal coding agent 都不完全相同。

對一般使用者而言，WorkBuddy 的核心價值是「把任務做完」：整理資料夾、分析 CSV、做簡報、寫報告、修改程式、跑命令、查資料、連接 GitHub/Jira/Gmail/Slack，最後留下文件、表格、程式碼或其他 artifact。

對本 repository 而言，更重要的一點是：

> **WorkBuddy run 測到的是一整個 execution surface，而不是只有 foundation model。**

實際結果可能同時受以下因素影響：

- WorkBuddy 自己的 agent harness / orchestration；
- 選到的模型，或 Auto routing；
- Hy3 的 reasoning / thinking mode；
- Ask / Craft / Plan；
- Default Permissions / Full Access；
- Workspace 與本機檔案；
- Skills；
- MCP；
- Connectors；
- Experts；
- Explore recipes；
- Memory；
- Automation / Assistant；
- 是否有其他 task / subagent 同時工作；
- account tier、region、產品版本；
- 外部 API、網路與第三方服務狀態。

因此，「WorkBuddy + Hy3」仍然是一個過於粗略的實驗標籤；後面會給出更適合 benchmark 的 metadata 格式。

---

# 2. 資訊可信度：本文怎麼區分「事實」和「宣傳」

WorkBuddy 很新，而且騰訊同時在推 WorkBuddy、CodeBuddy、WorkBuddy Enterprise、WMA、Hy3、WorkBuddy Bench。不同頁面的產品 vocabulary 仍在快速收斂，因此直接把官方網站所有敘述拼在一起，反而容易造成錯誤理解。

本文採三層證據模型。

## 2.1 官方可確認事實

優先使用：

- `workbuddy.ai` WorkBuddy 官方文件；
- Tencent Cloud WorkBuddy Enterprise 文件；
- Tencent 公司新聞稿／官方公告；
- WorkBuddy Privacy Policy 與 Service Agreement；
- `Tencent/workbuddy-bench` 官方 repository；
- 比較競品時，使用 Google、OpenAI、Anthropic、SpaceXAI/xAI 或 Pi 原作者的第一手資料。

這些資料可用來判斷「廠商目前公開承諾了什麼、產品有哪些功能」，但**官方來源不等於第三方驗證**。

## 2.2 廠商自述／內部指標

例如騰訊公開宣稱 Hy3 在 WorkBuddy 內部 workplace evaluation 中超過 90% task success、平均任務完成時間下降 34%。

這類數字有價值，但只能解讀為：

- 騰訊內部有一套評估；
- Hy3 是朝實際 Agent workload 調整；
- 廠商觀察到 end-to-end 成效提升。

不能直接把它寫成：

- 「Hy3 在獨立 benchmark 有 90% 成功率」；
- 「Hy3 比 Claude / GPT / Gemini 快 34%」；
- 「WorkBuddy 已被證明優於其他 agent」。

原因是 task set、grader、failure taxonomy、產品版本與對照組並未全部公開。

## 2.3 分析與推論

本文對 Antigravity、Codex、Claude Code、Grok Build、Pi 的比較，以及對 benchmark 設計的建議，是依各家公開 architecture / feature 進行的分析。

分析會盡量避免把 UI 行為反推出未公開的內部 implementation。

---

# 3. WorkBuddy 在騰訊產品線中的位置

理解 WorkBuddy 前，先把幾個容易混淆的名稱拆開。

| 產品 | 核心定位 | 主要使用情境 |
| --- | --- | --- |
| **CodeBuddy** | AI-native software development | Coding、IDE、CLI、開發工作流 |
| **WorkBuddy** | 通用工作型桌面 Agent | 本機檔案、Office artifact、研究、資料、程式、外部服務 |
| **WorkBuddy Managed Agents (WMA)** | 企業級雲端 Agent Runtime | 長時間、雲端、可治理、可追蹤的 Agent 執行 |
| **WorkBuddy Enterprise** | 企業產品／治理 umbrella | 將 CodeBuddy、WorkBuddy、WMA 放進企業管理框架 |

這四個名稱不能互換。

## 3.1 WorkBuddy 不是 CodeBuddy 的同義詞

兩者可能共享部分基礎設施、文件 portal、agent concepts，甚至歷史設定路徑仍可看到 `.codebuddy` 字樣，但：

- CodeBuddy 的 design center 是 software development；
- WorkBuddy 的 design center 是 general workplace task；
- WorkBuddy 對 Office artifact、本機非 repo 檔案、Connectors、Experts、Explore、Assistant 等能力更強調產品化。

因此，拿 CodeBuddy CLI 的行為直接推論 WorkBuddy desktop 的 contract 並不嚴謹。

## 3.2 WMA 也不是「WorkBuddy 雲端版」這麼簡單

Tencent Cloud 對 WorkBuddy Managed Agents 的公開說明，提供了一個非常有用的 agent architecture 視角。WMA 以 WorkBuddy Harness 為基礎，公開三個核心物件：

- **Agent**：模型、system role / prompt、Skills、tools；
- **Runtime**：獨立 Linux filesystem / terminal 環境，以及 Agent manifest / sessions；
- **Session**：一組獨立 conversation / history / context。

官方還把 harness 描述為包含 orchestration、memory、action、governance/control 等層。

這顯示騰訊在產品架構上確實把「模型」和「Agent harness」視為不同層級。

但 WMA 的雲端 sandbox、lifecycle、治理方式不應自動套用到 consumer desktop WorkBuddy；本文只把 WMA 當成**同一產品家族的 architecture context**。

---

# 4. WorkBuddy 的核心使用模型：Task，而不是 Chat

WorkBuddy 更適合以「Task」理解，而非傳統「Chat thread」。

一個 Task 通常有：

- 一個目標；
- 一個 workspace；
- 一組當前 context；
- 模型與 reasoning/routing；
- 可使用的 Skills / MCP / Connectors；
- 權限設定；
- 執行過程；
- 最終 deliverables。

官方文件也描述可以同時執行多個 Task。

## 4.1 Ask / Craft / Plan

目前官方 Task Bar 將核心互動模式區分為：

| Mode | 適合用途 | 是否直接改檔 |
| --- | --- | --- |
| **Ask** | 詢問、解釋、檢查、討論 | 原則上不直接修改檔案 |
| **Craft** | 直接完成工作 | 可以修改檔案、建立 artifact |
| **Plan** | 先產生方案，再由使用者確認執行 | 在 plan 被接受前暫緩實作 |

這不是單純 UI 選項，而是會改變 experiment 的重要變因。

同一 prompt：

- 在 Ask 可能只給分析；
- 在 Craft 會直接改 project；
- 在 Plan 會先把「想做什麼」外顯化，甚至讓使用者介入修正。

所以 benchmark README 應記錄 mode。

## 4.2 Workspace 與本機檔案

WorkBuddy 的一般工作單位很常是本機 folder，而不一定是 Git repository。

這使它很適合：

- 一堆 PDF + Excel + Word + 圖片；
- 研究資料；
- 本機專案資料夾；
- 多格式報告；
- 需要程式輔助處理、但最後 artifact 不是程式的任務。

這也是它和 coding-first agent 最大的文化差異之一。

## 4.3 多 Task 與 multi-agent 不應混為一談

官方文件能確認 WorkBuddy 支援 parallel tasks；騰訊對 Hy3 / WorkBuddy 的宣傳也提到 concurrent multi-agent。

研究上最好拆成兩件事：

- **parallel tasks**：使用者看得到的多個工作同時跑；
- **multi-agent orchestration**：一個複雜任務內部再委派其他 agent / specialist。

兩者都可能影響：

- elapsed time；
- output tokens；
- context allocation；
- filesystem contention；
- credit consumption；
- attribution。

因此應單獨紀錄。

---

# 5. 一張圖理解 WorkBuddy execution stack

下面是分析用的概念圖，不是騰訊官方 architecture diagram。

```text
使用者目標 / 本機檔案 / 外部服務內容
                  │
                  ▼
          Task + Workspace
          Ask / Craft / Plan
                  │
                  ▼
          權限與安全邊界
   Default Permissions / Full Access
                  │
                  ▼
          WorkBuddy Agent Harness
   planning · context · orchestration
       memory · action · governance
                  │
          ┌───────┴─────────┐
          ▼                 ▼
       Model Layer      Capability Layer
  Auto / Hy3 / other   Skills · Experts
  built-in / custom    MCP · Connectors
  API / Ollama         browser · scripts
          │            local files · shell
          └───────┬─────────┘
                  ▼
             External Systems
     filesystem · web · SaaS · IM · APIs
                  │
                  ▼
             Deliverables
  docs · sheets · slides · code · reports
                  │
          ┌───────┴─────────┐
          ▼                 ▼
       Memory           Automation / Assistant
  future personalization scheduled / remote work
```

這張圖的重點只有一個：

> **WorkBuddy 的模型只是整個執行系統其中一層。**

---

# 6. 權限模型：Default Permissions 與 Full Access

## 6.1 Default Permissions

Default Permissions 是比較接近一般安全預設的模式：

- workspace 內例行工作可正常進行；
- 風險較高的操作會停下來要求確認；
- 使用者應查看操作類型、範圍、理由再批准。

對一般人很合理，但對 benchmark 會引入 human-in-the-loop。

若每個 run 的批准次數不同，單純比較完成時間或成功率可能不公平。

## 6.2 Full Access

Full Access 會大幅降低逐步確認，允許更自由的：

- 寫檔／刪檔；
- 執行 scripts；
- 呼叫外部程式；
- 執行更完整的 autonomous workflow。

騰訊也明確建議只在可信、可復原、隔離的 workspace 使用。

## 6.3 Benchmark 應怎麼處理

最少要記：

1. 使用哪個 permission mode；
2. 有幾次 manual approval；
3. 是否有拒絕某個操作；
4. 是否因安全提示改變 prompt；
5. 是否給了 workspace 以外的 filesystem access。

若想測 agent 能力本身，可用 disposable workspace + version control / snapshot，再使用一致的權限 policy。

---

# 7. 模型層：WorkBuddy 不是 Hy3 的殼

WorkBuddy 是明確的 multi-model product。

## 7.1 Auto routing

Auto Mode 由平台依任務選模型。

優點：

- 一般使用者不用懂 model zoo；
- multimodal、reasoning、image 等需求可由平台調度。

缺點：

- benchmark attribution 很差；
- 同一個 prompt 在不同時間可能被路由到不同模型；
- 很難把結果寫成某一 model 的能力。

因此若研究的是特定 foundation model，應盡量關閉 Auto、明確選定模型。

## 7.2 明確選擇內建模型

WorkBuddy 可以暴露多個內建模型；可見模型會因：

- product version；
- account tier；
- promotion；
- region；
- service availability

而改變。

對 archive 應記錄「當時 UI 顯示的完整 model name」，而不是事後憑印象補寫。

## 7.3 Custom API model

WorkBuddy 支援把其他 API endpoint 加入模型層，並可設定 tool calling、vision、reasoning 等能力。

這點研究價值很高，因為它允許某種程度上的：

> **固定 WorkBuddy harness，替換 foundation model。**

也就是同一個 task 可以在相近 GUI、workspace、Skills、permission 架構下，用不同 provider 的 model 比較。

這比「不同平台 + 不同模型」的混合比較更能拆出 harness effect。

## 7.4 Ollama 本機模型

WorkBuddy 官方文件支援透過 OpenAI-compatible endpoint 連接 Ollama，常見 local endpoint 為 `localhost:11434`。

這提供：

- local inference；
- 自建模型；
- 某些隱私／內網情境；
- 避免外部 model API token 費用。

但要非常精確地說：

> **模型 inference 在本機 ≠ 整個 WorkBuddy task 完全離線。**

若同時開啟 web、MCP、Connector、Skills、Assistant、telemetry 等能力，資料仍可能離開裝置。

---

# 8. Hy3：為什麼它和 WorkBuddy 特別值得一起研究

Hy3 是 WorkBuddy 在 2026 年最重要的模型之一，而且騰訊很明確地把它和 Agent workload 綁在一起。

## 8.1 發展時間線

### 2026-04-24：Hy3 preview

騰訊公開並開源 Hy3 preview，定位為 hybrid fast/slow-thinking Mixture-of-Experts model。

官方 headline 規格：

- **295B total parameters**；
- **21B active parameters**；
- **最高 256K context**；
- 重點能力包含 reasoning、instruction following、context learning、coding、agent、inference efficiency。

騰訊同時強調其 pre-training / RL infrastructure 重建，以及「不是只追 benchmark，而是看 real-world usage」。

### 2026-07-06：正式 Hy3

正式版 Hy3 發布，騰訊強調相較 preview：

- 穩定性；
- 成本效率；
- coding agent；
- long-document understanding；
- multi-turn context；
- search QA；
- complex task execution。

並整合進 WorkBuddy、CodeBuddy、元寶、Marvis、ima 等產品，以及 Tencent Cloud TokenHub。

### 2026-07-24：TokenHub routing migration

騰訊雲公告 Hy3-preview Token Plan 轉向正式 Hy3，並清楚記錄三個 thinking mode：

- `no_think`：偏速度；
- `think_low`：較輕量推理；
- `think_high`：較深推理。

公告也直接指出正式模型吸收了 Yuanbao、WorkBuddy、ima、Marvis 等產品的真實回饋。

### 2026-08-05：全球可用性擴大

騰訊進一步宣佈 Hy3 global availability，WorkBuddy 是主要落地產品之一。

## 8.2 295B total / 21B active 到底是什麼意思

這個數字最容易被誤解。

Hy3 不是同時「既是 295B dense 又是 21B dense」。

MoE 的概念是：

- 整體模型有大量 expert parameters；
- 每個 token / computation step 只路由到其中一部分 expert；
- 因此**總容量很大，但每步實際啟用的參數較少**。

所以：

- 295B total：整個 model parameter capacity；
- 21B active：典型 inference step 被激活的參數規模。

但不能簡化成「運算量就是 21/295」。

仍要考慮：

- attention；
- routing；
- KV cache；
- memory movement；
- batch / parallelism；
- context length；
- serving implementation；
- hardware efficiency。

因此也不能只用這兩個 parameter 數字精準推算 agent 1M output tokens 的電力或碳排。

## 8.3 Fast / slow thinking

Hy3 的一個核心產品概念是 hybrid fast/slow thinking。

TokenHub 把它具體化成 `no_think` / `think_low` / `think_high`。

而放進 WorkBuddy 後還有另一層變因：

- Hy3 本身的 thinking depth；
- WorkBuddy 可能再做 model routing / orchestration。

所以若 UI 有顯示 thinking mode，benchmark 必須記錄；若 UI 隱藏，應寫「not exposed」，不要自行猜。

## 8.4 256K context 對 Agent 的實際意義

256K context 對以下 workload 很有吸引力：

- 大型 repository；
- 長規格文件；
- 多輪 tool traces；
- 長篇 PDF / docs；
- multi-step project history。

但 nominal context window 不等於「有效品質在 256K 都一樣」。

而且 WorkBuddy 可能使用：

- summarization / compaction；
- retrieval；
- re-read files；
- subagent delegation；
- product-side context management。

因此長任務最好記錄是否看到 context compaction / summary 行為。

## 8.5 Hy3 × WorkBuddy：model–harness co-design

這是 Hy3 最有研究價值的地方之一。

騰訊官方明確說 final Hy3 有吸收 WorkBuddy 等真實產品的 feedback，並針對 coding agent、長文件、多輪 context、complex task execution 改善。

分析上可以合理說：

> Hy3 並不是在真空中訓練後才隨便塞進 WorkBuddy；模型與產品 workload 存在明顯的共同優化關係。

這可能讓 Hy3 更適應：

- WorkBuddy tool schema；
- task decomposition；
- file/artifact workflow；
- error recovery；
- office/general work patterns。

但這只是合理的 model–harness co-design 解讀，不代表每個內部 schema 都被公開證實。

## 8.6 騰訊公布的 WorkBuddy 內部指標

騰訊宣稱 Hy3 在 WorkBuddy 內部 workplace evaluation：

- task success **> 90%**；
- 平均 task completion time 相較上一代 Hy 模型 **降低 34%**。

這應標為**vendor internal metric**。

不能拿來直接說 Hy3 勝過 GPT / Claude / Gemini，因為公開材料沒有提供足以做公平跨廠商比較的完整方法。

## 8.7 20+ Skills 與 100+ Skills 的官方數字矛盾

WorkBuddy 不同官方來源曾分別出現：

- 20+ skill packages；
- 100+ built-in skills。

最穩妥的處理不是選一個數字硬說誰對，而是承認：

- 產品在高速更新；或
- 不同頁面對「Skill」的 counting scope 不同。

對 benchmark 最可靠的是紀錄 run 當時實際可見與實際啟用的 Skills。

## 8.8 研究 Hy3 最值得做的四象限

若未來資源允許，最有意義的不是只跑「WorkBuddy score」，而是：

| | WorkBuddy harness | 其他 harness |
| --- | --- | --- |
| **Hy3** | 測騰訊 vertically integrated path | 測 Hy3 離開 WorkBuddy 後的能力 |
| **其他模型** | 測 WorkBuddy harness value | 對照其他完整 agent stack |

這種設計能部分拆開「模型能力」與「harness/orchestration 能力」。

---

# 9. Skills：把能力與 workflow 打包

WorkBuddy Skills 是可重用的 capability / workflow package。

官方文件描述的場景包含：

- 文件處理；
- data/reporting；
- design；
- file handling；
- 其他工作型流程。

WorkBuddy 還支援：

- Skill marketplace；
- community Skills；
- 匯入相容套件；
- 從自然語言建立 custom Skill；
- enable / disable / update / uninstall。

從 benchmark 角度，Skill 不是「UI 皮膚」。

它可能引入：

- 額外 instructions；
- tool usage；
- domain procedure；
- templates；
- artifact conventions。

所以 Skill 是實驗環境的一部分。

若想做 clean run，應明確記錄：

- 僅 default Skills；或
- 啟用了哪些 custom / marketplace Skills。

---

# 10. MCP：通用工具與資料邊界

WorkBuddy 有 UI-integrated MCP 支援。

公開文件涵蓋：

- 新增 MCP server；
- URL / auth config；
- OAuth；
- token refresh / reconnect；
- individual server/tool enable/disable；
- local / project-scoped config；
- Tencent Cloud MCP marketplace。

這讓 WorkBuddy 不需內建所有企業系統，就能把外部 tool/data 接到 Agent。

對 benchmark 而言，每個 MCP server 都應視為 dependency：

- server 名稱；
- version；
- enabled tools；
- 是否 reachable；
- authentication scope。

否則同樣 prompt 可能因工具 availability 不同而完全不同。

---

# 11. Connectors：產品化的 SaaS 整合

目前官方 Connector 頁面列出的服務包括：

- GitHub；
- GitLab；
- Jira；
- Confluence；
- Google Drive；
- Gmail；
- Notion；
- Slack。

Connectors 和 MCP 概念不同：

- **Connector**：WorkBuddy 對常見服務提供的 productized integration；
- **MCP**：通用 protocol boundary，可接更廣泛工具／資料源。

對企業／隱私評估來說兩者都很重要，因為它們都可能把 task data 帶出本機。

---

# 12. Experts：不是換模型，而是換「角色配置」

Expert Center 提供寫作、分析、開發、設計、商務等 domain-specialized agent configuration。

Expert 比較接近：

- specialized system prompt；
- domain persona；
- 預先調整的 context / procedure。

它不必代表 foundation model 改變。

因此 benchmark 如果選了 Expert，就應把它列成 configuration，而不能只寫 model name。

---

# 13. Explore：把 Prompt + Skill + Expert 變成可重用配方

Explore 是 WorkBuddy 的 curated/community recipe layer。

使用者可以看別人的成品／流程，選擇類似 **Make My Version**，再載入：

- prompt；
- Skill；
- Expert。

可以用三句話理解：

- Skill = Agent **能做什麼**；
- Expert = **誰／哪種專業角色**來做；
- Explore = 這些東西組合後，**可以做出什麼案例**。

Explore 很適合產品 onboarding，但對 zero-shot benchmark 會引入 pre-tuned context，所以必須記錄。

---

# 14. Automation：排程任務

WorkBuddy Automation 支援：

- hourly；
- daily；
- weekly；
- one-time；
- workspace 選擇；
- templates；
- completion notification。

需要注意：desktop Automation 和 7×24 cloud agent 不應混為一談。

WorkBuddy Enterprise 的 WMA 才是更明確的 managed cloud runtime 路線。

方案不同時可建立的 automation 數量也可能不同，所以 account tier 是實驗 metadata。

---

# 15. Assistant：用通訊軟體遠端下任務給本機 WorkBuddy

WorkBuddy Assistant 支援透過多種 messaging platform 把任務送到正在執行 WorkBuddy 的電腦。

官方列出的平台包含：

- Slack；
- Telegram；
- Discord；
- 企業微信；
- 飛書；
- 釘釘；
- QQ；
- 元寶派；
- WeChat AssistantBot。

典型流程：

1. 手機／聊天平台送訊息；
2. 本機 WorkBuddy 收到；
3. 使用本機 workspace / context 執行；
4. 結果再回傳到 messaging channel。

官方也明確說：電腦需保持開機、WorkBuddy 正在運行，而且要有穩定網路。

這和 vendor-hosted cloud agent 有本質差異：

- Assistant = remote control / dispatch 到本機；
- WMA = 獨立 cloud runtime。

---

# 16. Memory：好用，但對 benchmark 是重大 confounder

WorkBuddy 官方文件目前描述 Memory 預設開啟。

Memory 會從對話中整理：

- 相關 context；
- preferences；
- habits；
- 長期可重用資訊。

官方還描述：

- memory summary 會定期重新整理；
- 使用者可看、改、刪；
- 可以要求「記住」或「忘記」；
- 可從其他 AI service 匯入使用習慣 summary。

## 16.1 對實驗的影響

若 Memory 開啟，即使兩個 visible prompt 一樣，結果也可能因先前 unrelated conversation 不同而改變。

所以可以有兩種合法研究設計：

### Clean-memory policy

- 關閉／清除 Memory；
- 把 run 當 controlled benchmark。

### Naturalistic-product policy

- 保留 Memory；
- 把 run 當「真實個人化產品體驗」；
- 不宣稱是 clean foundation-model comparison。

最糟糕的是 Memory 開著卻不記錄。

---

# 17. Privacy：本機 workspace 不等於資料不離開本機

這一點非常重要。

WorkBuddy Privacy Policy 對 Inputs 的定義很廣，包含：

- prompts；
- text；
- files；
- audio；
- commands；
- chat/coding/agentic-session instructions；
- scheduled tasks；
- integrated-app content。

Outputs 則包含 response 和 actions。

## 17.1 第三方模型

政策指出 WorkBuddy 會使用不同第三方 LLM，使用者 Input 可能交由所選模型提供者進行 inference。

如果使用第三方：

- Skills；
- MCP；
- Connectors；
- messaging channels；
- external applications

相關內容也可能依使用者指令流向那些服務。

因此實際 data path 是：

> **model + tools + integrations 的整張 graph，而不是只看 WorkBuddy App 本身。**

## 17.2 Model training default

2026-07 的國際版 Privacy Policy 表示 Inputs / Outputs **預設不拿去做 AI model training**；BYO API model 也不會拿這些內容做 WorkBuddy model training。

這是 policy guarantee，不是 cryptographic isolation；企業仍需另外檢查所選 model provider 與第三方 integration 的條款。

## 17.3 Configuration data

政策還區分 configuration information，例如：

- model-selection preference；
- third-party account bindings；
- device bindings；
- Skills；
- messaging config；
- MCP config；
- automation settings；
- general settings。

官方說明這類 configuration 以本機 storage 為主，和會送去 remote inference 的 Inputs / Outputs 是不同類型。

## 17.4 Storage / retention

2026-07 國際政策的公開 snapshot 包含：

- Inputs / Outputs：service side 最長約 **14 days**，本機 copy 由使用者自行刪除；
- 某些 account / diagnostic / feedback data 依帳戶與刪除窗口保存；
- billing/payment 類資料可保存更久；
- configuration information 則依前述本機處理邏輯。

政策也指出國際服務資料儲存在 **Singapore**，全球 support/engineering 團隊可能依政策目的存取，包括中國境內團隊。

## 17.5 最精確的結論

不要寫：

> WorkBuddy 是本機 Agent，所以資料都留在電腦。

比較正確的是：

> WorkBuddy 可以直接操作本機 workspace；但 inference 與 connected-tool data 是否離開裝置，取決於模型、Skills、MCP、Connectors、messaging channels 與其他設定。使用 Ollama 可以把 LLM inference path 留在本機，但若要達到真正 local-only / air-gapped workflow，仍需控制整個 capability graph。

---

# 18. WorkBuddy Enterprise 與 WMA：企業治理層

企業環境多了 consumer benchmark 不會處理的東西：

- organization identity；
- role-based permissions；
- enterprise asset boundaries；
- centrally managed models；
- auditability；
- content safety；
- Connector / MCP policy；
- data residency；
- shared SaaS / dedicated / private deployment；
- WMA cloud-managed agents。

因此同樣「WorkBuddy + Hy3」，企業帳號和 Free consumer account 可能是完全不同的 execution environment。

對 archive 應把 account tier / enterprise status / region 寫進 run metadata。

---

# 19. 價格與平台支援：2026-08-24 snapshot

> [!WARNING]
> 以下價格和 promotion 是時間敏感資訊，只用來描述研究日期附近的產品狀態，不應當成永久方案。

國際版 WorkBuddy pricing 當時公開：

| Plan | 價格 | 公開基礎 Credits | Automation |
| --- | ---: | ---: | ---: |
| Free | US$0 | 100 base | standard 3，promotion 可能擴大 |
| Pro | US$10/月或 US$96/年 | 1,000 base + 1,000 bonus | standard 15，promotion 可能擴大 |
| Team | US$40/seat/月或 US$480/seat/年 | 1,000/seat shared pool | 含 team/admin/billing 能力 |

中國市場的 WorkBuddy Enterprise / Buddy AI 方案採不同 RMB 計價與 shared credits，因此**region 不能省略**。

## 19.1 OS

官方安裝文件在研究日期可確認：

- Windows 10 1809+ / Windows 11；
- x64 / ARM64；
- macOS 12+；
- Apple Silicon / Intel；
- 4 GB minimum / 8 GB recommended memory（依當時文件）；
- 正常 cloud-backed workflow 需要網路。

不同區域的登入方式也不同，例如 global OAuth 與中國服務的帳號流程。

---

# 20. WorkBuddy Bench：名稱像 WorkBuddy，但不是「桌面版 WorkBuddy benchmark」

Tencent 維護公開 repository `Tencent/workbuddy-bench`。

它的定位是**真實工作任務導向的 Agent benchmark / evaluation framework**。

官方目前分四個 subset：

| Subset | Tasks | 重點 |
| --- | ---: | --- |
| Code | 80 | repo-level software engineering |
| Web | 70 | frontend / GUI |
| Office | 50 | mixed office-file / data workflow |
| Security | 60 | security / vulnerability work |

框架把 agent CLI / harness 放進 local Docker sandbox，執行 task，捕捉：

- patch；
- trajectory；
- test result；
- efficiency；
- 最終 artifact / report。

它建立在 Harbor framework 之上。

## 20.1 Model 和 harness 是分開設定的

這一點對研究非常重要。

WorkBuddy Bench 的 config 把：

- model；
- harness；
- dataset

分開。

官方 README 的 model config example 使用 Hy3，但 harness 可以另外指定。

所以一個 WorkBuddy Bench score 不等於：

- WorkBuddy desktop 的分數；
- Hy3 純模型分數；
- consumer WorkBuddy 的 UX 分數。

必須同時報 model + harness + dataset version + evaluator。

## 20.2 為什麼它和本 archive 很搭

兩者的共同哲學是：

- 不只測短題；
- 測長時間工作；
- 看實體 artifact；
- 看是否真的通過 verification；
- 把 agent 當完整執行系統。

因此 WorkBuddy Bench 對未來本 archive 的 methodology 很有參考價值。

---

# 21. WorkBuddy 和其他 2026 Agent 平台：先看總表

以下比較的是 **execution surface**，不是宣稱哪個 foundation model 最聰明。

| 維度 | WorkBuddy | Google Antigravity | OpenAI Codex | Claude Code | Grok Build | Pi |
| --- | --- | --- | --- | --- | --- | --- |
| 核心身份 | 通用 workplace desktop agent | agent-first development platform | coding-first、快速擴向 knowledge work | coding-first agent engine | coding harness + broader Build ecosystem | 極簡 terminal coding harness |
| 主要 UX | Desktop tasks / artifacts | Desktop + CLI + SDK + IDE | App + CLI + IDE + cloud | CLI + IDE + desktop + web | Terminal TUI；另有 web/mobile Build | Terminal |
| 本機檔案 | 強 | 強 | 強 | 強 | 強 | 強 |
| Cloud runtime | WMA 為獨立 enterprise path | managed agent/API | Codex cloud | web/Routines | workflow/background + xAI surfaces | 由使用者自行整合 |
| Parallelism | parallel tasks / multi-agent claims | dynamic subagents / worktrees | parallel agents / worktrees | sessions / subagents / teams | 高 fan-out workflows | 核心不內建 |
| Model freedom | **高：built-in/custom API/Ollama** | 主要 Gemini ecosystem | OpenAI ecosystem | Claude-centric | xAI/Grok 為主，harness 更開放 | **非常高** |
| MCP | built in | built in | app/plugin/tool ecosystem | first-class | first-class | 刻意不放核心，可擴充 |
| Extensions | Skills/Experts/Explore | Skills/MCP/hooks | Skills/plugins | Skills/plugins/hooks | Skills/plugins/hooks | extensions/packages/templates |
| Office artifact | **核心 use case** | 仍偏 development-first | 2026 明顯擴張 | 可做，但 identity 仍是 SWE | broader xAI 可做，terminal harness 偏 coding | 取決於使用者自己搭 |
| Scheduling | Automation | Scheduled Tasks | Automations | local schedule + cloud Routines | workflows/automations | 外部工具 |
| Remote dispatch | 多種 IM Assistant | browser Remote Control | app/cloud ecosystem | Remote Control/Dispatch/Slack | Grok web/mobile | 自行整合 |
| Harness openness | desktop proprietary | proprietary | proprietary | proprietary/extensible | terminal harness open source | open source / minimal |
| 最適合 | general work + file/tool orchestration | multi-agent developer workflow | OpenAI-centric software/knowledge work | deep SWE + composability | parallel coding / harness transparency | power user control / research |

---

# 22. WorkBuddy vs Google Antigravity

2026 的 Antigravity 已不是簡單「Google 的 AI IDE」。Antigravity 2.0 是 standalone desktop app，另外有 CLI、SDK、IDE surface。

## 22.1 兩者共同方向

兩邊都在往：

- graphical agent command center；
- local project context；
- multiple agents/tasks；
- permission controls；
- Skills / MCP；
- scheduled work；
- remote monitoring；
- artifact-centric workflow

前進。

## 22.2 Antigravity 在 software project isolation 更原生

Antigravity Projects / Git worktree 是其 development design 的一部分。

多 agent 可在 isolation worktree 中處理不同修改，對大型 repo benchmark 很乾淨。

WorkBuddy 雖能寫程式，但 workspace 更 general-purpose，並沒有把 Git worktree isolation 當整個產品的核心 mental model。

所以：

- multi-repo / multi-agent coding → Antigravity 更自然；
- PDF + Excel + slide + email + research + scripts → WorkBuddy 更自然。

## 22.3 Model ecosystem

Antigravity 的 first-party vertical integration 主要圍繞 Gemini。

WorkBuddy 的 built-in + custom API + Ollama，對「固定 harness 換模型」研究更友善。

## 22.4 Remote control

Antigravity 的方向偏 browser-based Remote Control；WorkBuddy Assistant 則直接深度利用 workplace IM。

一個比較像遠端 command center，一個比較像把 Agent 接進日常聊天入口。

---

# 23. WorkBuddy vs OpenAI Codex

到 2026 年，說 Codex「只會 coding」已經不準確。

OpenAI 已把 Codex 推向：

- reports；
- spreadsheets；
- presentations；
- contracts；
- research；
- data analysis；
- workflow automation；
- lightweight tools。

因此它和 WorkBuddy 的產品範圍正在靠近。

## 23.1 歷史 design center 不同

Codex 的 DNA 仍是：

- Git repository；
- diff；
- code review；
- CLI/IDE；
- isolated worktree；
- cloud coding agent。

它是從「code 是通用工作機制」向外擴張 knowledge work。

WorkBuddy 則從「人有一個 workplace goal」出發，coding 是可用工具之一。

## 23.2 Model freedom

這裡 WorkBuddy 的研究彈性較大：

- Hy3；
- 其他 built-in model；
- custom API；
- local Ollama。

Codex 則是 OpenAI vertically integrated stack。

因此：

- 想測 OpenAI 端到端產品 → Codex 很合理；
- 想測同一 GUI/harness 換 provider → WorkBuddy 更方便。

## 23.3 最有趣的比較問題

Codex 問的是：

> coding-first Agent 可以多遠地擴張到 general knowledge work？

WorkBuddy 問的是：

> workplace-first Agent 可以多深地做 rigorous software engineering？

用同一份高難度 spec 跑兩邊，研究價值很高。

---

# 24. WorkBuddy vs Anthropic Claude Code

Claude Code 也早已不是純 terminal experience；Anthropic 現在提供 terminal、IDE、desktop、web，並有 Remote Control、Dispatch、Slack、scheduled tasks、MCP、Skills、Hooks、subagents、plugins、agent teams 等。

## 24.1 Claude Code 的核心仍是 software engineering

其最典型 abstraction 仍圍繞：

- `CLAUDE.md`；
- repo context；
- shell；
- code edits；
- Git；
- CI/CD；
- subagents；
- hooks。

WorkBuddy 的詞彙則是：

- workplace tasks；
- files；
- artifacts；
- Skills；
- Experts；
- Explore；
- Connectors；
- Assistant。

## 24.2 Extensibility 哲學

Claude Code 的 developer composability 很深：

- persistent instructions；
- Skills；
- MCP；
- subagents；
- teams；
- Hooks；
- Plugins；
- Agent SDK；
- CI integrations。

WorkBuddy 則把很多能力包成 GUI product feature，讓非技術使用者也能操作。

可以概括成：

- Claude Code：programmable developer agent；
- WorkBuddy：integrated generalist agent workstation。

## 24.3 Scheduling / remote

Claude Code 有 local scheduled tasks 與 cloud Routines；WorkBuddy 則有 desktop Automation + WMA cloud path。

Remote 方面 Claude Code 有 Remote Control / Dispatch / Slack；WorkBuddy 在亞洲 workplace IM 的 out-of-box breadth 更大。

---

# 25. WorkBuddy vs SpaceXAI / xAI Grok Build

Grok Build 需要拆兩層看：

1. terminal coding agent / harness；
2. 後來的 web/mobile Build experience。

若比較 Agent framework，terminal harness 才是更直接對手。

## 25.1 Grok Build terminal 的特色

官方公開能力包含：

- plan / review / approve；
- diff；
- parallel specialized subagents；
- Git worktrees；
- headless `-p`；
- Agent Client Protocol；
- Skills；
- plugins；
- Hooks；
- MCP。

而且 2026-07 之後 terminal harness / TUI 已 open source。

這給研究者很大的觀測優勢：可以直接讀 agent loop、tool dispatch、context assembly 等實作。

WorkBuddy desktop 則是 proprietary surface。

## 25.2 高 fan-out workflow

Grok Build Workflows 公開描述可把大型任務 fan-out 給大量 agents，default budget 128、特大型可到 1,024。

WorkBuddy 公開 desktop 文檔目前沒有同等清楚的大規模 fan-out contract。

若研究 100+ agent parallel code review / research，Grok Build 很特殊。

## 25.3 WorkBuddy 的強項

WorkBuddy 在 ordinary workplace integration 更完整：

- Office artifacts；
- Connectors；
- Experts；
- Explore；
- IM Assistant；
- multi-model UI。

所以兩者的強項中心不同。

---

# 26. WorkBuddy vs Pi coding agent

Pi 幾乎可以看成 WorkBuddy 的哲學反面。

Pi 的核心思想是：

> 保持 harness 很小，使用者自己決定工作流。

## 26.1 Pi 刻意不內建很多東西

Pi 官方設計刻意不把以下能力做成 core：

- MCP；
- subagents；
- permission popup；
- plan mode；
- todo；
- background bash。

不是因為做不到，而是希望使用者透過：

- TypeScript Extensions；
- Skills；
- prompt templates；
- Pi Packages；
- containers / tmux；
- external tools

自行組合。

WorkBuddy 則反過來，把這些類型能力大量產品化。

## 26.2 Model freedom

兩邊都很強，但 user persona 不同：

- WorkBuddy：GUI 加模型、custom API、Ollama；
- Pi：面向 power user 的 multi-provider runtime / SDK / RPC。

Pi 更 hackable；WorkBuddy 更 accessible。

## 26.3 Security philosophy

Pi 不在 core 強制 permission UI，安全架構更多由使用者的 container / extension / environment 決定。

WorkBuddy 內建 Default Permissions / Full Access，更適合 mainstream user，但較少 source-level control。

## 26.4 對研究最漂亮的實驗

如果能讓**同一 model、相近 API parameters**分別跑：

- Pi；
- WorkBuddy

就能相對直接地觀察 harness complexity / orchestration 對成功率、token、時間的影響。

---

# 27. 怎麼選平台：不是「誰最好」，而是哪個 execution surface 合適

## 選 WorkBuddy，如果你要：

- 研究 + Office + files + scripts + SaaS 混合；
- 非工程師也能使用；
- GUI 內直接換模型；
- custom API / Ollama；
- workplace Connectors；
- messaging remote dispatch；
- batteries-included Agent workstation。

## 選 Antigravity，如果你要：

- 開發工作為主；
- native Git worktrees / Projects；
- dynamic subagents；
- Google/Gemini agent ecosystem。

## 選 Codex，如果你要：

- Git / SWE 是中心；
- 同時想要迅速成長中的 knowledge-work support；
- OpenAI vertically integrated stack；
- parallel agents / app / cloud / CLI / IDE。

## 選 Claude Code，如果你要：

- deep repository work；
- programmable developer automation；
- MCP + Hooks + Skills + subagents + plugins；
- Claude model ecosystem。

## 選 Grok Build，如果你要：

- open-source coding harness；
- 大規模 parallel fan-out；
- worktrees；
- xAI/Grok-native workflow；
- source-level harness inspection。

## 選 Pi，如果你要：

- 最小 core；
- 最大程度自己控制；
- multi-provider；
- 自己搭 permissions / subagents / MCP / orchestration；
- research / hacking flexibility。

---

# 28. WorkBuddy 的主要優勢

## 28.1 從一開始就不是只為 Git repository 設計

一個資料夾裡放 PDF、Excel、Word、PPT、圖片、CSV，對 WorkBuddy 是正常 workspace，而不是 coding agent 的 edge case。

## 28.2 模型選擇彈性高

Built-in / Auto / custom API / Ollama 讓它成為少數對一般使用者也容易做 cross-model experiment 的 graphical agent surface。

## 28.3 功能生態完整

Skills、MCP、Connectors、Experts、Explore、Automation、Assistant、Memory 都是同一產品裡可見的 first-class concept。

## 28.4 很「工作場所」的 remote control

透過企業微信、飛書、釘釘、Slack、Telegram、Discord 等 IM 直接 dispatch，符合很多亞洲組織真正的工作入口。

## 28.5 Hy3 的垂直整合

模型本身就從 WorkBuddy 等 real product feedback 改善，這讓 Hy3 + WorkBuddy 是非常值得研究的 vertically integrated stack。

## 28.6 從個人 desktop 到 enterprise runtime 有明確產品路線

WorkBuddy Enterprise + WMA 提供治理、權限、audit、cloud runtime 的延伸。

---

# 29. 目前限制與成熟度風險

## 29.1 產品變得太快

同一批官方頁面已可看到不同 Skill 數量與不同 vocabulary。半年後本文某些 UI 細節可能過時。

## 29.2 WorkBuddy / CodeBuddy 文件邊界還會混淆

共享 portal 和歷史 `.codebuddy` 命名容易讓研究者誤把 sibling-product 行為當 WorkBuddy contract。

## 29.3 Vendor metrics 尚缺完整第三方 replication

Hy3 / WorkBuddy 內部成功率與速度提升有參考價值，但不是 independent evidence。

## 29.4 Auto routing 破壞 model attribution

若研究 model，Auto 最好關閉；若研究產品，Auto 則可以保留，但結果標籤應是「WorkBuddy Auto」。

## 29.5 Memory 預設個人化會破壞 clean-room reproducibility

不處理 Memory，兩個帳號的結果可能天生不同。

## 29.6 能力越多，資料邊界越複雜

模型、Skills、MCP、Connectors、messaging 都可能是不同 data processor。

## 29.7 Desktop harness 是 closed source

不像 Pi / open-source Grok Build，可直接讀完整 agent loop；研究者只能從產品邊界、export、artifact、logs 反推 observable behavior。

---

# 30. 對本 archive：WorkBuddy run 最低應記錄哪些 metadata

一份有研究價值的 WorkBuddy run README，至少應有：

## Product identity

- WorkBuddy version / build；
- OS；
- region：Global / China；
- account tier；
- run date / timezone。

## Model

- model display name；
- model ID/version（若可見）；
- Auto on/off；
- reasoning / thinking level；
- custom API provider / endpoint type；
- Ollama model/version（若使用）。

## Agent configuration

- Ask / Craft / Plan；
- Default Permissions / Full Access；
- workspace policy；
- enabled Skills；
- Expert；
- Explore recipe；
- MCP servers / tools；
- Connectors；
- Memory on/off/cleared；
- other concurrent tasks / subagents。

## Human intervention

- permission approvals；
- plan approval / edits；
- retry messages；
- 人工修改 generated files；
- restart / reconnect；
- 中途換模型。

## Evidence

- 完整 conversation export；
- filtered model-output corpus；
- output-extraction policy；
- final project snapshot；
- build/test logs；
- independent review；
- visible credits / usage / timing。

---

# 31. 建議的 WorkBuddy benchmark protocol

## Phase A — Freeze environment

1. 記 WorkBuddy version、OS、region、tier。
2. 記當時可選模型。
3. 明確選一個 model；除非就是測 Auto，否則關 Auto。
4. Hy3 如果能選 thinking level，記下來。
5. Clean benchmark 時關閉／清除 Memory。
6. 關閉不必要 Connectors、MCP、Experts、Explore、custom Skills。
7. 用獨立 disposable workspace。
8. 建立 Git / filesystem snapshot。
9. 記 permission mode。

## Phase B — Run

1. 投入固定 spec，不偷偷簡化。
2. 允許 Agent 在既定 mode 下自行 plan / execute。
3. 每次 human intervention 都記錄。
4. primary run 期間不要人工改 generated file。
5. 記 timestamps、credits、model switches、agent/subagent activity。

## Phase C — Evidence capture

1. Export 完整 conversation。
2. 生成 model-output-only corpus。
3. **Repository source 用 Repomix 另算，chat 裡的 Write/Edit file bodies 不重複計入。**
4. Archive final project，排除不必要 build/cache artifacts。
5. 在 WorkBuddy 外部做 independent build/test/review。
6. README 同時寫成功與失敗。

## Phase D — Interpretation

最後至少分開回答四題：

1. **最終 project 是否滿足 spec？**
2. **Agent 產生多少 output / compute proxy？**
3. **需要多少 human intervention？**
4. **結果多少來自 model，多少可能來自 WorkBuddy harness/tooling？**

不要把一個 end-to-end product result 簡化成「model IQ」。

---

# 32. 對本 repository 最推薦的 run label

不要只寫：

> WorkBuddy + Hy3

比較有研究價值的格式是：

> **Tencent WorkBuddy Desktop · vX.Y · Hy3 `think_high` · Craft · Default Permissions · Memory Off · no custom Skills/MCP/Connectors · Pro · Global · Windows 11**

如果某欄不可見，就寫 `not exposed`，不要猜。

這個格式承認現代 Agent 系統其實是：

> **foundation model + harness + tools + permissions + memory + UI + services + environment**

---

# 33. 名詞速查

**Agent harness**  
負責 context assembly、model calls、tool dispatch、task state、orchestration、memory/control 的中間執行層。

**Artifact / Deliverable**  
可離開 conversation 單獨檢查的成果，如文件、表格、簡報、report、code changes。

**Auto routing**  
由 WorkBuddy 依任務自動選 model；方便但降低 model attribution。

**Connector**  
對 GitHub、Gmail、Jira、Slack 等已知服務的產品化整合。

**Expert**  
domain-specialized persona / agent configuration，不必代表模型不同。

**Explore**  
把 prompt、Skill、Expert 組成可重用案例／recipe 的入口。

**Hy3**  
騰訊 2026 年 hybrid fast/slow-thinking MoE；295B total、21B active、最高 256K context。

**MCP**  
Model Context Protocol，讓 Agent 存取外部 tools / data 的開放協定。

**MoE**  
Mixture of Experts；透過 routing 只啟用部分 expert parameters 的模型架構。

**Skill**  
可重用的 instructions / capability / workflow package。

**WMA**  
WorkBuddy Managed Agents，WorkBuddy Enterprise 旗下的 managed cloud agent-runtime product。

**Workspace**  
WorkBuddy Task 的主要本機 folder / context boundary。

---

# 34. 第一手來源

以下來源是英文版研究時於 **2026-08-24** 檢查的第一手資料。中文版重新編排內容，但不另行把未證實資訊補成事實。

## 34.1 WorkBuddy / Tencent

### Product / architecture

- Tencent Cloud — **WorkBuddy Enterprise — Product Overview**  
  <https://cloud.tencent.com/document/product/1831/134329>
- Tencent Cloud — **WorkBuddy — Product Introduction**  
  <https://cloud.tencent.com/document/product/1831/134384>
- Tencent Cloud — **WorkBuddy Enterprise — Product Advantages**  
  <https://cloud.tencent.com/document/product/1831/134330>
- Tencent Cloud — **WorkBuddy Managed Agents — Product Introduction**  
  <https://cloud.tencent.com/document/product/1831/134407>
- Tencent WorkBuddy Docs — **Task Bar / Ask, Craft, Plan, Skills, permissions, parallel tasks**  
  <https://www.workbuddy.ai/docs/zh/workbuddy/From-Beginner-to-Expert-Guide/Function-Description/Task-Bar>
- Tencent WorkBuddy Docs — **Permission Modes**  
  <https://www.workbuddy.ai/docs/workbuddy/From-Beginner-to-Expert-Guide/Function-Description/Permission-Modes>
- Tencent WorkBuddy Docs — **Model Configuration**  
  <https://www.workbuddy.ai/docs/workbuddy/From-Beginner-to-Expert-Guide/Function-Description/Model>

### Skills / MCP / Connectors / workflow

- Tencent WorkBuddy Docs — **MCP Integration**  
  <https://www.workbuddy.ai/docs/workbuddy/From-Beginner-to-Expert-Guide/Function-Description/MCP-Guide>
- Tencent WorkBuddy Docs — **Connectors**  
  <https://www.workbuddy.ai/docs/workbuddy/From-Beginner-to-Expert-Guide/Function-Description/Connector>
- Tencent WorkBuddy Docs — **Expert Center**  
  <https://www.workbuddy.ai/docs/workbuddy/From-Beginner-to-Expert-Guide/Function-Description/Expert-Center>
- Tencent WorkBuddy Docs — **Explore**  
  <https://www.workbuddy.ai/docs/workbuddy/From-Beginner-to-Expert-Guide/Function-Description/Explore>
- Tencent WorkBuddy Docs — **Automation**  
  <https://www.workbuddy.ai/docs/workbuddy/From-Beginner-to-Expert-Guide/Function-Description/Automation-Guide>
- Tencent WorkBuddy Docs — **Assistant Remote Control**  
  <https://www.workbuddy.ai/docs/workbuddy/From-Beginner-to-Expert-Guide/Function-Description/Assistant>
- Tencent WorkBuddy Docs — **Memory**  
  <https://www.workbuddy.ai/docs/workbuddy/From-Beginner-to-Expert-Guide/Function-Description/Memory>

### Privacy / plans / installation

- Tencent — **Tencent WorkBuddy Privacy Policy**（2026-07 版本）  
  <https://www.workbuddy.ai/document/privacy-policy>
- Tencent — **Tencent WorkBuddy Service Agreement**  
  <https://www.workbuddy.ai/document/term>
- Tencent WorkBuddy Docs — **Pricing**  
  <https://www.workbuddy.ai/docs/workbuddy/pricing>
- Tencent WorkBuddy Docs — **Windows Installation Guide**  
  <https://www.workbuddy.ai/docs/workbuddy/From-Beginner-to-Expert-Guide/Installation-Win-Guide>
- Tencent WorkBuddy Docs — **Mac Installation Guide**  
  <https://www.workbuddy.ai/docs/workbuddy/From-Beginner-to-Expert-Guide/Installation-Mac-Guide>

### Hy3

- Tencent — **Tencent Unveils Hy3 preview; Model Enhances Agent Capabilities and Real-World Usability**（2026-04-24）  
  <https://www.tencent.com/tencent-unveils-hy3-preview-model-enhances-agent-capabilities-and-real-world-usability/>
- Tencent — **Tencent Hunyuan Officially Releases Hy3, Advancing Agent Capabilities and Deeper Product Integration**（2026-07-06）  
  <https://www.tencent.com/tencent-hunyuan-officially-releases-hy3-advancing-agent-capabilities-and-deeper-product-integration/>
- Tencent Cloud — **Hy3 preview Token Plan routing migration to Hy3**  
  <https://cloud.tencent.com/announce/detail/2384>
- Tencent — **Tencent Hy3 Now Available Globally, Extending Practical AI Across Products, Workflows and Cloud Services**（2026-08-05）  
  <https://www.tencent.com/tencent-hy3-now-available-globally-extending-practical-ai-across-products-workflows-and-cloud-services/>
- Tencent Cloud — **Hunyuan / Hy3 product specifications**  
  <https://cloud.tencent.com/product/tclm>

### WorkBuddy Bench

- Tencent — **WorkBuddy Bench official repository**  
  <https://github.com/Tencent/workbuddy-bench>

## 34.2 Google Antigravity

- **Introducing Google Antigravity 2.0**（2026-05-19）  
  <https://www.antigravity.google/blog/introducing-google-antigravity-2>
- **Feature Overview**  
  <https://antigravity.google/docs/features>
- **Documentation Home / surfaces**  
  <https://antigravity.google/docs/home>
- **Subagents, Hooks, Scheduled Tasks, Agent Management, Voice, and Much More**（2026-05-19）  
  <https://antigravity.google/blog/google-io-2026-feature-deep-dive>
- Google Developers Blog — **Transitioning Gemini CLI to Antigravity CLI**  
  <https://developers.googleblog.com/en/an-important-update-transitioning-gemini-cli-to-antigravity-cli/>

## 34.3 OpenAI Codex

- OpenAI — **Introducing the Codex app**（2026-02-02）  
  <https://openai.com/index/introducing-the-codex-app/>
- OpenAI — **Codex for every role, tool, and workflow**（2026-06-02）  
  <https://openai.com/index/codex-for-every-role-tool-workflow/>
- OpenAI — **Codex is becoming a productivity tool for everyone**（2026-06-02）  
  <https://openai.com/index/codex-for-knowledge-work/>
- OpenAI — **How agents are transforming work**  
  <https://openai.com/index/how-agents-are-transforming-work/>
- OpenAI — **Running Codex safely at OpenAI**（2026-05-08）  
  <https://openai.com/index/running-codex-safely-at-openai/>

## 34.4 Anthropic Claude Code

- Anthropic — **Claude Code overview**  
  <https://code.claude.com/docs/en/overview>
- Anthropic — **Extend Claude Code**  
  <https://code.claude.com/docs/en/features-overview>
- Anthropic — **Use Claude Code Desktop**  
  <https://code.claude.com/docs/en/desktop>
- Anthropic — **Schedule recurring tasks in Claude Code Desktop**  
  <https://code.claude.com/docs/en/desktop-scheduled-tasks>
- Anthropic — **Automate work with Routines**  
  <https://code.claude.com/docs/en/routines>

## 34.5 SpaceXAI / xAI Grok Build

- SpaceXAI — **Introducing Grok Build**（2026-05-25）  
  <https://x.ai/news/grok-build-cli>
- SpaceXAI — **Grok Build is Now Open Source**（2026-07-15）  
  <https://x.ai/news/grok-build-open-source>
- SpaceXAI — **Workflows in Grok Build**（2026-07-23）  
  <https://x.ai/news/workflows>
- SpaceXAI — **Grok Build on web and mobile**（2026-08-19）  
  <https://x.ai/news/grok-build-for-everyone>
- SpaceXAI — **Grok Build changelog**  
  <https://x.ai/build/changelog>

## 34.6 Pi

- `badlogic/pi-mono` — **Pi monorepo**  
  <https://github.com/badlogic/pi-mono>
- `badlogic/pi-mono` — **Pi coding agent documentation**  
  <https://github.com/badlogic/pi-mono/tree/main/packages/coding-agent>
- `badlogic/pi-mono` — **Pi Packages**  
  <https://github.com/badlogic/pi-mono/blob/main/packages/coding-agent/docs/packages.md>

---

# 35. 最後的研究結論

WorkBuddy 最值得研究的地方，不是它「功能很多」，也不是單純因為它內建 Hy3。

真正有意思的是：

1. **它把 foundation model 明確包在一個可替換模型的通用 agent harness 裡；**
2. **它同時把本機檔案、Office artifact、程式、web、SaaS、MCP、Skills、Memory、Automation、遠端 dispatch 都視為同一工作環境的一部分；**
3. **騰訊又同時在訓練 Hy3、開發 WorkBuddy、做 CodeBuddy、WMA 與 WorkBuddy Bench，形成完整 vertically integrated agent stack；**
4. **這種整合使它非常實用，也讓「模型能力」與「平台能力」更難直接拆開。**

因此對本 archive 最合理的研究態度，不是把 WorkBuddy 貶成「只是 wrapper」，也不是把騰訊的 end-to-end internal metric 當成純模型 benchmark。

真正應該被比較的是：

> **一個被完整記錄的 execution surface，在固定 specification 下，最後能不能交付正確 artifact、付出多少模型輸出、需要多少人工介入、以及平台本身提供了多少額外能力。**

這也是為什麼 WorkBuddy 很適合放進 `spec-driven-implementation-archive`：它迫使我們把「模型」、「Agent harness」與「產品執行環境」三件事分開看。