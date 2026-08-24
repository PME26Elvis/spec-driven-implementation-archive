/*
 * WorkBuddy Conversation Exporter — MHTML-calibrated v1.0.0
 *
 * Paste this entire file into DevTools Console on an open WorkBuddy conversation.
 * It downloads TWO files:
 *   1) workbuddy-....md               Full conversation backup
 *   2) workbuddy-....output-only.txt  Tokenizer-ready model-output corpus
 *
 * Tokenizer corpus definition (compute-oriented, not billing-oriented):
 *   INCLUDE
 *     - visible WorkBuddy reasoning / deep-thinking prose
 *     - normal WorkBuddy-authored prose / final responses
 *     - raw terminal/execute-command text chosen by the model
 *   EXCLUDE
 *     - user messages
 *     - terminal stdout/stderr / command results
 *     - read-file/search results
 *     - write/edit-file bodies and diffs (repo contents are measured separately)
 *     - UI chrome, timestamps, tool status labels, file cards, downloads
 *
 * SAFETY
 *   - No generic button scanning.
 *   - No Download/Save/Export clicks.
 *   - Only exact WorkBuddy collapse headers inside .cr-agent are clicked when closed.
 *   - Only exact .cr-tool-head--collapsible heads are clicked when their content shell is hidden.
 *   - File write/edit details MAY be opened in the page for the user's MHTML backup, but their body
 *     is deliberately excluded from the tokenizer .txt.
 *   - Read-file and search/list-file tool contents are never added to the tokenizer .txt.
 *
 * Calibrated against the supplied 2026-08-17 WorkBuddy MHTML sample, which contained:
 *   .cr-self-message, .cr-agent, .cr-reasoning, .cr-collapse,
 *   .cr-tool-call__slot[data-tool-name], .cr-tool-exp, .cr-tool-head--collapsible,
 *   execute_command / read_file / search_content / write_to_file / replace_in_file.
 *
 * Expected edge cases:
 *   - Virtualized message list: snapshots are accumulated while sweeping the real conversation scroller.
 *   - Identical repeated model commands: preserved per tool-call id; not globally deduplicated.
 *   - Collapsed content already present in DOM: extracted even if a click cannot change state.
 *   - Unknown future tool types: preserved in Markdown as a tool block but excluded from tokenizer TXT
 *     unless they are structurally recognized as execute_command.
 *   - Huge write-file diffs may themselves be virtualized; this does not affect tokenizer TXT because
 *     repo file bodies are intentionally excluded.
 */
