# Tencent WorkBuddy — Platform Profile and Comparative Analysis

**Research date:** 2026-08-24  
**Document purpose:** execution-surface reference for `spec-driven-implementation-archive`  
**Status:** analytical profile, not official Tencent documentation  
**Primary subject:** Tencent WorkBuddy desktop agent and the surrounding WorkBuddy ecosystem  

> [!IMPORTANT]
> WorkBuddy is a fast-moving product. Model availability, pricing, limits, UI labels, bundled Skills, supported connectors, and enterprise deployment options can change quickly. This document intentionally timestamps volatile facts and distinguishes **official product facts**, **vendor claims**, and **analysis/inference**. For a benchmark run, the run-specific README remains the source of truth for the exact model, tier, settings, permissions, memory state, and product version used.

## 1. Executive summary

Tencent WorkBuddy is best understood as a **general-purpose agentic desktop workstation for knowledge work**, rather than as a conventional chatbot or a coding agent with a graphical shell. Tencent describes the product as a full-scenario workplace AI agent desktop workbench: a user gives a natural-language goal, the system decomposes and executes multi-step work, operates over authorized local files, calls tools and connected services, and returns inspectable deliverables such as documents, spreadsheets, presentations, charts, code, reports, and other artifacts.

That positioning matters for this archive. A WorkBuddy run is not merely a test of the selected foundation model. It is the observed behavior of a complete execution surface that can include model routing, a WorkBuddy agent harness, permissions, local file access, Skills, MCP servers, Connectors, Experts, browser or command execution, memory, scheduled tasks, remote-control channels, and product-side orchestration. Two nominally identical prompts can therefore differ materially if any of those surrounding conditions differ.

WorkBuddy also sits inside a broader Tencent product family. Tencent Cloud now groups **CodeBuddy**, **WorkBuddy**, and **WorkBuddy Managed Agents (WMA)** under WorkBuddy Enterprise. The distinctions are meaningful:

- **CodeBuddy** is the coding-centric product family and development surface.
- **WorkBuddy** is the local/general workplace agent desktop workbench.
- **WorkBuddy Managed Agents** is a cloud-hosted enterprise runtime product built around the WorkBuddy Harness, with isolated Linux runtimes and sessions.

The WorkBuddy desktop is therefore neither simply “CodeBuddy for office files” nor the same thing as WMA. The products share infrastructure and ecosystem concepts, but they have different user-facing roles and execution boundaries.

A major part of WorkBuddy’s 2026 identity is **Tencent Hy3**. Hy3 is a hybrid fast/slow-thinking Mixture-of-Experts language model with **295 billion total parameters, 21 billion active parameters, and up to a 256K context window**. Tencent first released and open-sourced Hy3 preview in April 2026 and formally released Hy3 in July 2026. It is deeply integrated into WorkBuddy, CodeBuddy, Yuanbao, Marvis, ima, and Tencent Cloud TokenHub. Tencent explicitly says the final model was improved using feedback from real products including WorkBuddy. That makes Hy3 especially relevant when a WorkBuddy run selects it manually: the foundation model and the execution harness have been co-developed around overlapping real-world task distributions.

At the same time, **WorkBuddy is a multi-model product**. The product offers built-in models, an Auto routing mode, custom API models, and local Ollama integration. Consequently, “WorkBuddy” should never be treated as a model name. For reproducible experiments, this repository should record both the **surface** (`Tencent WorkBuddy`) and the **selected model/routing mode** (`Hy3`, `Auto`, or another explicit model), along with permission and memory settings.

Compared with other major 2026 agent platforms, WorkBuddy’s clearest distinguishing combination is:

1. a graphical, local-file-oriented workplace agent rather than a terminal-first shell;
2. broad office artifact creation alongside coding and research;
3. unusually broad model-provider freedom, including custom and local models;
4. a batteries-included ecosystem of Skills, Experts, Explore recipes, Connectors, MCP, scheduled Automation, and messaging-based remote control;
5. consumer and enterprise product paths under one umbrella.

Its main trade-off for rigorous benchmarking is the same strength that makes it useful to ordinary users: the platform adds many layers beyond the foundation model. Memory, Auto routing, Skills, account entitlements, permissions, remote integrations, and fast-changing product features can all affect a run. A serious evaluation must therefore treat WorkBuddy as an **execution environment** and control or document those layers explicitly.

---

## 2. Evidence model used in this profile

This profile uses three evidence bands.

### 2.1 Official fact

A statement is treated as an official product fact when it is supported by one of the following:

- Tencent WorkBuddy documentation on `workbuddy.ai`;
- Tencent Cloud WorkBuddy Enterprise documentation;
- Tencent corporate announcements or investor materials;
- Tencent WorkBuddy Privacy Policy or Service Agreement;
- the official `Tencent/workbuddy-bench` repository;
- first-party documentation from a competitor when describing that competitor.

Official documentation is authoritative for what Tencent or another vendor currently documents, but it is not automatically an independent verification of performance.

### 2.2 Vendor claim / internal metric

Performance, adoption, retention, speed, benchmark, or market-share statements published by Tencent or another vendor are labeled as **vendor claims** unless a third-party methodology is available. For example, Tencent’s statement that Hy3 exceeded a 90% task-success rate in internal WorkBuddy evaluations is useful evidence about Tencent’s product-development target, but it is not interchangeable with a public, independently reproducible benchmark.

### 2.3 Analysis / inference

Comparisons and benchmark-design implications are analytical conclusions drawn from the documented product architectures. They should not be read as vendor claims. In particular, this document avoids inferring undocumented internals merely from UI behavior.

---

## 3. What WorkBuddy is

Tencent Cloud’s product introduction describes WorkBuddy as a **full-scenario workplace AI agent desktop workbench** for many functional roles. The official description emphasizes five ideas:

- natural-language task specification;
- autonomous task decomposition and execution;
- multimodal work over documents, spreadsheets, presentations, and data;
- operations over authorized local files;
- delivery of outputs that can be inspected and accepted, rather than only conversational advice.

This framing makes WorkBuddy closer to an “AI workstation” than to a chat assistant. Its core unit of work is a **task**. In the current task-bar documentation, each conversation is an independent task with its own workspace and context, and multiple tasks can run in parallel.

The official task UI exposes three modes:

| Mode | Intent | File-changing behavior |
| --- | --- | --- |
| **Ask** | Q&A, inspection, clarification | Does not modify files |
| **Craft** | Directly execute the requested work | Can modify files and create deliverables |
| **Plan** | Produce a plan first, then execute after review | Delays execution until the plan is accepted |

This is a useful conceptual separation for benchmarks. An Ask-mode answer and a Craft-mode implementation are not equivalent experiments even if the prompt text is identical. Plan mode adds an explicit human checkpoint and may change the effective reasoning trajectory.

### 3.1 Local workspaces and deliverables

A WorkBuddy task is associated with a workspace, normally a folder from which the agent reads and to which it writes. The product is explicitly designed to perform real file operations: batch organization, document creation, code execution, report generation, data transformation, and similar work.

The important distinction is that **local execution does not mean that all task data remains local**. The WorkBuddy Privacy Policy states that task Inputs can include prompts, files, audio, commands, scheduled tasks, and integrated-app data, and that these Inputs may be processed by third-party LLM providers and by third-party Skills, MCP servers, or integrations selected by the user. A local workspace is therefore an execution boundary, not automatically a data-residency guarantee. A local Ollama model can change that data path substantially, but other enabled network tools may still send data externally.

### 3.2 Multiple tasks and agent parallelism

The task documentation says WorkBuddy supports multiple tasks in parallel. Tencent’s August 2026 Hy3 global announcement goes further, describing concurrent multi-agent capabilities. These should be distinguished:

- **parallel tasks** are visible user-level tasks that can run concurrently;
- **multi-agent orchestration** describes internal or explicit delegation among agents within more complex work.

For this archive, either form of parallelism affects token usage, elapsed time, filesystem contention, and attribution of work. A run should record whether the agent was allowed to spawn or coordinate subagents and whether other WorkBuddy tasks were concurrently active.

---

## 4. WorkBuddy in the Tencent product family

A common source of confusion is the shared documentation and branding around CodeBuddy, WorkBuddy, and WorkBuddy Enterprise. Tencent Cloud’s current WorkBuddy Enterprise overview gives the cleanest first-party taxonomy.