(async () => {
  'use strict';

  const CONFIG = Object.freeze({
    debug: true,
    expandAllConversationCollapses: true,
    expandToolDetailsForBackup: true,
    maxRuntimeMs: 6 * 60 * 1000,
    maxTopLoadPasses: 50,
    stableTopPasses: 3,
    maxSweepRounds: 4,
    stableSweepRounds: 2,
    maxSamplesPerRound: 140,
    sweepStepViewportRatio: 0.90,
    maxExpansionPassesPerViewport: 10,
    maxAttemptsPerStableDisclosure: 2,
    mutationQuietMs: 70,
    mutationMaxWaitMs: 450,
    postClickDelayMs: 24,
    restoreScrollPosition: true,
    filenamePrefix: 'workbuddy',
    exporterVersion: 'WorkBuddy Console Export — MHTML-calibrated v1.0.0',
  });

  const startedAt = Date.now();
  const sleep = ms => new Promise(resolve => setTimeout(resolve, ms));
  const log = (...args) => CONFIG.debug && console.log('[WorkBuddy Export]', ...args);
  const warn = (...args) => console.warn('[WorkBuddy Export]', ...args);

  const stats = {
    topLoadPasses: 0,
    sweepRounds: 0,
    samples: 0,
    collapseClicks: 0,
    toolDetailClicks: 0,
    uniqueDisclosureSignatures: new Set(),
    failedOrNoChange: 0,
    userTurnsSeen: new Set(),
    agentTurnsSeen: new Set(),
    centerEventsSeen: new Set(),
    reasoningFragments: 0,
    reasoningChars: 0,
    modelTextFragments: 0,
    modelTextChars: 0,
    commandFragments: 0,
    commandChars: 0,
    excludedWriteEditEvents: 0,
    excludedReadSearchEvents: 0,
    unknownToolEvents: 0,
    convergenceReached: false,
    runtimeLimitReached: false,
  };

  // Keeps per-message snapshots across virtualization.
  const frameStore = new Map();
  const disclosureAttempts = new Map();
  let firstSeenCounter = 0;

  function runtimeExceeded() {
    if (Date.now() - startedAt > CONFIG.maxRuntimeMs) {
      stats.runtimeLimitReached = true;
      return true;
    }
    return false;
  }

  function normalizeSpace(s) {
    return String(s || '')
      .normalize('NFKC')
      .replace(/\u00a0/g, ' ')
      .replace(/[\u200b\ufeff]/g, '')
      .replace(/\s+/g, ' ')
      .trim();
  }

  function cleanText(s) {
    return String(s || '')
      .replace(/\r\n?/g, '\n')
      .replace(/\u00a0/g, ' ')
      .replace(/[\u200b\ufeff]/g, '')
      .replace(/[ \t]+\n/g, '\n')
      .replace(/\n[ \t]+/g, '\n')
      .replace(/\n{4,}/g, '\n\n\n')
      .trim();
  }

  function hashString(input) {
    let h = 0x811c9dc5;
    const s = String(input || '');
    for (let i = 0; i < s.length; i += 1) {
      h ^= s.charCodeAt(i);
      h = Math.imul(h, 0x01000193);
    }
    return (h >>> 0).toString(36);
  }

  function sanitizeFilenamePart(value, fallback = 'conversation') {
    const cleaned = String(value || '')
      .normalize('NFKC')
      .replace(/[<>:"/\\|?*\x00-\x1F]/g, ' ')
      .replace(/\s+/g, ' ')
      .trim()
      .slice(0, 90);
    return cleaned || fallback;
  }

  function dynamicFence(text, min = 3) {
    const runs = String(text || '').match(/`+/g) || [];
    const longest = runs.reduce((m, r) => Math.max(m, r.length), 0);
    return '`'.repeat(Math.max(min, longest + 1));
  }

  function absoluteHref(raw) {
    if (!raw || /^javascript:/i.test(raw)) return '';
    try { return new URL(raw, location.href).href; } catch { return String(raw); }
  }

  function inlineCode(text) {
    const value = String(text || '').replace(/\n+/g, ' ');
    const fence = dynamicFence(value, 1);
    const body = /^\s|\s$/.test(value) ? ` ${value.trim()} ` : value;
    return `${fence}${body}${fence}`;
  }

  function childrenMarkdown(node, ctx) {
    return Array.from(node.childNodes || []).map(n => domToMarkdown(n, ctx)).join('');
  }

  function tableMarkdown(table) {
    const trs = Array.from(table.querySelectorAll('tr'));
    if (!trs.length) return '';
    const rows = trs.map(tr => Array.from(tr.children)
      .filter(c => /^(TH|TD)$/.test(c.tagName))
      .map(c => cleanText(childrenMarkdown(c, { inPre: false, listDepth: 0 }))
        .replace(/\|/g, '\\|')
        .replace(/\s*\n\s*/g, '<br>')));
    const width = Math.max(0, ...rows.map(r => r.length));
    if (!width) return '';
    rows.forEach(r => { while (r.length < width) r.push(''); });
    let hi = trs.findIndex(tr => tr.querySelector('th'));
    if (hi < 0) hi = 0;
    const header = rows[hi];
    const body = rows.filter((_, i) => i !== hi);
    const line = r => `| ${r.join(' | ')} |`;
    return `\n${line(header)}\n${line(new Array(width).fill('---'))}${body.length ? `\n${body.map(line).join('\n')}` : ''}\n\n`;
  }

  function listMarkdown(list, depth = 0) {
    const ordered = list.tagName === 'OL';
    const start = Number(list.getAttribute('start')) || 1;
    const items = Array.from(list.children).filter(el => el.tagName === 'LI');
    let out = '\n';
    items.forEach((li, i) => {
      const clone = li.cloneNode(true);
      clone.querySelectorAll(':scope > ul, :scope > ol').forEach(n => n.remove());
      const body = cleanText(childrenMarkdown(clone, { inPre: false, listDepth: depth })).replace(/\s*\n\s*/g, ' ');
      out += `${'  '.repeat(depth)}${ordered ? `${start + i}.` : '-'} ${body}\n`;
      Array.from(li.children).forEach(ch => {
        if (ch.tagName === 'UL' || ch.tagName === 'OL') out += listMarkdown(ch, depth + 1).replace(/^\n/, '');
      });
    });
    return `${out}\n`;
  }

  function domToMarkdown(node, ctx = { inPre: false, listDepth: 0 }) {
    if (!node) return '';
    if (node.nodeType === Node.TEXT_NODE) return ctx.inPre ? (node.nodeValue || '') : (node.nodeValue || '').replace(/[\t\r]+/g, ' ');
    if (node.nodeType !== Node.ELEMENT_NODE) return '';

    const el = node;
    const tag = el.tagName.toUpperCase();

    // Explicit UI chrome only; do not blanket-remove role=button because file/path links may be meaningful.
    if (el.matches('script,style,noscript,template,svg,.cr-toolbar,.cr-agent__header,.cr-collapse__header,.cr-tool-head')) return '';

    switch (tag) {
      case 'BR': return '\n';
      case 'HR': return '\n\n---\n\n';
      case 'P': return `${childrenMarkdown(el, ctx).trim()}\n\n`;
      case 'STRONG': case 'B': return `**${childrenMarkdown(el, ctx).trim()}**`;
      case 'EM': case 'I': return `*${childrenMarkdown(el, ctx).trim()}*`;
      case 'DEL': case 'S': return `~~${childrenMarkdown(el, ctx).trim()}~~`;
      case 'CODE': {
        if (ctx.inPre) return el.getAttribute('data-content') || el.textContent || '';
        return inlineCode(el.getAttribute('data-content') || el.textContent || '');
      }
      case 'PRE': {
        const code = el.querySelector('code');
        const raw = code?.getAttribute('data-content') || code?.textContent || el.textContent || '';
        const classes = `${code?.className || ''} ${el.className || ''}`;
        const lang = (classes.match(/(?:language|lang)-([a-z0-9_+.-]+)/i) || [])[1] || '';
        const fence = dynamicFence(raw, 3);
        return `\n${fence}${lang}\n${raw.replace(/\n$/, '')}\n${fence}\n\n`;
      }
      case 'A': {
        const label = cleanText(childrenMarkdown(el, ctx)) || el.getAttribute('title') || '';
        const href = absoluteHref(el.getAttribute('href'));
        if (!href) return label;
        if (!label || label === href) return `<${href}>`;
        return `[${label}](<${href.replace(/>/g, '%3E')}>)`;
      }
      case 'IMG': {
        const src = absoluteHref(el.currentSrc || el.getAttribute('src'));
        const alt = String(el.getAttribute('alt') || '').replace(/([\[\]])/g, '\\$1');
        return src ? `![${alt}](<${src.replace(/>/g, '%3E')}>)` : (alt ? `[Image: ${alt}]` : '');
      }
      case 'BLOCKQUOTE': {
        const body = cleanText(childrenMarkdown(el, ctx));
        return body ? `\n${body.split('\n').map(l => `> ${l}`).join('\n')}\n\n` : '';
      }
      case 'UL': case 'OL': return listMarkdown(el, ctx.listDepth || 0);
      case 'TABLE': return tableMarkdown(el);
      default:
        if (/^H[1-6]$/.test(tag)) return `${'#'.repeat(Number(tag[1]))} ${childrenMarkdown(el, ctx).trim()}\n\n`;
        return childrenMarkdown(el, ctx);
    }
  }

  function markdownOf(el) {
    return cleanText(domToMarkdown(el.cloneNode(true)));
  }

  function plainOf(el) {
    if (!el) return '';
    const clone = el.cloneNode(true);
    clone.querySelectorAll('script,style,noscript,template,svg,.cr-toolbar,.cr-collapse__header,.cr-tool-head').forEach(n => n.remove());
    clone.querySelectorAll('br').forEach(br => br.replaceWith('\n'));
    clone.querySelectorAll('p,div,li,h1,h2,h3,h4,h5,h6,pre,blockquote,tr').forEach(n => {
      n.insertAdjacentText('beforebegin', '\n');
      n.insertAdjacentText('afterend', '\n');
    });
    return cleanText(clone.textContent || '');
  }

  function getFrames() {
    const frames = Array.from(document.querySelectorAll('.cr-message-list .cr-frame, .cr-message-list__content .cr-frame'));
    if (frames.length) return frames;
    return Array.from(document.querySelectorAll('.cr-frame'));
  }

  function frameKind(frame) {
    if (frame.querySelector(':scope .cr-self-message')) return 'user';
    if (frame.querySelector(':scope .cr-agent')) return 'agent';
    if (frame.classList.contains('cr-frame--center')) return 'event';
    return 'unknown';
  }

  function frameId(frame) {
    return frame.getAttribute('data-cr-frame-id')
      || frame.querySelector('[data-message-id]')?.getAttribute('data-message-id')
      || frame.querySelector('[data-user-message-id]')?.getAttribute('data-user-message-id')
      || `fallback-${frameKind(frame)}-${hashString(normalizeSpace(frame.textContent).slice(0, 8000))}`;
  }

  function stableDisclosureSignature(el, kind) {
    // Do NOT use a broad closest([data-page-node-id]) ancestor: in the supplied WorkBuddy
    // MHTML most descendants share the same page-root id, which would collapse hundreds of
    // independent disclosures into one false signature.
    const ownPnid = el.getAttribute('data-page-node-id');
    if (ownPnid) return `${kind}:pnid:${ownPnid}`;

    const frame = el.closest('.cr-frame');
    const fid = frame ? frameId(frame) : 'noframe';
    const slot = el.closest('.cr-tool-call__slot');
    const toolId = slot?.getAttribute('data-tool-call-id') || '';
    if (toolId) return `${kind}:tool:${fid}:${toolId}`;

    const section = el.closest('section.cr-collapse');
    if (frame && section) {
      const siblings = Array.from(frame.querySelectorAll('section.cr-collapse'));
      const ordinal = siblings.indexOf(section);
      const title = normalizeSpace(section.querySelector(':scope > .cr-collapse__header .cr-collapse__title')?.textContent || '');
      const content = Array.from(section.children).find(ch => ch.classList?.contains('cr-collapse__content'));
      const preview = normalizeSpace(content?.textContent || '').slice(0, 260);
      return `${kind}:collapse:${fid}:${ordinal}:${hashString(title + '|' + preview)}`;
    }

    const text = normalizeSpace(el.textContent).slice(0, 220);
    return `${kind}:${fid}:${hashString(text)}`;
  }

  function isConversationCollapseClosed(section) {
    if (!section?.classList?.contains('cr-collapse')) return false;
    const content = Array.from(section.children).find(ch => ch.classList?.contains('cr-collapse__content'));
    return section.classList.contains('cr-collapse--collapsed') || content?.getAttribute('aria-hidden') === 'true';
  }

  function isToolDetailClosed(head) {
    const exp = head?.closest('.cr-tool-exp');
    if (!exp) return false;
    const shell = Array.from(exp.children).find(ch => ch.classList?.contains('cr-tool-exp__content-shell'));
    if (!shell) return false;
    return !head.classList.contains('cr-tool-head--expanded') || shell.getAttribute('aria-hidden') === 'true';
  }

  function safeDisclosureTargets() {
    const targets = [];
    if (CONFIG.expandAllConversationCollapses) {
      document.querySelectorAll('.cr-agent section.cr-collapse.cr-collapse--collapsed').forEach(section => {
        const header = Array.from(section.children).find(ch => ch.classList?.contains('cr-collapse__header'));
        if (header && isConversationCollapseClosed(section)) targets.push({ clickEl: header, stateEl: section, kind: 'collapse' });
      });
    }
    if (CONFIG.expandToolDetailsForBackup) {
      document.querySelectorAll('.cr-agent .cr-tool-head.cr-tool-head--collapsible').forEach(head => {
        if (isToolDetailClosed(head)) targets.push({ clickEl: head, stateEl: head, kind: 'tool' });
      });
    }
    return targets;
  }

  async function waitForMutationQuiet(target) {
    return new Promise(resolve => {
      let quiet = null;
      let max = null;
      const finish = () => {
        if (quiet) clearTimeout(quiet);
        if (max) clearTimeout(max);
        observer.disconnect();
        resolve();
      };
      const reset = () => {
        if (quiet) clearTimeout(quiet);
        quiet = setTimeout(finish, CONFIG.mutationQuietMs);
      };
      const observer = new MutationObserver(reset);
      try {
        observer.observe(target === document.scrollingElement ? document.body : target, { childList: true, subtree: true, attributes: true, attributeFilter: ['class', 'aria-hidden'] });
      } catch { resolve(); return; }
      max = setTimeout(finish, CONFIG.mutationMaxWaitMs);
      reset();
    });
  }

  async function expandSafeDisclosures(scroller) {
    let totalClicks = 0;
    for (let pass = 0; pass < CONFIG.maxExpansionPassesPerViewport && !runtimeExceeded(); pass += 1) {
      const targets = safeDisclosureTargets();
      if (!targets.length) break;
      let clicked = 0;
      for (const t of targets) {
        // Only interact with currently rendered controls. Parent collapses are opened first;
        // nested controls become visible on the next pass. This avoids dispatching synthetic
        // clicks into display:none subtrees.
        if (!t.clickEl.isConnected || t.clickEl.getClientRects().length === 0) continue;
        const sig = stableDisclosureSignature(t.clickEl, t.kind);
        stats.uniqueDisclosureSignatures.add(sig);
        const attempts = disclosureAttempts.get(sig) || 0;
        if (attempts >= CONFIG.maxAttemptsPerStableDisclosure) continue;
        disclosureAttempts.set(sig, attempts + 1);

        const beforeClosed = t.kind === 'collapse' ? isConversationCollapseClosed(t.stateEl) : isToolDetailClosed(t.stateEl);
        if (!beforeClosed) continue;
        try {
          t.clickEl.click();
          clicked += 1;
          totalClicks += 1;
          if (t.kind === 'collapse') stats.collapseClicks += 1;
          else stats.toolDetailClicks += 1;
          await sleep(CONFIG.postClickDelayMs);
          const afterClosed = t.kind === 'collapse' ? isConversationCollapseClosed(t.stateEl) : isToolDetailClosed(t.stateEl);
          if (afterClosed) stats.failedOrNoChange += 1;
        } catch {
          stats.failedOrNoChange += 1;
        }
      }
      if (!clicked) break;
      await waitForMutationQuiet(scroller);
    }
    return totalClicks;
  }

  function findConversationScroller(frames) {
    // WorkBuddy's explicit viewport gets first priority when actually scrollable.
    const explicit = document.querySelector('.cr-message-list-viewport');
    if (explicit && explicit.scrollHeight > explicit.clientHeight + 40) {
      log('Using explicit .cr-message-list-viewport scroller', {
        scrollHeight: explicit.scrollHeight, clientHeight: explicit.clientHeight,
      });
      return explicit;
    }

    const scores = new Map();
    for (const f of frames) {
      let cur = f.parentElement;
      let depth = 0;
      while (cur && cur !== document.body && depth < 22) {
        const cs = getComputedStyle(cur);
        if (/(auto|scroll|overlay)/i.test(cs.overflowY) && cur.scrollHeight > cur.clientHeight + 40) {
          const rec = scores.get(cur) || { hits: 0, ratio: 0 };
          rec.hits += 1;
          rec.ratio = Math.max(rec.ratio, cur.scrollHeight / Math.max(1, cur.clientHeight));
          scores.set(cur, rec);
        }
        cur = cur.parentElement;
        depth += 1;
      }
    }
    if (scores.size) {
      const [chosen, meta] = Array.from(scores.entries()).sort((a, b) => b[1].hits - a[1].hits || b[1].ratio - a[1].ratio)[0];
      log('Using inferred conversation scroller', { hits: meta.hits, ratio: meta.ratio, className: String(chosen.className || '') });
      return chosen;
    }
    warn('No inner scroller detected; falling back to document scroller.');
    return document.scrollingElement || document.documentElement;
  }

  function getScrollTop(scroller) {
    if ([document.scrollingElement, document.documentElement, document.body].includes(scroller)) return window.scrollY || document.scrollingElement?.scrollTop || 0;
    return scroller.scrollTop;
  }
  function setScrollTop(scroller, value) {
    if ([document.scrollingElement, document.documentElement, document.body].includes(scroller)) window.scrollTo(0, value);
    else scroller.scrollTop = value;
  }
  function metrics(scroller) {
    if ([document.scrollingElement, document.documentElement, document.body].includes(scroller)) {
      const root = document.scrollingElement || document.documentElement;
      return { top: getScrollTop(root), height: root.scrollHeight, client: window.innerHeight };
    }
    return { top: scroller.scrollTop, height: scroller.scrollHeight, client: scroller.clientHeight };
  }

  function diffMarkdown(slot) {
    const lines = Array.from(slot.querySelectorAll('.cr-tool-diff__line'));
    if (!lines.length) return '';
    const text = lines.map(line => {
      const sign = line.querySelector('.cr-tool-diff__sign')?.textContent || '';
      const code = line.querySelector('.cr-tool-diff__code')?.textContent || '';
      return `${sign}${code}`;
    }).join('\n');
    const fence = dynamicFence(text, 3);
    return `${fence}diff\n${text}\n${fence}`;
  }

  function extractToolUnit(slot) {
    const name = slot.getAttribute('data-tool-name') || 'unknown';
    const callId = slot.getAttribute('data-tool-call-id') || '';
    const head = slot.querySelector('.cr-tool-head');
    const primary = slot.querySelector('.cr-tool-head__primary');
    const secondary = slot.querySelector('.cr-tool-head__secondary');
    const status = slot.getAttribute('data-tool-status') || '';

    let md = `### Tool · ${name}\n\n`;
    const title = normalizeSpace(primary?.textContent || '');
    const secondaryText = normalizeSpace(secondary?.textContent || '');
    if (title) md += `**Target:** ${title}${secondaryText ? ` — ${secondaryText}` : ''}\n\n`;

    let outputText = '';
    let outputKind = 'excluded';

    if (name === 'execute_command') {
      const cmdEl = slot.querySelector('.cr-tool-exec__command');
      const fallbackCommand = primary?.getAttribute('title') || '';
      const command = cleanText(cmdEl?.textContent || fallbackCommand);
      if (command) {
        const fence = dynamicFence(command, 3);
        md += `${fence}bash\n${command}\n${fence}\n\n`;
        outputText = command;
        outputKind = 'command';
      }
      // Any other command result/status is retained only in Markdown, never tokenizer TXT.
      const resultCandidates = Array.from(slot.querySelectorAll('.cr-tool-exec__output,.cr-tool-exec__result,.cr-tool-exec__stderr,.cr-tool-exec__stdout'));
      if (resultCandidates.length) {
        const result = cleanText(resultCandidates.map(n => n.textContent || '').join('\n'));
        if (result) {
          const fence = dynamicFence(result, 3);
          md += `**Tool result:**\n\n${fence}text\n${result}\n${fence}\n\n`;
        }
      }
    } else if (name === 'write_to_file' || name === 'replace_in_file') {
      const diff = diffMarkdown(slot);
      if (diff) md += `${diff}\n\n`;
    } else if (name === 'read_file' || name === 'search_content' || name === 'list_files') {
      const body = slot.querySelector('.cr-tool-exp__content');
      const bodyText = cleanText(body?.textContent || '');
      if (bodyText) {
        const fence = dynamicFence(bodyText, 3);
        md += `${fence}text\n${bodyText}\n${fence}\n\n`;
      }
    } else {
      const body = slot.querySelector('.cr-tool-exp__content');
      const bodyMd = body ? markdownOf(body) : '';
      if (bodyMd) md += `${bodyMd}\n\n`;
    }

    if (status) md += `*Tool status: ${status}*\n`;
    return { key: `tool:${callId || hashString(normalizeSpace(slot.textContent))}`, type: 'tool', md: cleanText(md), outputText, outputKind };
  }

  function extractReasoningUnit(reasoning, index) {
    // WorkBuddy's .cr-reasoning contains one inner .cr-collapse titled "深度思考".
    const content = reasoning.querySelector('.cr-collapse__content-inner') || reasoning;
    const md = markdownOf(content);
    const text = plainOf(content);
    return {
      key: `reasoning:${reasoning.getAttribute('data-page-node-id') || index}:${hashString(text.slice(0, 400))}`,
      type: 'reasoning',
      md: md ? `### Thinking\n\n${md}` : '',
      outputText: text,
      outputKind: 'reasoning',
    };
  }

  function extractModelTextUnit(block, index) {
    const md = markdownOf(block);
    const text = plainOf(block);
    return {
      key: `model-text:${block.getAttribute('data-page-node-id') || index}:${hashString(text.slice(0, 400))}`,
      type: 'model-text', md, outputText: text, outputKind: 'model-text',
    };
  }

  function collectAgentUnits(agent) {
    const candidates = [];
    agent.querySelectorAll('.cr-reasoning').forEach((el, i) => candidates.push({ el, kind: 'reasoning', i }));
    agent.querySelectorAll('.cr-tool-call__slot').forEach((el, i) => candidates.push({ el, kind: 'tool', i }));
    agent.querySelectorAll('.cr-text-block').forEach((el, i) => {
      if (el.closest('.cr-tool-call__slot') || el.closest('.cr-reasoning')) return;
      candidates.push({ el, kind: 'model-text', i });
    });

    candidates.sort((a, b) => {
      if (a.el === b.el) return 0;
      const p = a.el.compareDocumentPosition(b.el);
      if (p & Node.DOCUMENT_POSITION_FOLLOWING) return -1;
      if (p & Node.DOCUMENT_POSITION_PRECEDING) return 1;
      return 0;
    });

    return candidates.map(c => c.kind === 'reasoning'
      ? extractReasoningUnit(c.el, c.i)
      : c.kind === 'tool'
        ? extractToolUnit(c.el)
        : extractModelTextUnit(c.el, c.i));
  }

  function extractFrame(frame) {
    const id = frameId(frame);
    const kind = frameKind(frame);
    const orderHint = frame.getBoundingClientRect().top + (window.scrollY || 0);

    if (kind === 'user') {
      const content = frame.querySelector('.cr-self-message__content') || frame.querySelector('.cr-self-message');
      const md = content ? markdownOf(content) : '';
      stats.userTurnsSeen.add(id);
      return { id, kind, md, outputParts: [], orderHint };
    }

    if (kind === 'agent') {
      const agent = frame.querySelector('.cr-agent');
      const units = collectAgentUnits(agent);
      stats.agentTurnsSeen.add(id);
      return { id, kind, units, orderHint };
    }

    if (kind === 'event') {
      const text = cleanText(frame.textContent || '');
      stats.centerEventsSeen.add(id);
      return { id, kind, md: text, outputParts: [], orderHint };
    }

    return { id, kind: 'unknown', md: markdownOf(frame), outputParts: [], orderHint };
  }

  function mergeFrame(record) {
    const existing = frameStore.get(record.id);
    if (!existing) {
      frameStore.set(record.id, { ...record, firstSeen: firstSeenCounter++ });
      return true;
    }

    let changed = false;
    if (record.kind === 'agent') {
      const byKey = new Map((existing.units || []).map(u => [u.key, u]));
      for (const unit of record.units || []) {
        const old = byKey.get(unit.key);
        if (!old) { byKey.set(unit.key, unit); changed = true; }
        else {
          // Expansion may reveal a richer Markdown representation later.
          if ((unit.md || '').length > (old.md || '').length) { old.md = unit.md; changed = true; }
          if ((unit.outputText || '').length > (old.outputText || '').length) { old.outputText = unit.outputText; changed = true; }
        }
      }
      existing.units = Array.from(byKey.values());
    } else if ((record.md || '').length > (existing.md || '').length) {
      existing.md = record.md;
      changed = true;
    }
    existing.orderHint = Math.min(existing.orderHint ?? Infinity, record.orderHint ?? Infinity);
    return changed;
  }

  function captureSnapshot() {
    stats.samples += 1;
    let changed = false;
    getFrames().forEach(frame => { if (mergeFrame(extractFrame(frame))) changed = true; });
    return changed;
  }

  async function loadTop(scroller) {
    let stable = 0;
    let lastSig = '';
    for (let pass = 0; pass < CONFIG.maxTopLoadPasses && stable < CONFIG.stableTopPasses && !runtimeExceeded(); pass += 1) {
      stats.topLoadPasses += 1;
      setScrollTop(scroller, 0);
      await waitForMutationQuiet(scroller);
      await expandSafeDisclosures(scroller);
      captureSnapshot();
      const m = metrics(scroller);
      const sig = `${Math.round(m.height)}:${frameStore.size}:${stats.agentTurnsSeen.size}:${stats.userTurnsSeen.size}`;
      stable = sig === lastSig ? stable + 1 : 0;
      lastSig = sig;
      log(`top-load pass ${pass + 1}`, { signature: sig, stable });
      await sleep(60);
    }
  }

  async function sweep(scroller) {
    let stableRounds = 0;
    let lastSignature = '';
    for (let round = 0; round < CONFIG.maxSweepRounds && stableRounds < CONFIG.stableSweepRounds && !runtimeExceeded(); round += 1) {
      stats.sweepRounds += 1;
      let changedThisRound = false;
      let clicksThisRound = 0;

      setScrollTop(scroller, 0);
      await waitForMutationQuiet(scroller);

      for (let sample = 0; sample < CONFIG.maxSamplesPerRound && !runtimeExceeded(); sample += 1) {
        clicksThisRound += await expandSafeDisclosures(scroller);
        if (captureSnapshot()) changedThisRound = true;
        const m = metrics(scroller);
        const maxTop = Math.max(0, m.height - m.client);
        if (m.top >= maxTop - 2) break;
        const step = Math.max(140, Math.floor(m.client * CONFIG.sweepStepViewportRatio));
        const next = Math.min(maxTop, m.top + step);
        if (next <= m.top + 1) break;
        setScrollTop(scroller, next);
        await waitForMutationQuiet(scroller);
        await sleep(30);
      }

      clicksThisRound += await expandSafeDisclosures(scroller);
      if (captureSnapshot()) changedThisRound = true;

      const totalUnits = Array.from(frameStore.values()).reduce((n, f) => n + (f.units?.length || 0), 0);
      const totalChars = Array.from(frameStore.values()).reduce((n, f) => n + (f.md?.length || 0) + (f.units || []).reduce((s, u) => s + (u.md?.length || 0), 0), 0);
      const signature = `${frameStore.size}:${totalUnits}:${totalChars}`;
      log(`sweep round ${round + 1}`, { signature, clicksThisRound, changedThisRound });

      if (signature === lastSignature && !changedThisRound && clicksThisRound === 0) stableRounds += 1;
      else stableRounds = 0;
      lastSignature = signature;

      setScrollTop(scroller, 0);
      await waitForMutationQuiet(scroller);
    }
    stats.convergenceReached = !stats.runtimeLimitReached && stableRounds >= CONFIG.stableSweepRounds;
  }

  function orderedFrames() {
    return Array.from(frameStore.values()).sort((a, b) => a.firstSeen - b.firstSeen);
  }

  function buildMarkdown() {
    const frames = orderedFrames();
    const now = new Date();
    let md = '# WorkBuddy Conversation Export\n\n';
    md += `**Exported:** ${now.toLocaleString()}  \n`;
    md += `**Frames:** ${frames.length} (User ${stats.userTurnsSeen.size} / WorkBuddy ${stats.agentTurnsSeen.size} / Events ${stats.centerEventsSeen.size})  \n`;
    md += `**URL:** ${location.href}  \n`;
    md += `**Export Method:** ${CONFIG.exporterVersion}\n\n`;
    md += '> Conversation-oriented backup. Tool results and file edits are retained when they exist in the rendered DOM; very large virtualized file diffs may be partial. The companion .output-only.txt applies the tokenizer filtering policy.\n\n---\n\n';

    for (const frame of frames) {
      if (frame.kind === 'user') {
        md += `## Human\n\n${frame.md || ''}\n\n---\n\n`;
      } else if (frame.kind === 'agent') {
        md += '## WorkBuddy\n\n';
        for (const unit of frame.units || []) {
          if (unit.md) md += `${unit.md}\n\n`;
        }
        md += '---\n\n';
      } else if (frame.kind === 'event') {
        md += `## System Event\n\n${frame.md || ''}\n\n---\n\n`;
      } else if (frame.md) {
        md += `## Unknown Frame\n\n${frame.md}\n\n---\n\n`;
      }
    }
    return md.trimEnd() + '\n';
  }

  function buildOutputOnlyText() {
    const parts = [];
    stats.reasoningFragments = 0; stats.reasoningChars = 0;
    stats.modelTextFragments = 0; stats.modelTextChars = 0;
    stats.commandFragments = 0; stats.commandChars = 0;
    stats.excludedWriteEditEvents = 0;
    stats.excludedReadSearchEvents = 0;
    stats.unknownToolEvents = 0;

    for (const frame of orderedFrames()) {
      if (frame.kind !== 'agent') continue;
      for (const unit of frame.units || []) {
        if (unit.type === 'tool') {
          const key = unit.key || '';
          // The key itself does not carry the tool name, so inspect the rendered Markdown header
          // for diagnostics only. Classification for inclusion still comes from outputKind.
          if (/^### Tool · (write_to_file|replace_in_file)/m.test(unit.md || '')) stats.excludedWriteEditEvents += 1;
          else if (/^### Tool · (read_file|search_content|list_files)/m.test(unit.md || '')) stats.excludedReadSearchEvents += 1;
          else if (unit.outputKind !== 'command' && /^### Tool · /m.test(unit.md || '')) stats.unknownToolEvents += 1;
        }
        const text = cleanText(unit.outputText || '');
        if (!text) continue;
        if (unit.outputKind === 'reasoning') {
          parts.push(text); stats.reasoningFragments += 1; stats.reasoningChars += text.length;
        } else if (unit.outputKind === 'model-text') {
          parts.push(text); stats.modelTextFragments += 1; stats.modelTextChars += text.length;
        } else if (unit.outputKind === 'command') {
          parts.push(text); stats.commandFragments += 1; stats.commandChars += text.length;
        }
      }
    }
    // No headings/category labels are added: this file is intended to go directly into a tokenizer.
    return parts.join('\n\n').trim() + '\n';
  }

  function download(content, filename, mime) {
    const blob = new Blob([content], { type: `${mime};charset=utf-8` });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = filename;
    a.style.display = 'none';
    document.body.appendChild(a);
    a.click();
    a.remove();
    setTimeout(() => URL.revokeObjectURL(url), 30000);
  }

  function baseFilename() {
    let title = document.title || '';
    const firstUser = orderedFrames().find(f => f.kind === 'user');
    if (!title || /^Task Share$/i.test(title)) title = firstUser?.md?.replace(/[#*_`>\[\]()]/g, ' ').slice(0, 90) || 'conversation';
    return sanitizeFilenamePart(title).replace(/\s+/g, '-');
  }

  const initialFrames = getFrames();
  if (!initialFrames.length) throw new Error('No WorkBuddy .cr-frame conversation DOM found on this page.');
  const scroller = findConversationScroller(initialFrames);
  const originalTop = getScrollTop(scroller);

  log('Starting safe exhaustive capture.', {
    initialFrames: initialFrames.length,
    initialCollapsed: document.querySelectorAll('.cr-agent section.cr-collapse.cr-collapse--collapsed').length,
    initialClosedToolDetails: Array.from(document.querySelectorAll('.cr-agent .cr-tool-head.cr-tool-head--collapsible')).filter(isToolDetailClosed).length,
  });

  try {
    await loadTop(scroller);
    await sweep(scroller);

    const md = buildMarkdown();
    const txt = buildOutputOnlyText();
    const stamp = new Date().toISOString().replace(/[:.]/g, '-');
    const base = baseFilename();
    const mdName = `${CONFIG.filenamePrefix}-${base}-${stamp}.md`;
    const txtName = `${CONFIG.filenamePrefix}-${base}-${stamp}.output-only.txt`;

    download(md, mdName, 'text/markdown');
    // Stagger two downloads slightly to avoid browsers coalescing/suppressing simultaneous synthetic clicks.
    await sleep(180);
    download(txt, txtName, 'text/plain');

    const summary = {
      files: { markdown: mdName, tokenizerText: txtName },
      frames: { total: frameStore.size, user: stats.userTurnsSeen.size, workbuddy: stats.agentTurnsSeen.size, events: stats.centerEventsSeen.size },
      tokenizerCorpus: {
        reasoning: { fragments: stats.reasoningFragments, chars: stats.reasoningChars },
        modelProseAndFinalResponses: { fragments: stats.modelTextFragments, chars: stats.modelTextChars },
        executeCommands: { fragments: stats.commandFragments, chars: stats.commandChars },
        totalChars: txt.length,
        excludedWriteEditEvents: stats.excludedWriteEditEvents,
        excludedReadSearchEvents: stats.excludedReadSearchEvents,
        unknownToolEventsExcluded: stats.unknownToolEvents,
      },
      expansion: {
        uniqueDisclosureSignatures: stats.uniqueDisclosureSignatures.size,
        conversationCollapseClicks: stats.collapseClicks,
        toolDetailClicks: stats.toolDetailClicks,
        failedOrNoChange: stats.failedOrNoChange,
      },
      sweep: {
        topLoadPasses: stats.topLoadPasses,
        rounds: stats.sweepRounds,
        samples: stats.samples,
        convergenceReached: stats.convergenceReached,
        runtimeLimitReached: stats.runtimeLimitReached,
      },
    };

    window.__workBuddyExportV1 = { markdown: md, outputOnlyText: txt, summary, frames: orderedFrames() };
    console.log('[WorkBuddy Export] DONE');
    console.table({
      'User turns': summary.frames.user,
      'WorkBuddy turns': summary.frames.workbuddy,
      'System events': summary.frames.events,
      'Reasoning chars': summary.tokenizerCorpus.reasoning.chars,
      'Model prose chars': summary.tokenizerCorpus.modelProseAndFinalResponses.chars,
      'Command chars': summary.tokenizerCorpus.executeCommands.chars,
      'Tokenizer TXT chars': summary.tokenizerCorpus.totalChars,
      'Write/edit events excluded from TXT': summary.tokenizerCorpus.excludedWriteEditEvents,
      'Read/search events excluded from TXT': summary.tokenizerCorpus.excludedReadSearchEvents,
      'Collapse clicks': summary.expansion.conversationCollapseClicks,
      'Tool-detail clicks': summary.expansion.toolDetailClicks,
      'Converged': summary.sweep.convergenceReached,
      'Runtime limit hit': summary.sweep.runtimeLimitReached,
    });
    console.log('[WorkBuddy Export] Full summary:', summary);
    console.log('[WorkBuddy Export] In-memory result: window.__workBuddyExportV1');
    if (!stats.convergenceReached) warn('Strict convergence was not reached. Files were still generated; inspect summary and optionally Save As MHTML after expansion.');
  } finally {
    if (CONFIG.restoreScrollPosition) {
      try { setScrollTop(scroller, originalTop); } catch {}
    }
  }
})();