| Product | Primary role | Execution style |
| --- | --- | --- |
| **CodeBuddy** | AI-native software development | Coding-centric IDE/CLI/product surfaces |
| **WorkBuddy** | General workplace AI agent | Desktop workbench over local files, tools, office artifacts, research, and connected services |
| **WorkBuddy Managed Agents (WMA)** | Managed enterprise agents | Cloud-hosted runtime/session infrastructure |

This distinction should be preserved in benchmark metadata. A CodeBuddy CLI result is not a WorkBuddy desktop result. A WMA run is not a local WorkBuddy run.

### 4.1 WorkBuddy Managed Agents

WMA is especially useful for understanding Tencent’s agent architecture without assuming that the desktop is implemented identically. Tencent says WMA is built on the **WorkBuddy Harness** and defines three core objects:

- **Agent**: model, system role/prompt, Skills and tools;
- **Runtime**: an independent cloud environment with a Linux filesystem/terminal plus agent manifest and sessions;
- **Session**: an independent conversation/history/context inside a runtime.

Tencent describes the harness as containing orchestration, memory, action, and governance/control layers. WMA adds cloud persistence, traceability, evaluation, lifecycle management, enterprise identity, and cloud-resident sensitive assets. This is evidence that Tencent treats the agent harness as a first-class layer distinct from the model.

However, the desktop WorkBuddy should not be assumed to use the exact same sandbox or cloud lifecycle as WMA. In this profile, WMA is used as **sibling-product architectural context**, not as a substitute for desktop documentation.

### 4.2 CodeBuddy documentation overlap

The `workbuddy.ai` documentation portal still carries “Tencent Cloud Code Assistant CodeBuddy” site branding on many WorkBuddy pages, and some WorkBuddy model pages refer to historical `.codebuddy` configuration paths. That overlap is real, but it can mislead researchers into attributing CodeBuddy-only CLI behavior to WorkBuddy desktop.

This profile therefore uses a conservative rule: if a page is explicitly under the WorkBuddy documentation path and speaks about “Tencent WorkBuddy,” it is treated as WorkBuddy evidence. CLI- or IDE-specific CodeBuddy pages are used only when the relationship is explicitly stated or when discussing sibling-product design.

---

## 5. A practical mental model of the WorkBuddy stack

The following diagram is analytical, not an official architecture diagram. It summarizes the documented layers that can influence a WorkBuddy task.

```text
User goal / files / connected-app context
                │
                ▼
       Task mode + workspace
        Ask / Craft / Plan
                │
                ▼
     Permission & safety policy
 Default Permissions / Full Access
                │
                ▼
        WorkBuddy agent harness
  planning · context · orchestration
       memory · action · control
                │
        ┌───────┴────────┐
        ▼                ▼
 Model selection      Capability layer
 Auto / Hy3 /       Skills · Experts
 built-in / custom  MCP · Connectors
 API / Ollama       browser · scripts
        │           local files · code
        └───────┬────────┘
                ▼
       Tools and external systems
 filesystem · shell · web · SaaS · IM
                │
                ▼
       Artifacts / changes / result
 docs · sheets · slides · code · reports
                │
      ┌─────────┴──────────┐
      ▼                    ▼
 Memory / personalization  Automation / Assistant
 future sessions            scheduled or remote tasks
```

The model is only one layer. This is the single most important methodological point for anyone using WorkBuddy in an agent benchmark.

---

## 6. Permission model and safety boundary

WorkBuddy documents two principal permission modes.

### 6.1 Default Permissions

Default Permissions is the recommended everyday mode. Routine actions inside the workspace can proceed, while higher-risk actions stop for confirmation. The documentation asks the user to review the operation type, target scope, and reason before approving sensitive actions.

This resembles the human-in-the-loop approval model used by several coding agents, but WorkBuddy’s scope is broader because the agent can touch office files, external programs, and non-repository folders.

### 6.2 Full Access

Full Access suppresses those step-by-step confirmations and allows actions such as file writes/deletes, script execution, and external program calls to proceed without the same confirmation flow. Tencent explicitly recommends using it only for trusted, recoverable, isolated tasks.

For benchmark work, permission mode is a **material experimental variable**. Default mode may introduce pauses, rejected actions, or user intervention. Full Access may increase completion rates but also changes the safety posture. A run report should state the mode and enumerate any manual approvals.

### 6.3 Recommended benchmark practice

For a controlled implementation benchmark:

1. create a dedicated disposable workspace;
2. use version control or a pre-run snapshot;
3. record permission mode;
4. if using Default Permissions, record every approval or rejection;
5. avoid granting unrelated external filesystem access;
6. preserve WorkBuddy’s change view or exported conversation where possible;
7. do not compare a Full Access run to a heavily interrupted Default run without noting the difference.

---

## 7. Model layer: WorkBuddy is deliberately multi-model

The current WorkBuddy model documentation says the product includes multiple built-in models covering general reasoning, multimodal, and image-processing use cases, and that the available set can vary by version, account type, and service availability.

### 7.1 Auto routing

**Auto Mode** selects a model based on the task. This is convenient for ordinary users but undesirable for model-specific benchmarking unless routing is itself the subject of evaluation. “WorkBuddy Auto” is a valid surface configuration, but the result cannot be attributed cleanly to one model.

### 7.2 Explicit built-in model selection

Paid plans and promotional periods can expose a broader set of named models. The set is volatile. A run should record the exact display name and, when visible, the model ID/version rather than merely “WorkBuddy.”

### 7.3 Custom API models

WorkBuddy provides a UI for adding custom model endpoints and can populate capabilities such as tool calling, image input, and reasoning mode for recognized providers. Custom endpoints can also be configured manually.

This is strategically important: WorkBuddy can be treated as a **harness independent of the foundation model**. In principle, the same WorkBuddy task can be evaluated with Hy3, another hosted model, a self-hosted model, or an internal company endpoint while preserving much of the upper-layer UX and tooling.

### 7.4 Local Ollama

WorkBuddy officially documents Ollama integration through an OpenAI-compatible local endpoint, normally on port 11434. Tencent presents this for privacy/on-premises scenarios, avoiding external token/API costs, and offline model inference.

The benchmark interpretation should be precise. Local Ollama can keep the **model inference path** on the machine, but WorkBuddy Skills, Connectors, MCP servers, web browsing, telemetry, or other services may still use the network. “Local model” is not equivalent to “fully air-gapped WorkBuddy task” unless the whole tool graph is controlled.

---

# 8. Hy3 deep dive

Hy3 deserves a separate section because it is both a foundation model and a major part of Tencent’s 2026 WorkBuddy strategy.

## 8.1 Timeline

### April 24, 2026 — Hy3 preview

Tencent announced and open-sourced **Hy3 preview**, describing it as a hybrid fast-and-slow-thinking Mixture-of-Experts model. The official specifications were:

- 295B total parameters;
- 21B active parameters;
- up to 256K context;
- focus areas including complex reasoning, instruction following, context learning, coding, agent capabilities, and inference efficiency.

Tencent also said it had rebuilt its pre-training and reinforcement-learning infrastructure beginning in February 2026, emphasizing broad capability, real-world evaluation rather than benchmark-only optimization, and model/inference co-design for cost efficiency.

### July 6, 2026 — official Hy3

Tencent formally released Hy3, describing improved stability and cost efficiency relative to preview. Hy3 was integrated across WorkBuddy/CodeBuddy, Yuanbao, Marvis, ima, and other Tencent products, and made available through Tencent Cloud TokenHub.

The underlying headline architecture remained 295B total / 21B active MoE with up to 256K context.

### July 24, 2026 — TokenHub migration and thinking modes

Tencent Cloud announced that Hy3-preview Token Plan traffic would route to Hy3 and documented three reasoning modes:

- `no_think` — fastest response;
- `think_low` — lighter/faster reasoning;
- `think_high` — deeper reasoning.

The same announcement says the final Hy3 incorporated feedback from Yuanbao, WorkBuddy, ima, and Marvis and focused improvements on coding agents, long-document understanding, multi-turn context, search QA, and complex task execution.

### August 5, 2026 — broader global availability

Tencent announced global Hy3 availability through WorkBuddy, Tencent Design Miora, and TokenHub. The WorkBuddy integration was highlighted as a major product surface for practical agent use.

## 8.2 What “295B total / 21B active” means

Hy3’s parameter counts are frequently misunderstood. The model is not simultaneously a 295B dense model and a 21B dense model. In a Mixture-of-Experts architecture, the model contains many expert parameter blocks, while routing selects only a subset for a given token or computation step.

- **295B total** describes the overall parameter capacity stored across the model.
- **21B active** describes the approximate parameter subset activated for an inference step under the documented routing design.

This allows a large total representational capacity without paying the compute cost of executing all 295B parameters on every token. It does **not** imply that compute is exactly `21/295` of a comparable dense model: attention, routing, memory movement, KV-cache behavior, parallel hardware efficiency, context length, and implementation details all matter.

For an agent workload, that distinction is especially relevant because long reasoning chains can make decoding cost dominate. Sparse activation helps explain why Tencent emphasizes both reasoning capability and deployment economics, but it should not be converted into a precise energy or cost estimate without serving-system measurements.

## 8.3 Fast/slow thinking as a product concept

Tencent calls Hy3 a hybrid fast-and-slow-thinking model. TokenHub’s `no_think`, `think_low`, and `think_high` modes make that concept operational: the caller can choose a latency/reasoning-depth trade-off.

In WorkBuddy, there are two additional layers to consider:

1. the model may have its own reasoning-mode selection or explicit setting;
2. WorkBuddy itself may route or orchestrate tasks, including switching to specialized models for capabilities the current model does not provide.

Therefore, a “Hy3 run” should ideally record the thinking mode when exposed. If WorkBuddy hides it behind a product preset, the report should say so rather than guessing.

## 8.4 Context length and agent use

A 256K context window is valuable for long documents, repository context, multi-step task histories, and tool-rich agent trajectories. But context-window maximum and effective context quality are different questions. A long-horizon WorkBuddy task can also involve compression, retrieval, file re-reading, subagents, or product-side context management.

For a benchmark, record not only the model’s nominal context limit but also whether the platform visibly compacted or summarized conversation history.

## 8.5 Hy3 and WorkBuddy co-design

Tencent’s final-release and global-availability materials explicitly connect Hy3 development with real product feedback. The TokenHub migration notice names WorkBuddy among the products whose feedback informed improvements. Tencent’s August global announcement says Hy3 was co-designed with Tencent products and optimized using large-scale real-world usage.

This matters analytically. Agent performance is not just “model intelligence.” A model trained or post-trained on the interaction patterns of a particular harness can be better calibrated to that harness’s tool schemas, task decomposition style, office deliverables, and error recovery. Hy3 inside WorkBuddy is therefore a notable example of **model–harness co-design**.

## 8.6 Tencent’s WorkBuddy internal metrics: useful, but not an independent benchmark

Tencent reports that, in internal workplace evaluations, Hy3 used in WorkBuddy achieved **more than 90% task success** and reduced average task completion time by **34%** relative to the previous Hy model iteration. Tencent’s global article also says WorkBuddy has more than 100 built-in skills and concurrent multi-agent capabilities.

These numbers should be handled carefully:

- the evaluation set and scoring procedure are not fully published in the announcement;
- it is a Tencent internal evaluation, not an independent laboratory benchmark;
- the comparison is to a prior Hy iteration, not necessarily to competing frontier models under identical conditions;
- product updates to WorkBuddy may also influence end-to-end results.

The proper interpretation is: Tencent reports substantial end-to-end improvement from deploying Hy3 in its own workplace agent. It is evidence of product tuning and the vendor’s performance target, not a substitute for an external reproducible evaluation.

## 8.7 “20+ Skills” versus “100+ Skills”

Two official sources currently use different counts. The WorkBuddy task-bar documentation says the product includes **20+ skill packages**, while Tencent’s August Hy3 global announcement says WorkBuddy has **more than 100 built-in skills**.

The safest interpretation is that the product and/or definition of “built-in skill” evolved, or that the two pages use different counting scopes. This profile deliberately does not collapse them into a single timeless number. For a recorded run, inspect the actual available Skill set at the time of execution.

## 8.8 Why Hy3 matters for this archive

For this repository, Hy3 offers a useful experimental axis:

- WorkBuddy + Hy3 tests Tencent’s vertically integrated model/harness path;
- WorkBuddy + another hosted model tests the same surface with a different foundation model;
- WorkBuddy + local Ollama can probe how much performance comes from the harness versus a chosen local model;
- Hy3 in a different harness (for example a generic coding harness) can probe the reverse direction.

That 2×2 style of comparison is much more informative than treating “WorkBuddy score” and “Hy3 score” as synonyms.

---

## 9. Skills and custom capabilities

Skills are packaged capabilities or workflows that the agent can invoke. Current WorkBuddy documentation describes bundled Skills for office documents, data/reporting, design, file handling, and related scenarios. Users can select installed Skills when creating a task.

WorkBuddy also supports:

- a Skill marketplace;
- community Skills;
- importing compatible community skill packages;
- custom Skill creation from natural-language descriptions;
- enabling, disabling, updating, and uninstalling Skills.

This makes Skills analogous to reusable workflow modules. From a benchmark perspective, Skills can materially change success rates by injecting instructions, tools, templates, or domain procedures. A reproducible run should record the enabled Skills or explicitly state that no non-default Skills were enabled.

### 9.1 Security implications

A Skill is not merely decorative prompt text if it can invoke tools or direct the agent to execute actions. WorkBuddy says marketplace Skills are security-scanned before installation, but third-party capability code should still be treated as part of the trusted computing base.

This is conceptually similar to Claude Code plugins, Codex plugins/Skills, Antigravity Skills, Grok Build plugins, or Pi packages. The ecosystem layer is powerful precisely because it can alter agent behavior.

---

## 10. MCP integration

WorkBuddy provides UI-integrated support for the Model Context Protocol (MCP), an open standard for connecting models/agents to external tools and data.

The documented capabilities include:

- adding MCP servers;
- URL/authentication configuration;
- OAuth flows with token refresh/reconnection;
- enabling or disabling individual servers/tools;
- local/project-scoped configuration in current Chinese WorkBuddy documentation;
- a Tencent Cloud MCP marketplace.

The practical effect is that WorkBuddy can expose databases, business systems, messaging, APIs, and custom tools to the agent without those integrations being hard-coded into the WorkBuddy core.

For benchmark reproducibility, an MCP server is part of the experimental environment. Record server names, versions, enabled tools, and whether remote services were reachable.

---

## 11. Connectors

WorkBuddy’s first-party Connectors provide OAuth/API integrations for common services. The current official connector page lists:

- GitHub;
- GitLab;
- Jira;
- Confluence;
- Google Drive;
- Gmail;
- Notion;
- Slack.

Once connected, they can become automatically available during task execution. This is a major reason WorkBuddy can behave as a cross-application work agent rather than a file-only assistant.

The distinction between **Connector** and **MCP** is worth preserving:

- Connectors are productized integrations with a guided setup and known service semantics;
- MCP is a general protocol boundary for external tools/services.

Both can move data outside the local machine, and their scopes should be documented in high-assurance experiments.

---

## 12. Expert Center

The Expert Center provides specialized personas/configurations for domains such as writing/content, data/analysis, development, design, and business. Tencent describes each Expert as carrying domain-specific knowledge and tailored prompts.

This is not the same as selecting a different foundation model. An Expert is better thought of as a **task-specialized agent configuration/persona layer**. The model may stay the same while the effective system/context instructions change.

For benchmarking, using an Expert is analogous to using a specialized system prompt or pre-packaged agent. It should be recorded explicitly.

---

## 13. Explore

Explore is WorkBuddy’s curated/community recipe layer. Users can browse examples and click **Make My Version**, which preloads the Prompt, associated Skill, and Expert configuration.

Tencent’s own conceptual distinction is useful:

- **Skill**: what the agent can do;
- **Expert**: who/which specialized role helps;
- **Explore**: an example of what was produced by combining those capabilities.

Explore lowers the blank-page barrier for ordinary users, but from a scientific evaluation perspective it introduces pre-tuned context. A run created from an Explore recipe is not equivalent to a zero-shot prompt unless that recipe is captured.

---

## 14. Automation

WorkBuddy Automation supports scheduled recurring or one-time tasks. Current documentation lists:

- hourly;
- daily;
- weekly;
- one-time schedules;
- optional workspace selection;
- templates;
- completion notifications through connected messaging platforms.

The current global pricing page also makes the number of automated tasks a plan entitlement, with promotional limits that may differ from standard limits.

A local WorkBuddy automation should not be conflated with a cloud-managed always-on agent. WorkBuddy’s separate WMA product is the stronger fit for 7×24 cloud execution. For desktop automations, the exact runtime prerequisites should be verified for the installed version.

---

## 15. Assistant remote control

WorkBuddy Assistant allows users to send tasks to a computer running WorkBuddy through messaging apps. The official documentation lists Slack, Telegram, Discord, WeChat Work, Feishu, DingTalk, QQ, YuanbaoPai, and WeChat AssistantBot.

The normal flow is:

1. user sends a message from a phone/chat platform;
2. WorkBuddy on the computer receives it;
3. the desktop executes the task using its local context;
4. the result is returned through the messaging channel.

Tencent explicitly notes that the computer must remain powered on, WorkBuddy must be running, and a stable Internet connection is required.

This feature is an important contrast with products that move the task to a vendor-hosted cloud sandbox. WorkBuddy Assistant is remote control of a local execution surface; WorkBuddy Managed Agents is the separate cloud-runtime path.

---

## 16. Memory and personalization

WorkBuddy Memory is enabled by default according to the current documentation. It extracts relevant context, preferences, and habits from conversations for use in future interactions. Memory summaries are regenerated nightly, can be viewed/edited, and users can ask WorkBuddy to remember or forget information. The product also supports importing usage habits from another AI service by copying a generated summary into WorkBuddy.

This is valuable product behavior and a major benchmark confounder.

### 16.1 Reproducibility implication

If Memory is enabled, a WorkBuddy run may depend on previous unrelated conversations even when the visible current prompt is identical. Therefore a benchmark should choose one of two policies:

- **clean-memory policy:** disable/clear memory and record that state;
- **naturalistic-product policy:** leave memory enabled but treat the result as a personalized end-user surface observation rather than a clean model comparison.

Neither is inherently wrong; the experiment must say which one it is.

---

## 17. Privacy, data flow, and security

This section uses the international WorkBuddy Privacy Policy last updated July 2026. Legal terms can change and should always be re-checked before enterprise deployment.

### 17.1 Controller and international service

The current international Privacy Policy identifies **Tencent Cloud International Pte. Ltd.** in Singapore as the data controller for the consumer service, while noting that enterprise customers may act as controller when WorkBuddy is provided through an enterprise sub-account.

### 17.2 What WorkBuddy treats as Inputs and Outputs

The policy defines Inputs broadly: prompts, text, files, audio, commands, chat/coding/agentic-session instructions, scheduled tasks, and integrated-app content. Outputs include generated responses and actions.

This broad definition is appropriate for an agent. A filesystem operation or connector call can carry substantially more sensitive context than a normal chatbot message.

### 17.3 Third-party LLM processing

The policy says WorkBuddy uses various third-party LLMs and that user Inputs are processed by a selected third-party LLM for inference. If users install third-party Skills, MCP connectors, messaging channels, or external applications, relevant personal information can also be sent to those services according to the user’s instruction.

Therefore the data path depends on the configured model and tool graph.

### 17.4 Model-training default

The international Privacy Policy states that **Inputs and Outputs are not used for AI model training by default**. Users who previously enabled “Help improve the model” can turn it off. It also says Inputs/Outputs are not used for WorkBuddy model training when the user brings their own AI model through API-key integration.

This is a policy statement, not a cryptographic property. Organizations should still evaluate the privacy terms of the chosen third-party model provider and integrations.

### 17.5 Configuration data

The policy says configuration information—such as model-selection preferences, third-party account bindings, device bindings, Skills, messaging configuration, MCP configuration, automation settings, and general settings—is stored locally on the device and is not processed on WorkBuddy servers for that configuration purpose.

This is an important distinction from Inputs/Outputs, which may be remotely processed.

### 17.6 Storage and cross-border access

The international policy says personal information is stored on servers in **Singapore** and that global support and engineering teams, including teams in the People’s Republic of China, may access information for the purposes described by the policy. It lists safeguards such as contractual arrangements and data-processing agreements.

### 17.7 Retention

The July 2026 policy states:

- Inputs and Outputs: up to **14 days** on the service side, with local copies retained until the user deletes them locally;
- account/diagnostic/feedback data: generally account duration plus the stated deletion window;
- billing/payment-related records: longer retention, including up to 24 months after account termination for specified data;
- configuration information: locally stored rather than server-processed as described above.

### 17.8 Security controls

Tencent lists encryption, hashing, role-based access controls, and audit logs among its information-security measures. WorkBuddy Enterprise adds broader organization-level controls such as asset isolation, role permissions, audit and content-security features, with deployment options ranging from shared SaaS to dedicated/private environments.

### 17.9 Practical security conclusion

“Runs on local files” should never be shortened to “all data stays local.” A more accurate statement is:

> WorkBuddy can execute against local workspaces, while inference and connected-tool data may leave the device depending on the chosen model, Skills, MCP servers, Connectors, messaging channels, and account configuration. Local Ollama can keep the LLM inference path local, but the rest of the capability graph must also be controlled for a genuinely local-only workflow.

---

## 18. Enterprise deployment and governance

WorkBuddy Enterprise is Tencent’s organizational umbrella across coding, workplace agents, and managed-agent runtime. Tencent Cloud documents multiple deployment forms including shared-cloud SaaS, dedicated-cloud arrangements, and private deployment options.

The enterprise positioning adds concerns that consumer benchmarks usually ignore:

- identity and role-based authorization;
- enterprise asset boundaries;
- centrally managed model availability;
- auditability;
- content safety;
- enterprise Connectors and MCP policies;
- data-residency/deployment choices;
- managed cloud agents through WMA.

An enterprise WorkBuddy run may therefore differ materially from a consumer/free run even when the UI and model name look similar.

---

## 19. Pricing and availability snapshot

**Snapshot date: 2026-08-24. Treat all pricing and promotions as volatile.**

The current global WorkBuddy pricing page lists:

| Plan | Price | Monthly credits (current documented base/bonus) | Automation entitlement |
| --- | ---: | ---: | ---: |
| Free | $0 | 100 base | 3 standard, with promotional expansion documented |
| Pro | $10/month or $96/year | 1,000 base + 1,000 bonus | 15 standard, with promotional expansion documented |
| Team | $40/seat/month or $480/seat/year | 1,000 per seat in shared pool | team features plus admin/billing controls |

Promotional periods temporarily expand model access, code-completion limits, daily credits, and automation counts. Those temporary entitlements are exactly why archive runs should record the **actual account tier and observed model availability**, not infer them from a later pricing page.

Tencent’s China-market WorkBuddy Enterprise/Buddy AI pricing is different and uses RMB subscription tiers and shared credits across CodeBuddy and WorkBuddy. Region should therefore be part of run metadata.

---

## 20. Platform availability

Current official installation guides document WorkBuddy desktop on Windows and macOS.

- Windows 10 1809+ or Windows 11, x64/ARM64;
- macOS 12+ on Apple Silicon or Intel;
- 4 GB minimum / 8 GB recommended memory in the current guides;
- Internet connectivity for normal cloud-backed use;
- filesystem and, on macOS, relevant system permissions for agent capabilities.

The global and China documentation also shows regional differences in authentication flows (for example, global Google/GitHub OAuth versus China-specific login paths). Again, **region is part of the execution environment**.

---

# 21. WorkBuddy Bench: what it is and what it is not

Tencent maintains the public `Tencent/workbuddy-bench` repository. Despite the name, WorkBuddy Bench is not simply a fixed test of the WorkBuddy desktop application. It is a broader agent evaluation framework designed around real-world work tasks and a configurable agent harness.

The official repository describes four subsets:

| Subset | Tasks | Focus |
| --- | ---: | --- |
| Code | 80 | repository-level software engineering |
| Web | 70 | frontend/GUI work |
| Office | 50 | mixed office-file/data workflows |
| Security | 60 | security/vulnerability work |

The framework places an agent CLI/harness into a local Docker sandbox, executes tasks, captures patches/trajectories/test results/efficiency, and scores the outcome. Model and harness are configured separately; the example model configuration in the repository uses Hy3, while harnesses can be selected independently.

### 21.1 Why it matters

WorkBuddy Bench supports Tencent’s broader thesis that agent quality should be evaluated on role-like real tasks rather than only short synthetic benchmarks. This aligns well with the philosophy of `spec-driven-implementation-archive`, which also focuses on long-horizon implementation and verification.

### 21.2 What it does not prove

A WorkBuddy Bench score does not automatically measure:

- WorkBuddy desktop UX;
- a particular consumer WorkBuddy version;
- the impact of WorkBuddy Memory, Experts, Explore, Assistant, or desktop permission prompts;
- a foundation model in isolation.

The benchmark’s model, harness, Docker environment, dataset version, and evaluator must all be specified.

---

# 22. Why WorkBuddy is interesting for this archive

This repository studies complete implementation runs under fixed specifications. WorkBuddy introduces several useful axes of comparison that coding-only surfaces do not expose as cleanly.

### 22.1 Model versus harness

Because WorkBuddy can select multiple models, it is possible to hold the harness approximately constant while varying the foundation model. Conversely, Hy3 can be used through other harnesses, allowing partial separation of model and orchestration effects.

### 22.2 Office-generalist versus code-specialist execution

A C17/X11 implementation task is software engineering, but WorkBuddy is not designed solely around Git workflows. Comparing it with coding-first agents can reveal whether a general workplace harness pays an orchestration penalty on deep code tasks or benefits from broader artifact/file tooling.

### 22.3 Long visible trajectories

WorkBuddy’s exported web/desktop conversation can expose substantial reasoning/tool trajectories depending on product version and exporter behavior. That can make it unusually useful for studying how an agent spends effort—provided output accounting is defined carefully and repository file payloads are not double-counted.

### 22.4 Personalization as a measurable variable

Memory can be deliberately disabled for clean experiments or retained for naturalistic product studies. Few benchmark archives make this distinction explicit even though modern agent products increasingly personalize behavior.

---

# 23. Minimum metadata for a reproducible WorkBuddy run

A serious run README should record at least the following.

### Product identity

- WorkBuddy version/build if visible;
- operating system;
- region/global versus China service;
- account tier;
- run date and timezone.

### Model

- model display name;
- exact model ID/version if visible;
- Auto routing on/off;
- reasoning/thinking level if exposed;
- custom API or local Ollama details if applicable.

### Agent configuration

- Ask / Craft / Plan;
- Default Permissions / Full Access;
- workspace path policy;
- enabled Skills;
- Expert selection;
- Explore template, if any;
- MCP servers/tools;
- Connectors;
- Memory enabled/disabled/cleared;
- other concurrent tasks/subagents.

### Human intervention

- permission approvals;
- plan approval/editing;
- retries or manual messages;
- user-edited files during the run;
- restarts/reconnects.

### Evidence

- exported full conversation;
- filtered model-output corpus and extraction policy;
- final project snapshot;
- build/test logs or external review;
- timestamps and usage/credit figures if visible.

Without these details, “WorkBuddy + Hy3” is too underspecified to be a strong reproducibility label.

---

# 24. Comparison at a glance

The following table summarizes product orientation as of the research date. It intentionally compares **execution surfaces**, not foundation-model intelligence.

| Dimension | WorkBuddy | Google Antigravity | OpenAI Codex | Claude Code | Grok Build | Pi |
| --- | --- | --- | --- | --- | --- | --- |
| Primary identity | General workplace agent desktop | Agent-first development platform | Coding-first agent expanding into knowledge work | Coding-first agent engine across CLI/desktop/web/IDE | Coding-agent harness plus broader Build branding | Minimal terminal coding harness |
| Main UX | Desktop tasks/artifacts | Desktop + CLI + SDK + IDE | App + CLI + IDE + cloud | CLI + IDE + desktop + web | Terminal TUI; separate web/mobile Build | Terminal |
| Local files | Strong | Strong in local modes | Strong local/app | Strong local/desktop/CLI | Strong terminal/local-first | Strong |
| Cloud runtime | Enterprise WMA is separate | Managed agent/API options | Codex cloud | Claude Code web/Routines | workflows/background plus broader xAI surfaces | User-supplied/integration-dependent |
| Parallelism | Parallel tasks, multi-agent claims | Dynamic subagents, projects, worktrees | Parallel agents, worktrees | Parallel sessions, subagents/agent teams by surface | Parallel subagents; workflows up to large fan-out | Not built in; user extends it |
| Model freedom | High: built-in, custom API, Ollama | Primarily Google/Gemini ecosystem | OpenAI model ecosystem | Claude-centric; some third-party hosting options | Grok/xAI by default; local-first harness now configurable | Very high, multi-provider/local/custom |
| MCP | Built in | Built in | Via apps/plugins/tool ecosystem; product evolves | First-class MCP | First-class MCP | Intentionally not core; extension possible |
| Skills/extensions | Skills + marketplace | Skills + MCP + hooks | Skills/plugins | Skills/plugins/hooks | Skills/plugins/hooks | Extensions/Skills/templates/packages |
| Office artifacts | Core use case | Increasingly broad, still development-first | Major 2026 expansion | Possible, but software-engineering identity remains strong | Broader xAI ecosystem supports office/build work; terminal harness remains coding-centric | Whatever user builds around it |
| Scheduling | WorkBuddy Automation | Scheduled Tasks | Automations | local scheduled tasks + cloud Routines | Grok-wide automations; workflows for large runs | External tools/extensions |
| Remote/mobile dispatch | Assistant via many IM channels | Browser Remote Control | app/cloud ecosystem | Remote Control, Dispatch, Slack/Channels | broader Grok web/mobile ecosystem | User-built/SDK integrations |
| Harness openness | Proprietary desktop; WMA documented | Proprietary core platform | Proprietary product | Proprietary product, extensible | Terminal harness open-sourced | Open source, MIT, highly hackable |
| Best fit | Nontechnical + technical workplace tasks with files/tools | Developers orchestrating multiple agent workflows | Software/knowledge work in OpenAI ecosystem | Deep software engineering with highly composable Claude tooling | High-parallel coding and xAI-native building | Power users who want minimalism and control |

This matrix is a simplification. The deeper comparisons below explain where the products have converged and where the differences remain structural.

---

# 25. WorkBuddy vs Google Antigravity

Google Antigravity changed substantially in 2026. Antigravity 2.0 is a standalone desktop application rather than merely an IDE feature, and Google also provides an Antigravity CLI, SDK, and IDE surface. It is therefore a broader platform than a simplistic “coding IDE” label suggests.

## 25.1 Shared ideas

Both products now support:

- graphical agent command centers;
- local project/file context;
- multiple concurrent agents/tasks;
- permissions and approval controls;
- Skills and MCP-style extensibility;
- scheduled work;
- remote monitoring/control;
- artifact-oriented progress/results.

This convergence reflects a broader industry shift from inline autocomplete to long-running autonomous work.

## 25.2 Antigravity’s structural advantage for software projects

Antigravity’s Projects and native Git worktree support are explicitly designed for development isolation. A Project can span multiple folders, use scoped security settings, and launch agents in separate worktrees. Dynamic subagents can receive isolated worktrees automatically.

WorkBuddy can certainly perform coding tasks, but its core workspace abstraction is more general and office-file oriented. It does not define itself around Git isolation in the same way.

For a multi-agent coding benchmark, Antigravity’s worktree-native orchestration is therefore a cleaner default. For a mixed task involving spreadsheets, presentations, local documents, email, and research, WorkBuddy’s general workplace assumptions may be more natural.

## 25.3 Model ecosystem

Antigravity’s first-party product is tightly aligned with Google’s Gemini model ecosystem. WorkBuddy intentionally supports multiple built-in and custom providers and local Ollama.

That makes WorkBuddy more attractive for **harness-controlled cross-model experiments**. Antigravity can still be studied as a platform, but changing the underlying model family is not the same first-class product proposition.

## 25.4 Scheduling and remote control

Both platforms have scheduled tasks. Antigravity 2.0 Remote Control lets a user monitor/approve desktop sessions from a browser while retaining local workstation context. WorkBuddy Assistant routes tasks through a much broader list of messaging platforms, which is especially convenient for workplace chat ecosystems.

The conceptual difference is interface more than capability: browser-based remote command center versus chat-platform remote dispatch.

## 25.5 Research recommendation

Use Antigravity when the experiment is fundamentally about multi-repository development, worktrees, dynamic developer subagents, or Google’s unified agent platform. Use WorkBuddy when the experiment is about general knowledge work, office artifacts, connector-heavy workflows, or comparing multiple models inside one desktop harness.

---

# 26. WorkBuddy vs OpenAI Codex

Codex began as a coding agent, but by mid-2026 it is inaccurate to describe it as “coding only.” OpenAI reports more than five million weekly Codex users and says knowledge workers use Codex for reports, spreadsheets, presentations, contracts, research, data analysis, workflow automation, and lightweight tools. OpenAI has also introduced role-specific plugins, Sites, and annotations.

## 26.1 Shared direction

WorkBuddy and Codex increasingly overlap in:

- long-running autonomous tasks;
- parallel agent work;
- reusable Skills/workflows;
- scheduled Automations;
- creation of office/knowledge artifacts;
- connected applications and data;
- visual review of produced work.

This makes them interesting direct competitors in 2026 rather than products from entirely different categories.

## 26.2 Different historical center of gravity

Codex’s strongest architectural DNA remains software development:

- Git repositories;
- isolated worktrees;
- diffs and code review;
- CLI and IDE history;
- cloud coding agents;
- developer automation.

OpenAI’s knowledge-work expansion builds outward from the premise that code is a universal mechanism for transforming information and automating work.

WorkBuddy starts from a different user story: the user has a workplace task and local/connected business files, and the agent should plan, operate tools, and deliver an artifact. Coding is one capability among many.

That distinction affects ergonomics even when the final tasks overlap.

## 26.3 Model freedom

WorkBuddy has a notable advantage for researchers who want provider flexibility: built-in models, custom API models, and Ollama are documented first-class paths.

Codex is an OpenAI product and is optimized around OpenAI’s model/service stack. That can improve vertical integration, but it is not a neutral multi-provider harness in the WorkBuddy/Pi sense.

## 26.4 Worktrees versus general filesystem work

Codex’s app explicitly isolates parallel coding agents in Git worktrees. This is excellent for software reliability. WorkBuddy’s permissions/workspaces are more flexible for arbitrary file trees and office projects, but that flexibility does not automatically provide the same Git-native isolation.

## 26.5 Knowledge-work convergence

OpenAI’s June 2026 product changes significantly narrow WorkBuddy’s “generalist” differentiation. Role plugins package applications, skills, instructions, and workflows; Codex can open and work on documents, slides, spreadsheets, and interactive Sites. The difference is now more about **ecosystem, model choice, and product philosophy** than whether either product can touch office work.

## 26.6 Research recommendation

For this archive, Codex is the stronger comparator when asking: “How far can a coding-first agent extend into general work?” WorkBuddy asks the reverse question: “How far can a workplace-first desktop agent go on rigorous engineering?” Running the same spec through both is therefore unusually informative.

---

# 27. WorkBuddy vs Anthropic Claude Code

Claude Code is another product that should not be frozen in its early terminal-only image. Current Anthropic documentation says the same Claude Code engine is available through terminal, IDE, desktop, and web surfaces, with Remote Control, Dispatch, Slack integration, recurring tasks, MCP, Skills, Hooks, subagents, plugins, and agent teams.

## 27.1 Claude Code’s software-engineering center

Claude Code still defines itself as an **agentic coding tool**. Its core abstractions—`CLAUDE.md`, repository context, shell commands, code edits, Git integration, CI/CD, subagents, hooks—are engineered around software development.

WorkBuddy’s core vocabulary is broader: workplace tasks, files, office deliverables, Skills, Experts, Explore, Connectors, Assistant.

## 27.2 Extensibility comparison

Claude Code has one of the deepest composability stories among commercial coding agents:

- `CLAUDE.md` persistent instructions;
- Skills;
- MCP;
- subagents;
- agent teams;
- Hooks;
- Plugins and marketplaces;
- Agent SDK;
- CI integrations.

WorkBuddy is more curated in the consumer desktop: Skills, Experts, Explore, Connectors, MCP, Automation, and Assistant are exposed through a user-friendly GUI. This lowers the barrier for non-developers but gives less of the “Unix-like programmable agent” feel of Claude Code CLI.

## 27.3 Scheduling

Claude Code has a particularly explicit local/cloud split:

- **Desktop scheduled tasks** run on the local machine and can use local files/tools;
- **Routines** run on Anthropic-managed infrastructure and can trigger on schedules, API calls, or GitHub events;
- `/loop` handles session-local polling.

Tencent’s analogous split is WorkBuddy desktop Automation versus WorkBuddy Managed Agents for 7×24 cloud-hosted execution, though the products are organized differently.

## 27.4 Remote operation

Claude Code supports Remote Control, Dispatch, Slack, mobile/browser continuation, and Channels. WorkBuddy Assistant supports a wider set of IM platforms out of the box, especially China/Asia workplace ecosystems.

## 27.5 Models

Claude Code is primarily optimized around Claude models, although Anthropic supports enterprise hosting paths and some third-party cloud providers. WorkBuddy’s custom-model and local-model UI makes model plurality a more central product feature.

## 27.6 Research recommendation

Claude Code is a strong baseline for deep engineering quality and highly composable agent tooling. WorkBuddy is a strong baseline for an integrated generalist agent workstation. If WorkBuddy + Hy3 matches or exceeds Claude Code on a hard software spec, that result would be especially interesting because the surfaces have different design centers; if Claude Code wins, the result may reflect harness specialization as much as foundation-model quality.

---

# 28. WorkBuddy vs SpaceXAI Grok Build

“Grok Build” now refers to two related but distinct experiences:

1. the **terminal coding agent/harness** introduced in May 2026;
2. the later **web/mobile Build experience** for creating and publishing apps, games, websites, and dashboards.

For agent-framework comparison, the terminal coding agent is the closer peer.

## 28.1 Terminal Grok Build

SpaceXAI introduced Grok Build as a terminal agent with:

- plan/review/approve workflow;
- clean diffs;
- parallel specialized subagents;
- deep Git worktree integration;
- headless `-p` mode;
- Agent Client Protocol support.

In July, SpaceXAI open-sourced the harness/TUI, including the agent loop, tools, extension system, Skills, plugins, Hooks, MCP, and subagents. It also added a local-first path using a locally configured inference backend.

That openness is a major structural difference from WorkBuddy desktop.

## 28.2 Workflows and extreme fan-out

Grok Build Workflows can fan a large task across many parallel agents. SpaceXAI documents a default budget of 128 agents and up to 1,024 for large jobs, with resumable phases and reusable workflow definitions. This is much more explicit large-scale fan-out than WorkBuddy’s public desktop documentation currently exposes.

For huge code-review/research decompositions, Grok Build’s workflow engine is a distinctive capability.

## 28.3 WorkBuddy’s advantage in ordinary workplace integration

WorkBuddy remains the more integrated general workplace desktop:

- office documents and data are first-class;
- Connectors are surfaced directly;
- Experts and Explore target nontechnical workflows;
- Assistant dispatch supports many workplace messaging apps;
- the model layer can be replaced independently.

Grok’s broader 2026 ecosystem increasingly covers office work too—SpaceXAI has shipped Skills, Connectors, automations, Office add-ins, web/mobile Build, and other agent features—but the terminal Grok Build harness remains strongly engineering-centric.

## 28.4 Openness and observability

Researchers can inspect the open-source Grok Build harness and, in principle, trace context assembly and tool dispatch. WorkBuddy’s desktop harness is proprietary. This gives Grok Build an advantage for mechanistic/implementation-level agent research.

On the other hand, WorkBuddy may be more representative of how non-developer users actually consume an integrated agent product.

## 28.5 Research recommendation

Use Grok Build when harness transparency, terminal automation, Git isolation, or very large agent fan-out is central. Use WorkBuddy when cross-application workplace automation and model-provider flexibility are central.

---

# 29. WorkBuddy vs Pi coding agent

Pi is the philosophical opposite of WorkBuddy in several respects. The official `badlogic/pi-mono` project describes Pi as a **minimal terminal coding harness** that should adapt to the user rather than dictate a workflow.

## 29.1 Pi’s deliberate minimalism

Pi intentionally omits several features that other agents make core:

- no built-in MCP;
- no built-in subagents;
- no built-in permission popups;
- no built-in plan mode;
- no built-in to-do system;
- no built-in background bash.

The point is not that Pi cannot do those things. Its design is to implement them through TypeScript Extensions, Skills, prompt templates, Pi Packages, external containers/tmux, or user-defined tooling.

WorkBuddy does the opposite: it integrates permissions, Plan mode, Skills, Connectors, MCP, Automation, remote control, Experts, and curated recipes directly into the product.

## 29.2 Model freedom

Both are strong on model choice, but in different ways.

- WorkBuddy gives ordinary users a GUI for built-in providers, custom API endpoints, and Ollama.
- Pi exposes a multi-provider agent runtime intended for technical users and can be embedded in other software through RPC/SDK modes.

Pi is therefore more hackable; WorkBuddy is more accessible.

## 29.3 Security philosophy

Pi does not impose permission popups in core. Its documentation suggests containers or custom extension-based gates, and warns that Pi packages can run with full system access. The user owns the security architecture.

WorkBuddy gives the user a default permission boundary and an explicit Full Access mode. That is safer for a mainstream audience but less minimal.

## 29.4 Ecosystem philosophy

Pi Packages can bundle extensions, Skills, prompt templates, and themes. Pi exposes a small core and lets communities build the rest.

WorkBuddy exposes a curated Skill marketplace, Experts, Explore, Connectors, MCP, Assistant, and Automation. It is a **platform product** rather than a toolkit that asks the user to compose everything.

## 29.5 Research recommendation

Pi is excellent when the benchmarker wants maximum harness control, provider flexibility, and source-level observability. WorkBuddy is excellent when the benchmarker wants to observe a commercial, integrated agent workstation as a real end user would experience it.

A particularly informative experiment would run the same model through Pi and WorkBuddy, holding model API parameters as constant as practical. That would isolate some of the value and cost of WorkBuddy’s orchestration layer.

---

# 30. Decision guide

Choose **WorkBuddy** when:

- the task mixes research, office artifacts, local files, scripts, and SaaS tools;
- non-developer usability matters;
- you want to switch among models or use local Ollama without changing the whole agent UX;
- messaging-based remote dispatch or workplace Connectors are important;
- you want a batteries-included desktop rather than assembling an agent stack.

Choose **Google Antigravity** when:

- the task is development-heavy and benefits from native worktrees/projects;
- dynamic subagents and Google’s unified desktop/CLI/SDK platform are important;
- Gemini-centric integration is acceptable or desired.

Choose **OpenAI Codex** when:

- software engineering and Git workflows are central but you also want rapidly expanding knowledge-work features;
- OpenAI’s app/CLI/IDE/cloud ecosystem and role-specific plugins are a good fit;
- worktree isolation and parallel agent supervision are valuable.

Choose **Claude Code** when:

- deep repository work and programmable developer automation dominate;
- you value a mature mix of CLI, desktop, web, MCP, Hooks, Skills, subagents, agent teams, and cloud/local scheduling;
- Claude models are the preferred foundation.

Choose **Grok Build** when:

- you want an open-source terminal harness;
- large parallel fan-out/workflows or Git worktrees are central;
- you want xAI/Grok-native coding and the ability to inspect the harness.

Choose **Pi** when:

- you want the smallest and most user-controlled harness;
- multi-provider freedom and source-level extensibility are more important than built-in conveniences;
- you are comfortable building your own permission, subagent, MCP, and orchestration layers.

---

# 31. Strengths of WorkBuddy

## 31.1 General-work orientation from first principles

WorkBuddy does not need to reinterpret a repository as the center of every task. A directory of PDFs, spreadsheets, notes, slide decks, images, and CSV files is a normal workspace rather than an awkward edge case.

## 31.2 Model plurality

The ability to select built-in models, Auto routing, custom APIs, and local Ollama gives WorkBuddy one of the broadest model-selection stories among mainstream graphical agent workstations.

## 31.3 Integrated ecosystem

Skills, Connectors, MCP, Experts, Explore, Automation, Assistant, and Memory are exposed as coherent product features instead of requiring the user to assemble separate tools.

## 31.4 Workplace remote control

Assistant’s direct support for numerous messaging systems is practical in organizations where work already flows through chat.

## 31.5 Strong Tencent vertical integration with Hy3

Hy3’s real-product feedback loop and WorkBuddy-specific deployment provide a plausible model–harness optimization advantage for the tasks Tencent cares about, even though independent evaluation is still necessary.

## 31.6 Enterprise path

WorkBuddy Enterprise and WMA provide a path from individual desktop usage toward centrally governed and cloud-hosted agent operation.

---

# 32. Limitations, uncertainties, and maturity risks

## 32.1 Product velocity

WorkBuddy is new and changing quickly. Official pages already show different skill counts and evolving UI/configuration vocabulary. A report written in August 2026 should not be assumed accurate months later.

## 32.2 Documentation overlap with CodeBuddy

The shared documentation portal can blur product boundaries. Researchers should avoid assuming every `.codebuddy` CLI/IDE behavior is a WorkBuddy desktop contract.

## 32.3 Vendor metrics are not independent evidence

Tencent’s Hy3/WorkBuddy success-rate and speed claims are valuable but should be replicated externally. The internal test set, scoring rubric, and failure taxonomy are not fully exposed in the corporate announcement.

## 32.4 Auto routing complicates attribution

A convenient Auto mode can silently undermine model-specific comparisons. Explicit model selection is preferable for this archive.

## 32.5 Memory complicates reproducibility

Default-on personalization means clean-room model comparisons require deliberate memory control.

## 32.6 Third-party integration expands the data boundary

The more useful WorkBuddy becomes through models, Skills, MCP, Connectors, and messaging, the more important it becomes to understand which service receives which content.

## 32.7 Closed desktop harness

Unlike Pi or the now-open Grok Build terminal harness, WorkBuddy desktop cannot be audited end-to-end from source. Researchers observe the product boundary rather than inspect all orchestration internals.

---

# 33. Recommended protocol for future WorkBuddy benchmark runs

For a fixed software specification in this archive, the following protocol would make WorkBuddy results substantially more interpretable.

## Phase A — environment freeze

1. Record WorkBuddy version/build and OS.
2. Record account region and tier.
3. Screenshot or text-record available models.
4. Select one explicit model; disable Auto unless Auto is the subject.
5. Record Hy3 thinking mode if available.
6. Disable or clear Memory for a clean benchmark.
7. Disable non-required Connectors, MCP servers, Experts, Explore recipes, and custom Skills.
8. Use a dedicated workspace cloned from a known input package.
9. Record permission mode.

## Phase B — run

1. Submit the fixed task specification without hidden manual simplification.
2. Allow the platform to plan and execute under the chosen mode.
3. Record every human approval/intervention.
4. Do not manually edit generated files during the primary run.
5. Preserve timestamps, credit/usage displays, model-selection changes, and agent/subagent activity if visible.

## Phase C — evidence capture

1. Export the complete conversation.
2. Generate a model-output-only corpus under a documented policy.
3. Separately count final implementation artifacts with Repomix or another reproducible counter to avoid double-counting code bodies embedded in chat.
4. Archive the final project tree without build/cache artifacts unless those are required evidence.
5. Run independent build/tests/review outside WorkBuddy.
6. Save a run README describing failures as well as successes.

## Phase D — interpretation

Separate four questions:

- **Did the final project satisfy the specification?**
- **How much model output / compute proxy did the run consume?**
- **How much human intervention was required?**
- **Which outcome should be attributed to model choice versus WorkBuddy harness/tooling?**

This avoids turning one end-to-end product score into an unsupported claim about foundation-model intelligence.

---

# 34. Glossary

**Agent harness** — the orchestration layer that assembles context, invokes the model, exposes tools, handles tool calls, manages task state, and decides how to continue execution.

**Artifact / deliverable** — a concrete output such as a document, spreadsheet, presentation, report, code change, or other file that can be inspected separately from the conversation.

**Auto routing** — WorkBuddy mode that selects a model according to the task rather than keeping one explicit foundation model fixed.

**Connector** — a productized integration to a known third-party service such as GitHub, Gmail, Jira, or Slack.

**Expert** — a WorkBuddy domain-specialized persona/configuration with tailored prompts/knowledge; not necessarily a different foundation model.

**Explore** — WorkBuddy’s curated/community recipes that combine prompts, Skills, and Experts into reusable examples.

**Hy3** — Tencent’s 2026 hybrid fast/slow-thinking MoE model, 295B total / 21B active parameters with up to 256K context.

**MCP** — Model Context Protocol, an open standard for exposing external tools/data to agents.

**MoE** — Mixture of Experts; a model architecture in which routing activates a subset of expert parameters for a given computation.

**Skill** — packaged instructions/capabilities/workflows that an agent can load or invoke.

**WMA** — WorkBuddy Managed Agents, Tencent’s managed cloud agent-runtime product under WorkBuddy Enterprise.

**Workspace** — the principal local folder/context boundary associated with a WorkBuddy task.

---

# 35. Source bibliography

The links below were reviewed for this profile on **2026-08-24** unless otherwise noted. Product documentation is listed before secondary analytical interpretation.

## 35.1 WorkBuddy / Tencent first-party sources

### Product and architecture

- Tencent Cloud, **WorkBuddy Enterprise — Product Overview**  
  <https://cloud.tencent.com/document/product/1831/134329>
- Tencent Cloud, **WorkBuddy — Product Introduction**  
  <https://cloud.tencent.com/document/product/1831/134384>
- Tencent Cloud, **WorkBuddy Enterprise — Product Advantages**  
  <https://cloud.tencent.com/document/product/1831/134330>
- Tencent Cloud, **WorkBuddy Managed Agents — Product Introduction**  
  <https://cloud.tencent.com/document/product/1831/134407>
- Tencent WorkBuddy Docs, **Task Bar / Ask, Craft, Plan, Skills, permissions, parallel tasks**  
  <https://www.workbuddy.ai/docs/zh/workbuddy/From-Beginner-to-Expert-Guide/Function-Description/Task-Bar>
- Tencent WorkBuddy Docs, **Permission Modes**  
  <https://www.workbuddy.ai/docs/workbuddy/From-Beginner-to-Expert-Guide/Function-Description/Permission-Modes>
- Tencent WorkBuddy Docs, **Model Configuration**  
  <https://www.workbuddy.ai/docs/workbuddy/From-Beginner-to-Expert-Guide/Function-Description/Model>

### Ecosystem and workflow

- Tencent WorkBuddy Docs, **MCP Integration**  
  <https://www.workbuddy.ai/docs/workbuddy/From-Beginner-to-Expert-Guide/Function-Description/MCP-Guide>
- Tencent WorkBuddy Docs, **Connectors**  
  <https://www.workbuddy.ai/docs/workbuddy/From-Beginner-to-Expert-Guide/Function-Description/Connector>
- Tencent WorkBuddy Docs, **Expert Center**  
  <https://www.workbuddy.ai/docs/workbuddy/From-Beginner-to-Expert-Guide/Function-Description/Expert-Center>
- Tencent WorkBuddy Docs, **Explore**  
  <https://www.workbuddy.ai/docs/workbuddy/From-Beginner-to-Expert-Guide/Function-Description/Explore>
- Tencent WorkBuddy Docs, **Automation**  
  <https://www.workbuddy.ai/docs/workbuddy/From-Beginner-to-Expert-Guide/Function-Description/Automation-Guide>
- Tencent WorkBuddy Docs, **Assistant Remote Control**  
  <https://www.workbuddy.ai/docs/workbuddy/From-Beginner-to-Expert-Guide/Function-Description/Assistant>
- Tencent WorkBuddy Docs, **Memory**  
  <https://www.workbuddy.ai/docs/workbuddy/From-Beginner-to-Expert-Guide/Function-Description/Memory>

### Privacy, plans, and installation

- Tencent, **Tencent WorkBuddy Privacy Policy** (last updated July 2026)  
  <https://www.workbuddy.ai/document/privacy-policy>
- Tencent, **Tencent WorkBuddy Service Agreement**  
  <https://www.workbuddy.ai/document/term>
- Tencent WorkBuddy Docs, **Pricing**  
  <https://www.workbuddy.ai/docs/workbuddy/pricing>
- Tencent WorkBuddy Docs, **Windows Installation Guide**  
  <https://www.workbuddy.ai/docs/workbuddy/From-Beginner-to-Expert-Guide/Installation-Win-Guide>
- Tencent WorkBuddy Docs, **Mac Installation Guide**  
  <https://www.workbuddy.ai/docs/workbuddy/From-Beginner-to-Expert-Guide/Installation-Mac-Guide>

### Hy3

- Tencent, **Tencent Unveils Hy3 preview; Model Enhances Agent Capabilities and Real-World Usability**, 2026-04-24  
  <https://www.tencent.com/tencent-unveils-hy3-preview-model-enhances-agent-capabilities-and-real-world-usability/>
- Tencent, **Tencent Hunyuan Officially Releases Hy3, Advancing Agent Capabilities and Deeper Product Integration**, 2026-07-06  
  <https://www.tencent.com/tencent-hunyuan-officially-releases-hy3-advancing-agent-capabilities-and-deeper-product-integration/>
- Tencent Cloud, **Hy3 preview Token Plan routing migration to Hy3**, documenting `no_think`, `think_low`, and `think_high`  
  <https://cloud.tencent.com/announce/detail/2384>
- Tencent, **Tencent Hy3 Now Available Globally, Extending Practical AI Across Products, Workflows and Cloud Services**, 2026-08-05  
  <https://www.tencent.com/tencent-hy3-now-available-globally-extending-practical-ai-across-products-workflows-and-cloud-services/>
- Tencent Cloud, **Hunyuan model product page / Hy3 specifications**  
  <https://cloud.tencent.com/product/tclm>

### WorkBuddy Bench

- Tencent, **WorkBuddy Bench official repository**  
  <https://github.com/Tencent/workbuddy-bench>

## 35.2 Google Antigravity first-party sources

- Google Antigravity, **Introducing Google Antigravity 2.0**, 2026-05-19  
  <https://www.antigravity.google/blog/introducing-google-antigravity-2>
- Google Antigravity, **Feature Overview**  
  <https://antigravity.google/docs/features>
- Google Antigravity, **Documentation Home / surfaces**  
  <https://antigravity.google/docs/home>
- Google Antigravity, **Subagents, Hooks, Scheduled Tasks, Agent Management, Voice, and Much More**, 2026-05-19  
  <https://antigravity.google/blog/google-io-2026-feature-deep-dive>
- Google Developers Blog, **Transitioning Gemini CLI to Antigravity CLI**, 2026-05-19  
  <https://developers.googleblog.com/en/an-important-update-transitioning-gemini-cli-to-antigravity-cli/>

## 35.3 OpenAI Codex first-party sources

- OpenAI, **Introducing the Codex app**, 2026-02-02  
  <https://openai.com/index/introducing-the-codex-app/>
- OpenAI, **Codex for every role, tool, and workflow**, 2026-06-02  
  <https://openai.com/index/codex-for-every-role-tool-workflow/>
- OpenAI, **Codex is becoming a productivity tool for everyone**, 2026-06-02  
  <https://openai.com/index/codex-for-knowledge-work/>
- OpenAI, **How agents are transforming work**, 2026  
  <https://openai.com/index/how-agents-are-transforming-work/>
- OpenAI, **Running Codex safely at OpenAI**, 2026-05-08  
  <https://openai.com/index/running-codex-safely/>

## 35.4 Anthropic Claude Code first-party sources

- Anthropic, **Claude Code overview**  
  <https://code.claude.com/docs/en/overview>
- Anthropic, **Extend Claude Code**  
  <https://code.claude.com/docs/en/features-overview>
- Anthropic, **Use Claude Code Desktop**  
  <https://code.claude.com/docs/en/desktop>
- Anthropic, **Schedule recurring tasks in Claude Code Desktop**  
  <https://code.claude.com/docs/en/desktop-scheduled-tasks>
- Anthropic, **Automate work with Routines**  
  <https://code.claude.com/docs/en/routines>

## 35.5 SpaceXAI / xAI Grok Build first-party sources

- SpaceXAI, **Introducing Grok Build**, 2026-05-25  
  <https://x.ai/news/grok-build-cli>
- SpaceXAI, **Grok Build is Now Open Source**, 2026-07-15  
  <https://x.ai/news/grok-build-open-source>
- SpaceXAI, **Workflows in Grok Build**, 2026-07-23  
  <https://x.ai/news/workflows>
- SpaceXAI, **Grok Build on web and mobile**, 2026-08-19  
  <https://x.ai/news/grok-build-for-everyone>
- SpaceXAI, **Grok Build changelog**  
  <https://x.ai/build/changelog>

## 35.6 Pi primary sources

- `badlogic/pi-mono`, **Pi monorepo**  
  <https://github.com/badlogic/pi-mono>
- `badlogic/pi-mono`, **Pi coding agent documentation**  
  <https://github.com/badlogic/pi-mono/tree/main/packages/coding-agent>
- `badlogic/pi-mono`, **Pi Packages**  
  <https://github.com/badlogic/pi-mono/blob/main/packages/coding-agent/docs/packages.md>

---

## 36. Final methodological takeaway

For this archive, the most accurate label is not simply **“WorkBuddy”** and not simply **“Hy3.”** A run is better represented as something like:

> **Tencent WorkBuddy desktop · version X · Hy3 `think_high` · Craft · Default Permissions · Memory off · no custom Skills/MCP/Connectors · Free/Pro tier · region Y**

That level of specificity acknowledges what modern agent systems actually are: a foundation model embedded in a harness, tool environment, permission system, memory layer, product UI, and service ecosystem.

WorkBuddy is particularly valuable to study because it makes those layers unusually visible and because Tencent is explicitly building both the model (Hy3) and the surrounding agent products. It is also a moving target. The correct research posture is therefore neither to dismiss it as “just another wrapper” nor to treat Tencent’s integrated end-to-end metrics as pure model benchmarks. The useful object of study is the **whole execution surface**, with its configuration recorded precisely enough that future runs can be understood and compared.
