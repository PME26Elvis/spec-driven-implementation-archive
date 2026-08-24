/*
 * Grok Console Markdown Export — MHTML-Calibrated Safe Exhaustive Refactor
 * Version: 1.4.0-mhtml-calibrated-safe
 *
 * Paste this entire file into DevTools Console on an open Grok conversation.
 * No extension/userscript manager is required. It immediately downloads Markdown.
 *
 * Design goals:
 * - Semantic/structural message discovery with layered fallbacks.
 * - Structural speaker detection first; alternating fallback only when the DOM is ambiguous.
 * - Preserve rendered Markdown: links, headings, lists, tables, blockquotes, code fences,
 *   inline code, emphasis, images, and line breaks.
 * - Actively open ONLY exact Grok disclosures observed in the calibration MHTML: canvas/reasoning, Bash command, Write file, Edited files, and explicitly-collapsed Read file controls.
 * - Preserve Think/reasoning blocks separately from the final answer when present.
 * - Preserve Think/Fun/DeepSearch mode indicators when detectable from DOM/badges.
 * - Handle lazy loading and virtualized long chats by accumulating snapshots while sweeping.
 * - Avoid duplicate UI/control text and normalize relative links to absolute URLs.
 * - Restore the user's original scroll position after capture.
 *
 * This is an independent robust rewrite informed by the MIT-licensed Enhanced Grok Export
 * project supplied by the user. See README-console-refactor.md in the delivery bundle.
 */
(async () => {
  'use strict';

  const CONFIG = Object.freeze({
    debug: true,
    loadOlder: true,
    expandReasoningDisclosures: true,

    // v1.2 deliberately has NO global click-count safety ceiling.
    // The old 400/600 style budget could stop a valid long sandbox/reasoning trace early.
    // Safety now comes from per-signature attempt limits + convergence + runtime limits.
    maxReasoningExpansionPassesPerViewport: 10,
    maxStatefulAttemptsPerSignature: 2,
    maxStatelessAttemptsPerSignature: 1,
    reasoningClickDelayMs: 18,

    // Aggressive conversation loading / virtualization sweep.
    maxTopLoadPasses: 60,
    stableTopPasses: 4,
    sweepVirtualizedConversation: true,
    maxSweepRounds: 10,
    stableSweepRounds: 2,
    maxSweepSamplesPerRound: 220,
    sweepStepViewportRatio: 0.68,
    maxRuntimeMs: 6 * 60 * 1000,

    mutationQuietMs: 160,
    mutationMaxWaitMs: 950,
    restoreScrollPosition: true,
    maxMessageChars: 500000,
    minMessageChars: 1,
    filenamePrefix: 'grok',
    exportMethod: 'Grok Console Export — MHTML-calibrated safe exhaustive v1.4.0',
  });

  const log = (...args) => CONFIG.debug && console.log('[Grok Export Exhaustive]', ...args);
  const warn = (...args) => console.warn('[Grok Export Exhaustive]', ...args);
  const sleep = ms => new Promise(resolve => setTimeout(resolve, ms));

  const UI_REMOVE_SELECTOR = [
    'script', 'style', 'noscript', 'template',
    'button', 'input', 'textarea', 'select',
    '[role="button"]', '[role="menu"]', '[role="menuitem"]', '[role="tooltip"]',
    '[aria-hidden="true"]',
    '.action-buttons',
    '[data-testid*="copy" i]', '[data-testid*="feedback" i]', '[data-testid*="share" i]',
    '[aria-label*="copy" i]', '[aria-label*="feedback" i]', '[aria-label*="share" i]',
  ].join(',');

  const REASONING_SELECTOR = [
    '.thinking-container',
    '[class*="thinking-container" i]',
    '[class*="reasoning" i]',
    '[data-testid*="thinking" i]',
    '[data-testid*="reasoning" i]',
    '[data-component*="thinking" i]',
    '[data-component*="reasoning" i]',
  ].join(',');

  const CONTENT_SELECTOR = '.response-content-markdown, [data-message-content], [data-testid="message-content"]';

  // v1.4 safety rule: there is NO generic button/[role=button]/[aria-expanded] scan.
  // These exact semantics were observed in the supplied 2026-08-14 Grok MHTML.
  // A control is clicked only when its state is explicitly CLOSED. Unknown/stateless controls
  // are never clicked. This prevents accidental Download/CSV/ZIP/Share/Copy actions.
  const SAFE_DISCLOSURE_SPECS = Object.freeze([
    { kind: 'reasoning-canvas', selector: '[data-testid="canvas-trigger"]' },
    { kind: 'bash-command', selector: '[aria-label="Bash command"]' },
    { kind: 'write-file', selector: '[aria-label="Write file"]' },
    { kind: 'edited-files', selector: '[aria-label="Edited files"]' },
    // Read-file controls in the calibration MHTML were non-clickable cursor-default DIVs.
    // If a future UI exposes one as an explicit closed disclosure, opening it is safe for
    // archival completeness; otherwise it is ignored.
    { kind: 'read-file', selector: '[aria-label="Read file"]' },
  ]);
  const DISCLOSURE_SELECTOR = SAFE_DISCLOSURE_SPECS.map(x => x.selector).join(',');

  function sanitizeFilenamePart(value, fallback = 'untitled') {
    const cleaned = String(value || '')
      .normalize('NFKC')
      .replace(/[<>:"/\\|?*\x00-\x1F]/g, ' ')
      .replace(/\s+/g, ' ')
      .trim()
      .slice(0, 100);
    return cleaned || fallback;
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

  function normalizeWhitespaceForKey(text) {
    return String(text || '').replace(/\s+/g, ' ').trim();
  }

  function normalizeHref(href) {
    if (!href) return '';
    const raw = String(href).trim();
    if (!raw || /^javascript:/i.test(raw)) return '';
    try {
      return new URL(raw, location.href).href;
    } catch {
      return raw;
    }
  }

  function markdownLinkTarget(url) {
    const safe = String(url || '').replace(/>/g, '%3E').replace(/</g, '%3C');
    return `<${safe}>`;
  }

  function dynamicBacktickFence(text, minimum = 3) {
    const runs = String(text || '').match(/`+/g) || [];
    const longest = runs.reduce((max, run) => Math.max(max, run.length), 0);
    return '`'.repeat(Math.max(minimum, longest + 1));
  }

  function inlineCode(text) {
    const value = String(text || '').replace(/\n+/g, ' ');
    const fence = dynamicBacktickFence(value, 1);
    const padded = /^\s|\s$/.test(value) ? ` ${value.trim()} ` : value;
    return `${fence}${padded}${fence}`;
  }

  function cleanMarkdown(md) {
    return String(md || '')
      .replace(/\u00a0/g, ' ')
      .replace(/[ \t]+\n/g, '\n')
      .replace(/\n{4,}/g, '\n\n\n')
      .replace(/^\s+|\s+$/g, '')
      .trim();
  }

  function childMarkdown(node, ctx) {
    return Array.from(node.childNodes || []).map(child => domToMarkdown(child, ctx)).join('');
  }

  function textCellMarkdown(cell) {
    return cleanMarkdown(childMarkdown(cell, { inPre: false, listDepth: 0 }))
      .replace(/\|/g, '\\|')
      .replace(/\s*\n\s*/g, '<br>');
  }

  function tableToMarkdown(table) {
    const rowNodes = Array.from(table.querySelectorAll('tr'));
    if (!rowNodes.length) return '';
    const rows = rowNodes.map(row => Array.from(row.children)
      .filter(cell => /^(TH|TD)$/.test(cell.tagName))
      .map(textCellMarkdown));
    const width = Math.max(0, ...rows.map(row => row.length));
    if (!width) return '';
    rows.forEach(row => { while (row.length < width) row.push(''); });

    let headerIndex = rowNodes.findIndex(row => row.querySelector('th'));
    if (headerIndex < 0) headerIndex = 0;
    const header = rows[headerIndex];
    const body = rows.filter((_, i) => i !== headerIndex);
    const line = cells => `| ${cells.join(' | ')} |`;
    return `\n${line(header)}\n${line(new Array(width).fill('---'))}${body.length ? `\n${body.map(line).join('\n')}` : ''}\n\n`;
  }

  function listToMarkdown(list, depth = 0) {
    const ordered = list.tagName === 'OL';
    const start = Number(list.getAttribute('start')) || 1;
    const items = Array.from(list.children).filter(el => el.tagName === 'LI');
    let out = '\n';

    items.forEach((li, index) => {
      const clone = li.cloneNode(true);
      clone.querySelectorAll(':scope > ul, :scope > ol').forEach(n => n.remove());
      const body = cleanMarkdown(childMarkdown(clone, { inPre: false, listDepth: depth }))
        .replace(/\s*\n\s*/g, ' ');
      const indent = '  '.repeat(depth);
      const marker = ordered ? `${start + index}.` : '-';
      out += `${indent}${marker} ${body}\n`;
      Array.from(li.children).forEach(child => {
        if (child.tagName === 'UL' || child.tagName === 'OL') out += listToMarkdown(child, depth + 1).replace(/^\n/, '');
      });
    });

    return `${out}\n`;
  }

  function domToMarkdown(node, ctx = { inPre: false, listDepth: 0 }) {
    if (!node) return '';

    if (node.nodeType === Node.TEXT_NODE) {
      if (ctx.inPre) return node.nodeValue || '';
      return (node.nodeValue || '').replace(/[\t\r]+/g, ' ');
    }
    if (node.nodeType !== Node.ELEMENT_NODE) return '';

    const el = node;
    const tag = el.tagName.toUpperCase();

    if (tag === 'GROK-EXPORT-MARKER') {
      const phase = el.getAttribute('data-phase') || 'POINT';
      const kind = el.getAttribute('data-kind') || 'unknown';
      const subtype = el.getAttribute('data-subtype') || '';
      const extra = subtype ? ` subtype=${subtype}` : '';
      return `\n<!-- GROK_EXPORT:${phase} kind=${kind}${extra} -->\n`;
    }

    if (el.matches(UI_REMOVE_SELECTOR)) return '';

    switch (tag) {
      case 'BR': return '\n';
      case 'HR': return '\n\n---\n\n';
      case 'P': return `${childMarkdown(el, ctx).trim()}\n\n`;
      case 'STRONG':
      case 'B': return `**${childMarkdown(el, ctx).trim()}**`;
      case 'EM':
      case 'I': return `*${childMarkdown(el, ctx).trim()}*`;
      case 'DEL':
      case 'S': return `~~${childMarkdown(el, ctx).trim()}~~`;
      case 'MARK': return `==${childMarkdown(el, ctx).trim()}==`;
      case 'KBD': return `<kbd>${childMarkdown(el, ctx).trim()}</kbd>`;
      case 'SUB': return `<sub>${childMarkdown(el, ctx).trim()}</sub>`;
      case 'SUP': return `<sup>${childMarkdown(el, ctx).trim()}</sup>`;
      case 'CODE': {
        if (ctx.inPre) return el.textContent || '';
        return inlineCode(el.textContent || '');
      }
      case 'PRE': {
        const code = el.querySelector('code');
        const raw = (code ? code.textContent : el.textContent) || '';
        const classText = `${code?.className || ''} ${el.className || ''}`;
        const lang = (classText.match(/(?:language|lang)-([a-z0-9_+.-]+)/i) || [])[1] || '';
        const fence = dynamicBacktickFence(raw, 3);
        return `\n${fence}${lang}\n${raw.replace(/\n$/, '')}\n${fence}\n\n`;
      }
      case 'A': {
        const href = normalizeHref(el.getAttribute('href'));
        let label = cleanMarkdown(childMarkdown(el, ctx));
        if (!label) label = el.getAttribute('aria-label') || el.getAttribute('title') || href;
        if (!href) return label;
        if (!label || label === href) return `<${href}>`;
        const title = (el.getAttribute('title') || '').replace(/"/g, '\\"').trim();
        return `[${label}](${markdownLinkTarget(href)}${title ? ` "${title}"` : ''})`;
      }
      case 'IMG': {
        const src = normalizeHref(el.currentSrc || el.getAttribute('src'));
        const alt = String(el.getAttribute('alt') || '').replace(/([\[\]])/g, '\\$1');
        if (!src) return alt ? `[Image: ${alt}]` : '';
        if (/^data:/i.test(src) && src.length > 4096) return alt ? `[Embedded image: ${alt}]` : '[Embedded image]';
        return `![${alt}](${markdownLinkTarget(src)})`;
      }
      case 'BLOCKQUOTE': {
        const body = cleanMarkdown(childMarkdown(el, ctx));
        return body ? `\n${body.split('\n').map(line => `> ${line}`).join('\n')}\n\n` : '';
      }
      case 'UL':
      case 'OL': return listToMarkdown(el, ctx.listDepth || 0);
      case 'TABLE': return tableToMarkdown(el);
      case 'DL': {
        const parts = [];
        for (const child of el.children) {
          if (child.tagName === 'DT') parts.push(`**${cleanMarkdown(childMarkdown(child, ctx))}**`);
          if (child.tagName === 'DD') parts.push(`: ${cleanMarkdown(childMarkdown(child, ctx))}`);
        }
        return `\n${parts.join('\n')}\n\n`;
      }
      default:
        if (/^H[1-6]$/.test(tag)) {
          return `${'#'.repeat(Number(tag[1]))} ${childMarkdown(el, ctx).trim()}\n\n`;
        }
        return childMarkdown(el, ctx);
    }
  }

  function closestGroupRow(element) {
    let cur = element;
    for (let depth = 0; cur && depth < 8; depth += 1, cur = cur.parentElement) {
      if (cur.classList?.contains('group/row')) return cur;
    }
    return null;
  }

  function semanticEventRoot(control) {
    const row = closestGroupRow(control);
    return row?.parentElement || row || null;
  }

  function markerElement(doc, phase, kind, subtype = '') {
    const marker = doc.createElement('grok-export-marker');
    marker.setAttribute('data-phase', phase);
    marker.setAttribute('data-kind', kind);
    if (subtype) marker.setAttribute('data-subtype', subtype);
    return marker;
  }

  function wrapSemanticNode(node, kind, subtype = '') {
    if (!node?.parentNode || node.getAttribute?.('data-grok-export-wrapped') === '1') return;
    node.setAttribute?.('data-grok-export-wrapped', '1');
    node.parentNode.insertBefore(markerElement(node.ownerDocument, 'BEGIN', kind, subtype), node);
    node.parentNode.insertBefore(markerElement(node.ownerDocument, 'END', kind, subtype), node.nextSibling);
  }

  function annotateSemanticRegions(clone) {
    // Reasoning prose rows (lightbulb "思考結果" in the calibration UI).
    clone.querySelectorAll('[aria-label="思考結果"], [aria-label="Thinking result"], [aria-label="Reasoning result"]').forEach(control => {
      const row = closestGroupRow(control);
      if (row) wrapSemanticNode(row, 'model_reasoning');
    });

    // Bash: exact tool-call input vs tool-return output. The calibration DOM renders
    // both under a group/shell container; the command <pre> owns a desktop-shell-command span.
    clone.querySelectorAll('[aria-label="Bash command"]').forEach(control => {
      const row = closestGroupRow(control);
      if (row) wrapSemanticNode(row, 'ui_event', 'bash_header');
      const root = semanticEventRoot(control);
      if (!root) return;
      const shell = Array.from(root.querySelectorAll('div')).find(div => div.classList?.contains('group/shell'));
      if (!shell) return;
      const pres = Array.from(shell.querySelectorAll('pre'));
      if (!pres.length) return;
      const commandPre = pres.find(pre => pre.querySelector('[class*="desktop-shell-command"]')) || pres[0];
      wrapSemanticNode(commandPre, 'model_tool_input', 'bash');
      pres.filter(pre => pre !== commandPre).forEach(pre => wrapSemanticNode(pre, 'tool_result', 'bash'));
    });

    // Write-file bodies are model-authored tool input. Expanded Edited-files groups in the
    // calibration MHTML contain nested Write-file events; those are handled here as well.
    clone.querySelectorAll('[aria-label="Write file"]').forEach(control => {
      const row = closestGroupRow(control);
      if (row) wrapSemanticNode(row, 'ui_event', 'write_file_header');
      const root = semanticEventRoot(control);
      if (!root) return;
      const pres = Array.from(root.querySelectorAll('pre')).filter(pre => !pre.closest('[class*="group/shell"]'));
      const codePres = pres.filter(pre => pre.querySelector('code') || /shiki/i.test(pre.className || ''));
      (codePres.length ? codePres : pres.slice(0, 1)).forEach(pre => wrapSemanticNode(pre, 'model_tool_input', 'write_file'));
    });

    // Group header only. Nested Write-file bodies are independently annotated above.
    clone.querySelectorAll('[aria-label="Edited files"]').forEach(control => {
      const row = closestGroupRow(control);
      if (row) wrapSemanticNode(row, 'ui_event', 'edited_files_header');
    });

    // Read-file is tool-return/input context, never model-generated output. Mark the complete
    // event root so the post-processor can exclude filenames/content/status text deterministically.
    clone.querySelectorAll('[aria-label="Read file"]').forEach(control => {
      const root = semanticEventRoot(control);
      if (root) wrapSemanticNode(root, 'tool_result', 'read_file');
    });

    return clone;
  }

  function cloneAndSanitize(root, removeReasoning = false) {
    const clone = annotateSemanticRegions(root.cloneNode(true));
    clone.querySelectorAll(UI_REMOVE_SELECTOR).forEach(el => el.remove());
    if (removeReasoning) clone.querySelectorAll(REASONING_SELECTOR).forEach(el => el.remove());
    return clone;
  }

  function getReasoningRoots(messageEl) {
    const roots = Array.from(messageEl.querySelectorAll(REASONING_SELECTOR))
      .filter(el => normalizeWhitespaceForKey(el.textContent).length > 0);
    // Keep outermost reasoning containers so nested wrappers are not exported twice.
    return roots.filter(el => !roots.some(other => other !== el && other.contains(el)));
  }

  function markdownFromRoots(roots) {
    return cleanMarkdown(roots.map(root => domToMarkdown(cloneAndSanitize(root))).join('\n\n'));
  }

  function extractVisibleStructure(messageEl) {
    const reasoningRoots = getReasoningRoots(messageEl);
    const reasoning = markdownFromRoots(reasoningRoots);

    let contentRoots = Array.from(messageEl.querySelectorAll(CONTENT_SELECTOR));
    contentRoots = contentRoots.filter(root => !reasoningRoots.some(reasoningRoot => reasoningRoot.contains(root)));
    contentRoots = contentRoots.filter(root => !contentRoots.some(other => other !== root && root.contains(other)));

    let response = '';
    if (contentRoots.length) {
      response = markdownFromRoots(contentRoots);
    } else {
      response = cleanMarkdown(domToMarkdown(cloneAndSanitize(messageEl, true)));
    }

    if (reasoning && response && normalizeWhitespaceForKey(response).includes(normalizeWhitespaceForKey(reasoning))) {
      response = cleanMarkdown(domToMarkdown(cloneAndSanitize(messageEl, true)));
    }

    return { reasoning, response };
  }

  function canonicalizeCandidate(el) {
    if (!el || !(el instanceof Element)) return null;
    return el.closest('.message-bubble') ||
      el.closest('[data-message-author-role]') ||
      el.closest('[data-testid*="message" i]') ||
      el;
  }

  function isPlausibleMessage(el) {
    if (!el || !el.isConnected) return false;
    const text = normalizeWhitespaceForKey(el.textContent);
    if (text.length < CONFIG.minMessageChars || text.length > CONFIG.maxMessageChars) return false;
    if (el.matches('nav,header,footer,aside,[role="dialog"],[role="menu"]')) return false;
    return true;
  }

  function domSort(elements) {
    return [...elements].sort((a, b) => {
      if (a === b) return 0;
      const pos = a.compareDocumentPosition(b);
      if (pos & Node.DOCUMENT_POSITION_FOLLOWING) return -1;
      if (pos & Node.DOCUMENT_POSITION_PRECEDING) return 1;
      return 0;
    });
  }

  function getMessageCandidates() {
    const buckets = [];
    const addAll = nodeList => {
      for (const node of nodeList) {
        const canonical = canonicalizeCandidate(node);
        if (canonical) buckets.push(canonical);
      }
    };

    addAll(document.querySelectorAll('[data-message-author-role="user"], [data-message-author-role="assistant"]'));
    addAll(document.querySelectorAll('.message-bubble'));
    addAll(document.querySelectorAll('.response-content-markdown'));
    addAll(document.querySelectorAll('[data-testid*="message" i]'));

    let unique = domSort([...new Set(buckets)]).filter(isPlausibleMessage);

    // Remove broad ancestors that merely contain multiple more-specific message candidates.
    unique = unique.filter(el => {
      const contained = unique.filter(other => other !== el && el.contains(other));
      return contained.length < 2;
    });

    if (unique.length) return unique;

    // Last-resort structural fallback. Keep only relatively leaf-like text containers.
    const fallback = Array.from(document.querySelectorAll('main div[dir="auto"], main div[dir="ltr"]'))
      .filter(isPlausibleMessage)
      .filter(el => el.querySelectorAll('div[dir="auto"],div[dir="ltr"]').length < 3);
    return domSort(fallback);
  }

  function collectSemanticHints(el) {
    const hints = [];
    let cur = el;
    for (let depth = 0; cur && depth < 5; depth += 1, cur = cur.parentElement) {
      for (const attr of ['data-message-author-role', 'data-author', 'data-role', 'aria-label', 'title']) {
        const value = cur.getAttribute?.(attr);
        if (value) hints.push(`${attr}:${value}`);
      }
      if (typeof cur.className === 'string') hints.push(`class:${cur.className}`);
    }
    return hints.join(' | ').toLowerCase();
  }

  function disclosureState(element) {
    if (!element) return 'unknown';
    if (element.tagName === 'SUMMARY') {
      const details = element.closest('details');
      if (details) return details.open ? 'open' : 'closed';
    }

    const ariaExpanded = element.getAttribute('aria-expanded');
    if (ariaExpanded === 'true') return 'open';
    if (ariaExpanded === 'false') return 'closed';

    const dataState = (element.getAttribute('data-state') || '').toLowerCase();
    if (/^(open|opened|expanded)$/.test(dataState)) return 'open';
    if (/^(closed|collapsed)$/.test(dataState)) return 'closed';

    const dataExpanded = (element.getAttribute('data-expanded') || '').toLowerCase();
    if (/^(true|1|open|expanded)$/.test(dataExpanded)) return 'open';
    if (/^(false|0|closed|collapsed)$/.test(dataExpanded)) return 'closed';

    return 'unknown';
  }

  function messageShellForDisclosure(element) {
    return element.closest('.message-bubble') ||
      element.closest('[data-message-author-role]') ||
      element.closest('[data-testid*="message" i]') ||
      element.closest('article') ||
      null;
  }

  function compactElementHints(element) {
    const chunks = [];
    const pushAttrs = node => {
      if (!node?.getAttribute) return;
      for (const attr of [
        'aria-label', 'title', 'data-testid', 'data-test-id', 'data-component',
        'data-state', 'data-role', 'data-purpose', 'data-type'
      ]) {
        const value = node.getAttribute(attr);
        if (value) chunks.push(`${attr}:${value}`);
      }
      if (typeof node.className === 'string' && node.className) chunks.push(`class:${node.className}`);
    };

    pushAttrs(element);
    let parent = element.parentElement;
    for (let i = 0; parent && i < 3; i += 1, parent = parent.parentElement) pushAttrs(parent);

    const iconNodes = element.querySelectorAll('svg,[data-lucide],[class*="icon" i],[aria-label]');
    for (const icon of Array.from(iconNodes).slice(0, 8)) {
      for (const attr of ['data-lucide', 'aria-label', 'class']) {
        const value = icon.getAttribute?.(attr);
        if (value) chunks.push(`icon-${attr}:${value}`);
      }
    }

    const label = normalizeWhitespaceForKey(element.getAttribute('aria-label') || element.getAttribute('title') || element.textContent);
    if (label && label.length <= 220) chunks.push(`text:${label}`);
    return chunks.join(' | ');
  }

  function isBeforeNode(a, b) {
    if (!a || !b || a === b) return false;
    return Boolean(a.compareDocumentPosition(b) & Node.DOCUMENT_POSITION_FOLLOWING);
  }

  function safeDisclosureKind(element) {
    for (const spec of SAFE_DISCLOSURE_SPECS) {
      try { if (element.matches(spec.selector)) return spec.kind; } catch {}
    }
    return null;
  }

  function disclosureSignature(element, shell, kind) {
    const shellStable = shell?.getAttribute('data-message-id') || shell?.getAttribute('data-id') || shell?.id || '';
    const responseText = normalizeWhitespaceForKey(shell?.querySelector(CONTENT_SELECTOR)?.textContent || shell?.textContent || '').slice(-420);
    const ordinal = shell ? Array.from(shell.querySelectorAll(DISCLOSURE_SELECTOR)).indexOf(element) : -1;
    return `${shellStable || hashString(responseText)}|${kind}|${ordinal}`;
  }

  function makeReasoningExpansionStats() {
    return {
      candidateScans: 0,
      uniqueSignatures: new Set(),
      clicks: 0,
      closedClicks: 0,
      statelessClicks: 0,
      successfulExpansions: 0,
      failedOrNoChange: 0,
      passCount: 0,
      sweepRounds: 0,
      convergenceReached: false,
      runtimeLimitReached: false,
      attempts: new Map(),
      successes: new Set(),
      rejected: new Set(),
      clicksByKind: new Map(),
      successesByKind: new Map(),
    };
  }

  function mapToObject(map) {
    return Object.fromEntries(Array.from(map.entries()).sort((a, b) => a[0].localeCompare(b[0])));
  }

  function publicReasoningExpansionStats(stats) {
    return {
      candidateScans: stats.candidateScans,
      uniqueDisclosuresSeen: stats.uniqueSignatures.size,
      clicks: stats.clicks,
      closedClicks: stats.closedClicks,
      statelessClicks: stats.statelessClicks,
      successfulExpansions: stats.successfulExpansions,
      failedOrNoChange: stats.failedOrNoChange,
      clicksByKind: mapToObject(stats.clicksByKind),
      successesByKind: mapToObject(stats.successesByKind),
      passCount: stats.passCount,
      sweepRounds: stats.sweepRounds,
      convergenceReached: stats.convergenceReached,
      runtimeLimitReached: stats.runtimeLimitReached,
      safetyPolicy: 'exact-known-selectors + explicit-closed-state-only; no generic button clicking',
    };
  }

  function findReasoningDisclosureCandidates(stats) {
    const candidates = [];
    const seenElements = new Set();

    for (const spec of SAFE_DISCLOSURE_SPECS) {
      for (const element of document.querySelectorAll(spec.selector)) {
        if (!(element instanceof HTMLElement) || seenElements.has(element)) continue;
        seenElements.add(element);
        stats.candidateScans += 1;

        const state = disclosureState(element);
        if (state !== 'closed') continue; // critical v1.4 safety invariant

        const shell = messageShellForDisclosure(element);
        if (!shell) continue;

        const kind = safeDisclosureKind(element) || spec.kind;
        const signature = disclosureSignature(element, shell, kind);
        stats.uniqueSignatures.add(signature);
        if (stats.successes.has(signature) || stats.rejected.has(signature)) continue;

        const attempts = stats.attempts.get(signature) || 0;
        if (attempts >= CONFIG.maxStatefulAttemptsPerSignature) continue;
        candidates.push({ element, state, signature, shell, kind });
      }
    }
    return candidates;
  }

  function expansionMetric(candidate) {
    const shell = candidate.shell;
    const element = candidate.element;
    return {
      state: disclosureState(element),
      elementText: normalizeWhitespaceForKey(element.textContent || '').length,
      shellText: normalizeWhitespaceForKey(shell?.textContent || '').length,
      shellElements: shell?.querySelectorAll('*').length || 0,
      scrollHeight: shell instanceof HTMLElement ? shell.scrollHeight : 0,
    };
  }

  function metricShowsExpansion(before, after) {
    if (before.state !== 'open' && after.state === 'open') return true;
    if (after.shellText > before.shellText + 24) return true;
    if (after.shellElements > before.shellElements + 2) return true;
    if (after.scrollHeight > before.scrollHeight + 16) return true;
    return false;
  }

  async function expandVisibleReasoningDisclosures(scroller, stats) {
    if (!CONFIG.expandReasoningDisclosures) return 0;
    let successfulThisCall = 0;

    for (let pass = 0; pass < CONFIG.maxReasoningExpansionPassesPerViewport; pass += 1) {
      const candidates = findReasoningDisclosureCandidates(stats);
      if (!candidates.length) break;
      stats.passCount += 1;
      let attemptedThisPass = 0;

      for (const candidate of candidates) {
        if (!candidate.element.isConnected) continue;
        const currentState = disclosureState(candidate.element);
        if (currentState === 'open') {
          stats.successes.add(candidate.signature);
          continue;
        }

        stats.attempts.set(candidate.signature, (stats.attempts.get(candidate.signature) || 0) + 1);
        const before = expansionMetric(candidate);
        attemptedThisPass += 1;
        try {
          if (candidate.element.tagName === 'SUMMARY') {
            const details = candidate.element.closest('details');
            if (details && !details.open) {
              details.open = true;
              details.dispatchEvent(new Event('toggle', { bubbles: false }));
            }
          } else {
            candidate.element.click();
          }
          stats.clicks += 1;
          stats.closedClicks += 1;
          stats.clicksByKind.set(candidate.kind, (stats.clicksByKind.get(candidate.kind) || 0) + 1);
          await sleep(CONFIG.reasoningClickDelayMs);
          await waitForMutationQuiet(candidate.shell || scroller);
          const after = expansionMetric(candidate);
          if (metricShowsExpansion(before, after)) {
            stats.successfulExpansions += 1;
            stats.successes.add(candidate.signature);
            stats.successesByKind.set(candidate.kind, (stats.successesByKind.get(candidate.kind) || 0) + 1);
            successfulThisCall += 1;
          } else {
            const attempts = stats.attempts.get(candidate.signature) || 0;
            const limit = CONFIG.maxStatefulAttemptsPerSignature;
            if (attempts >= limit) {
              stats.rejected.add(candidate.signature);
              stats.failedOrNoChange += 1;
            }
          }
        } catch (error) {
          const attempts = stats.attempts.get(candidate.signature) || 0;
          const limit = CONFIG.maxStatefulAttemptsPerSignature;
          if (attempts >= limit) stats.rejected.add(candidate.signature);
          log('Disclosure click failed', error, candidate.element);
        }
      }

      if (!attemptedThisPass) break;
      await waitForMutationQuiet(scroller);
      await sleep(CONFIG.reasoningClickDelayMs);
    }

    return successfulThisCall;
  }

  function detectSpeaker(element, index, previousSpeaker = null) {
    const directRole = element.getAttribute('data-message-author-role') || element.closest('[data-message-author-role]')?.getAttribute('data-message-author-role');
    if (/^(user|human)$/i.test(directRole || '')) return { speaker: 'Human', confidence: 'high', reason: 'data-message-author-role' };
    if (/^(assistant|grok)$/i.test(directRole || '')) return { speaker: 'Grok', confidence: 'high', reason: 'data-message-author-role' };

    if (element.closest('.items-end')) return { speaker: 'Human', confidence: 'high', reason: 'items-end structural alignment' };
    if (element.closest('.items-start')) return { speaker: 'Grok', confidence: 'medium', reason: 'items-start structural alignment' };

    const hints = collectSemanticHints(element);
    if (/\b(user|human|you|your message)\b/.test(hints) && !/assistant/.test(hints)) {
      return { speaker: 'Human', confidence: 'medium', reason: 'semantic attribute/class hint' };
    }
    if (/\b(assistant|grok|model response)\b/.test(hints)) {
      return { speaker: 'Grok', confidence: 'medium', reason: 'semantic attribute/class hint' };
    }

    const className = typeof element.className === 'string' ? element.className : '';
    if (className.includes('bg-surface-l1') || element.querySelector('[class*="bg-surface-l1"]')) {
      return { speaker: 'Human', confidence: 'medium', reason: 'upstream bg-surface-l1 heuristic' };
    }
    if (className.includes('max-w-none') && !className.includes('bg-surface-l1')) {
      return { speaker: 'Grok', confidence: 'medium', reason: 'upstream max-w-none heuristic' };
    }

    // Avoid content-style/length guessing. Alternation is deterministic and less semantically biased.
    const speaker = previousSpeaker ? (previousSpeaker === 'Human' ? 'Grok' : 'Human') : (index % 2 === 0 ? 'Human' : 'Grok');
    return { speaker, confidence: 'low', reason: 'alternating structural fallback' };
  }

  function smallModeBadgeText(element) {
    const texts = [];
    const nodes = element.querySelectorAll('[aria-label], [title], span, div');
    for (const node of nodes) {
      const text = normalizeWhitespaceForKey(node.getAttribute?.('aria-label') || node.getAttribute?.('title') || node.textContent);
      if (text && text.length <= 40) texts.push(text);
      if (texts.length > 200) break;
    }
    return texts.join(' | ');
  }

  function detectMode(element, structure) {
    if (structure.reasoning) return { mode: 'think', confidence: 'high', reason: 'visible reasoning/thinking block' };

    const hints = `${collectSemanticHints(element)} | ${smallModeBadgeText(element)}`.toLowerCase();
    if (/deep\s*search|deepsearch/.test(hints)) return { mode: 'deepsearch', confidence: 'high', reason: 'DeepSearch DOM/badge hint' };
    if (/fun\s*mode/.test(hints)) return { mode: 'fun', confidence: 'medium', reason: 'Fun Mode DOM/badge hint' };
    if (/thinking|think\s*mode|reasoning/.test(hints)) return { mode: 'think', confidence: 'medium', reason: 'Think/reasoning DOM/badge hint' };
    return { mode: 'standard', confidence: 'medium', reason: 'no mode indicator detected' };
  }

  function getScrollTop(scroller) {
    if (scroller === document.scrollingElement || scroller === document.documentElement || scroller === document.body) {
      return window.scrollY || document.scrollingElement?.scrollTop || 0;
    }
    return scroller.scrollTop;
  }

  function setScrollTop(scroller, value) {
    if (scroller === document.scrollingElement || scroller === document.documentElement || scroller === document.body) {
      window.scrollTo(0, value);
    } else {
      scroller.scrollTop = value;
    }
  }

  function getScrollMetrics(scroller) {
    if (scroller === document.scrollingElement || scroller === document.documentElement || scroller === document.body) {
      const root = document.scrollingElement || document.documentElement;
      return { scrollTop: getScrollTop(root), scrollHeight: root.scrollHeight, clientHeight: window.innerHeight };
    }
    return { scrollTop: scroller.scrollTop, scrollHeight: scroller.scrollHeight, clientHeight: scroller.clientHeight };
  }

  function findScrollContainer(candidates) {
    // New Grok sandbox UI is an app shell with multiple independent scrollers.
    // Score every scrollable ancestor by how many message candidates it contains,
    // rather than assuming document.body/window is the conversation scroller.
    const score = new Map();
    for (const candidate of candidates) {
      let cur = candidate.parentElement;
      let depth = 0;
      while (cur && cur !== document.body && depth < 20) {
        const style = getComputedStyle(cur);
        const scrollable = /(auto|scroll|overlay)/i.test(style.overflowY) && cur.scrollHeight > cur.clientHeight + 80;
        if (scrollable) {
          const entry = score.get(cur) || { hits: 0, ratio: 0, area: 0 };
          entry.hits += 1;
          entry.ratio = Math.max(entry.ratio, cur.scrollHeight / Math.max(1, cur.clientHeight));
          const r = cur.getBoundingClientRect();
          entry.area = Math.max(entry.area, Math.max(0, r.width) * Math.max(0, r.height));
          score.set(cur, entry);
        }
        cur = cur.parentElement;
        depth += 1;
      }
    }
    if (score.size) {
      const ranked = Array.from(score.entries()).sort((a, b) => {
        if (b[1].hits !== a[1].hits) return b[1].hits - a[1].hits;
        if (b[1].ratio !== a[1].ratio) return b[1].ratio - a[1].ratio;
        return b[1].area - a[1].area;
      });
      const [chosen, meta] = ranked[0];
      log('Selected conversation scroller', {
        tag: chosen.tagName,
        className: typeof chosen.className === 'string' ? chosen.className.slice(0, 180) : '',
        hits: meta.hits,
        scrollTop: chosen.scrollTop,
        scrollHeight: chosen.scrollHeight,
        clientHeight: chosen.clientHeight,
        overflowY: getComputedStyle(chosen).overflowY,
      });
      return chosen;
    }
    warn('No inner conversation scroller found; falling back to document scroller.');
    return document.scrollingElement || document.documentElement;
  }

  async function waitForMutationQuiet(target) {
    target = target || document.body;
    return new Promise(resolve => {
      let quietTimer = null;
      let maxTimer = null;
      const finish = () => {
        if (quietTimer) clearTimeout(quietTimer);
        if (maxTimer) clearTimeout(maxTimer);
        observer.disconnect();
        resolve();
      };
      const resetQuiet = () => {
        if (quietTimer) clearTimeout(quietTimer);
        quietTimer = setTimeout(finish, CONFIG.mutationQuietMs);
      };
      const observer = new MutationObserver(resetQuiet);
      try {
        observer.observe(target === document.scrollingElement ? document.body : target, { childList: true, subtree: true, characterData: false });
      } catch {
        resolve();
        return;
      }
      maxTimer = setTimeout(finish, CONFIG.mutationMaxWaitMs);
      resetQuiet();
    });
  }

  function getAbsoluteOrderKey(element, scroller) {
    const rect = element.getBoundingClientRect();
    if (scroller === document.scrollingElement || scroller === document.documentElement || scroller === document.body) {
      return (window.scrollY || 0) + rect.top;
    }
    const srect = scroller.getBoundingClientRect();
    return scroller.scrollTop + (rect.top - srect.top);
  }

  function extractSnapshot(scroller) {
    const candidates = getMessageCandidates();
    const temp = [];
    let previousSpeaker = null;

    candidates.forEach((element, index) => {
      const structure = extractVisibleStructure(element);
      const rawText = normalizeWhitespaceForKey([structure.reasoning, structure.response].filter(Boolean).join(' ')) || normalizeWhitespaceForKey(element.textContent);
      if (!rawText) return;

      const speakerInfo = detectSpeaker(element, index, previousSpeaker);
      previousSpeaker = speakerInfo.speaker;
      const modeInfo = detectMode(element, structure);

      temp.push({
        element,
        rawText,
        structure,
        speaker: speakerInfo.speaker,
        speakerInfo,
        mode: modeInfo.mode,
        modeInfo,
        orderKey: getAbsoluteOrderKey(element, scroller),
      });
    });

    return temp.map((record, i) => {
      const prev = temp[i - 1]?.rawText || '';
      const next = temp[i + 1]?.rawText || '';
      const stableId = record.element.getAttribute('data-message-id') || record.element.id || record.element.getAttribute('data-id') || '';
      const base = `${record.speaker}|${record.rawText}|${record.structure.reasoning}|${record.structure.response}`;
      const key = stableId ? `id:${stableId}` : `ctx:${hashString(base)}:${hashString(prev.slice(-160))}:${hashString(next.slice(0, 160))}`;
      return { ...record, key, stableId };
    });
  }

  function mergeSnapshot(store, snapshot) {
    const occurrence = new Map();
    snapshot.forEach(record => {
      let key = record.key;
      if (store.has(key) && store.get(key).rawText !== record.rawText) {
        key = `${key}:${hashString(record.rawText)}`;
      }

      // Preserve repeated identical turns if they co-exist in one DOM snapshot.
      const count = occurrence.get(key) || 0;
      occurrence.set(key, count + 1);
      if (count > 0) key = `${key}:occ${count + 1}`;

      const existing = store.get(key);
      if (!existing) {
        store.set(key, { ...record, key, firstSeenAt: Date.now(), seenCount: 1 });
      } else {
        existing.seenCount += 1;
        existing.orderKey = Math.min(existing.orderKey, record.orderKey);
        if (record.structure.reasoning.length > existing.structure.reasoning.length) existing.structure.reasoning = record.structure.reasoning;
        if (record.structure.response.length > existing.structure.response.length) existing.structure.response = record.structure.response;
        if (existing.speakerInfo.confidence === 'low' && record.speakerInfo.confidence !== 'low') {
          existing.speaker = record.speaker;
          existing.speakerInfo = record.speakerInfo;
        }
        if (existing.mode === 'standard' && record.mode !== 'standard') {
          existing.mode = record.mode;
          existing.modeInfo = record.modeInfo;
        }
      }
    });
  }

  async function captureConversationAcrossScroll() {
    const startedAt = Date.now();
    const runtimeExpired = () => Date.now() - startedAt >= CONFIG.maxRuntimeMs;
    let candidates = getMessageCandidates();
    if (!candidates.length) throw new Error('No message-like elements found on this page.');
    const scroller = findScrollContainer(candidates);
    const originalTop = getScrollTop(scroller);
    const store = new Map();
    const expansionStats = makeReasoningExpansionStats();

    const diagScroller = () => {
      const m = getScrollMetrics(scroller);
      return {
        tag: scroller.tagName || 'DOCUMENT',
        scrollTop: Math.round(m.scrollTop),
        scrollHeight: Math.round(m.scrollHeight),
        clientHeight: Math.round(m.clientHeight),
      };
    };

    await expandVisibleReasoningDisclosures(scroller, expansionStats);
    mergeSnapshot(store, extractSnapshot(scroller));
    log('Initial snapshot:', store.size, 'records;', diagScroller());

    if (CONFIG.loadOlder) {
      let stable = 0;
      let lastSignature = '';
      for (let pass = 1; pass <= CONFIG.maxTopLoadPasses && stable < CONFIG.stableTopPasses; pass += 1) {
        if (runtimeExpired()) { expansionStats.runtimeLimitReached = true; break; }
        setScrollTop(scroller, 0);
        await waitForMutationQuiet(scroller);
        await sleep(55);
        await expandVisibleReasoningDisclosures(scroller, expansionStats);
        mergeSnapshot(store, extractSnapshot(scroller));
        const m = getScrollMetrics(scroller);
        const signature = `${Math.round(m.scrollHeight)}:${getMessageCandidates().length}:${store.size}:${expansionStats.successfulExpansions}`;
        stable = signature === lastSignature ? stable + 1 : 0;
        lastSignature = signature;
        log(`top-load pass ${pass}: ${signature}, stable=${stable}`);
      }
    }

    if (CONFIG.sweepVirtualizedConversation && !runtimeExpired()) {
      let stableRounds = 0;
      let priorRoundSignature = '';

      for (let round = 1; round <= CONFIG.maxSweepRounds && stableRounds < CONFIG.stableSweepRounds; round += 1) {
        if (runtimeExpired()) { expansionStats.runtimeLimitReached = true; break; }
        expansionStats.sweepRounds = round;
        const recordsBefore = store.size;
        const expansionsBefore = expansionStats.successfulExpansions;
        let samples = 0;
        let position = 0;
        let maxHeightSeen = 0;

        setScrollTop(scroller, 0);
        await waitForMutationQuiet(scroller);

        while (samples < CONFIG.maxSweepSamplesPerRound) {
          if (runtimeExpired()) { expansionStats.runtimeLimitReached = true; break; }
          const metrics = getScrollMetrics(scroller);
          maxHeightSeen = Math.max(maxHeightSeen, metrics.scrollHeight);
          const maxTop = Math.max(0, metrics.scrollHeight - metrics.clientHeight);
          position = Math.min(position, maxTop);
          setScrollTop(scroller, position);
          await waitForMutationQuiet(scroller);
          await expandVisibleReasoningDisclosures(scroller, expansionStats);
          mergeSnapshot(store, extractSnapshot(scroller));
          samples += 1;

          const after = getScrollMetrics(scroller);
          maxHeightSeen = Math.max(maxHeightSeen, after.scrollHeight);
          const afterMaxTop = Math.max(0, after.scrollHeight - after.clientHeight);
          if (position >= afterMaxTop - 2) break;
          const step = Math.max(180, after.clientHeight * CONFIG.sweepStepViewportRatio);
          position = Math.min(afterMaxTop, position + step);
        }

        const newRecords = store.size - recordsBefore;
        const newExpansions = expansionStats.successfulExpansions - expansionsBefore;
        const roundSignature = `${store.size}:${expansionStats.successfulExpansions}:${Math.round(maxHeightSeen)}`;
        if (roundSignature === priorRoundSignature || (newRecords === 0 && newExpansions === 0)) stableRounds += 1;
        else stableRounds = 0;
        priorRoundSignature = roundSignature;
        log(`sweep round ${round}: samples=${samples}, newRecords=${newRecords}, newExpansions=${newExpansions}, store=${store.size}, height<=${Math.round(maxHeightSeen)}, stableRounds=${stableRounds}`);
      }

      expansionStats.convergenceReached = stableRounds >= CONFIG.stableSweepRounds;
    }

    if (runtimeExpired()) expansionStats.runtimeLimitReached = true;

    if (CONFIG.restoreScrollPosition) {
      setScrollTop(scroller, originalTop);
      await sleep(30);
    }

    let records = Array.from(store.values());
    records.sort((a, b) => a.orderKey - b.orderKey || a.firstSeenAt - b.firstSeenAt);

    // Final adjacency repair: only touch low-confidence role guesses.
    let previous = null;
    records = records.map((record, index) => {
      if (record.speakerInfo.confidence === 'low') {
        const inferred = previous ? (previous === 'Human' ? 'Grok' : 'Human') : (index % 2 === 0 ? 'Human' : 'Grok');
        record = { ...record, speaker: inferred, speakerInfo: { ...record.speakerInfo, reason: 'final adjacency fallback' } };
      }
      previous = record.speaker;
      return record;
    });

    window.__grokConsoleExportExhaustiveExpansionStats = publicReasoningExpansionStats(expansionStats);
    window.__grokConsoleExportExhaustiveScroller = diagScroller();
    log('Expansion result:', window.__grokConsoleExportExhaustiveExpansionStats);
    log('Scroller result:', window.__grokConsoleExportExhaustiveScroller);
    return records;
  }

  function renderMessageBody(record) {
    const reasoning = cleanMarkdown(record.structure.reasoning);
    const response = cleanMarkdown(record.structure.response);

    if (reasoning) {
      let out = '### Thinking\n\n';
      out += '<!-- GROK_EXPORT:BEGIN kind=assistant_thinking -->\n';
      out += `${reasoning}\n`;
      out += '<!-- GROK_EXPORT:END kind=assistant_thinking -->\n\n';
      if (response) {
        out += '### Response\n\n';
        out += '<!-- GROK_EXPORT:BEGIN kind=assistant_final -->\n';
        out += `${response}\n`;
        out += '<!-- GROK_EXPORT:END kind=assistant_final -->';
      }
      return out.trim();
    }
    if (record.speaker === 'Grok' && (response || record.rawText)) {
      const body = response || record.rawText;
      return `<!-- GROK_EXPORT:BEGIN kind=assistant_final -->\n${body}\n<!-- GROK_EXPORT:END kind=assistant_final -->`;
    }
    return response || record.rawText;
  }

  function formatAsMarkdown(records) {
    const now = new Date();
    const counts = records.reduce((acc, r) => {
      acc[r.speaker] = (acc[r.speaker] || 0) + 1;
      acc.modes[r.mode] = (acc.modes[r.mode] || 0) + 1;
      return acc;
    }, { Human: 0, Grok: 0, modes: {} });

    let md = '# Grok Conversation Export\n\n';
    md += `**Exported:** ${now.toLocaleString()}  \n`;
    md += `**Messages:** ${records.length} (Human ${counts.Human || 0} / Grok ${counts.Grok || 0})  \n`;
    md += `**URL:** ${location.href}  \n`;
    md += `**Export Method:** ${CONFIG.exportMethod}\n\n`;
    md += '> v1.4 is calibrated against a supplied Grok MHTML snapshot. It sweeps the live conversation scroller, but only opens exact known disclosures that explicitly report a closed state. It never generically clicks buttons. Invisible `GROK_EXPORT` comments preserve semantic provenance for optional offline post-processing.\n\n';
    md += '---\n\n';

    records.forEach((record, index) => {
      const modeIndicator = record.mode !== 'standard' ? ` [${record.mode.toUpperCase()}]` : '';
      md += `## ${record.speaker}${modeIndicator}\n\n`;
      md += `${renderMessageBody(record)}\n\n`;
      if (index < records.length - 1) md += '---\n\n';
    });

    return md.trimEnd() + '\n';
  }

  function getConversationTitle(records) {
    const title = document.title?.replace(/\s*[|–—-]\s*Grok.*$/i, '').trim();
    if (title && !/^Grok$/i.test(title)) return sanitizeFilenamePart(title);
    const firstHuman = records.find(r => r.speaker === 'Human');
    return sanitizeFilenamePart(firstHuman?.rawText?.slice(0, 100), 'untitled');
  }

  function downloadMarkdown(content, filename) {
    const blob = new Blob([content], { type: 'text/markdown;charset=utf-8' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = filename;
    a.rel = 'noopener';
    a.style.display = 'none';
    document.body.appendChild(a);
    a.click();
    a.remove();
    setTimeout(() => URL.revokeObjectURL(url), 2000);
  }

  async function run({ download = true } = {}) {
    if (!/grok\.com$|(^|\.)x\.com$/i.test(location.hostname)) {
      warn('This script is intended for grok.com or x.com Grok pages; continuing anyway for testing.');
    }

    const records = await captureConversationAcrossScroll();
    if (!records.length) throw new Error('Conversation capture returned zero messages.');

    const markdown = formatAsMarkdown(records);
    const title = getConversationTitle(records).replace(/\s+/g, '-');
    const timestamp = new Date().toISOString().replace(/[:.]/g, '-');
    const filename = `${CONFIG.filenamePrefix}-${title}-${records.length}msgs-${timestamp}.md`;

    if (download) downloadMarkdown(markdown, filename);
    console.log(`[Grok Export Exhaustive] Captured ${records.length} records${download ? ` -> ${filename}` : ''}`);
    console.log('[Grok Export Exhaustive] Expansion stats:', window.__grokConsoleExportExhaustiveExpansionStats || {});
    console.log('[Grok Export Exhaustive] Conversation scroller:', window.__grokConsoleExportExhaustiveScroller || {});
    console.table(records.map((r, i) => ({
      '#': i + 1,
      speaker: r.speaker,
      speakerConfidence: r.speakerInfo.confidence,
      mode: r.mode,
      modeConfidence: r.modeInfo.confidence,
      chars: r.rawText.length,
      thinkingChars: r.structure.reasoning.length,
      seen: r.seenCount,
    })));

    return { records, markdown, filename };
  }

  // v1.4 calibration note: the supplied MHTML exposed 42 .message-bubble nodes (21 user / 21 assistant),
  // 198 Bash-command controls, 11 Write-file controls, 7 Edited-files groups, 2 Read-file rows,
  // 52 reasoning-result rows, and 21 canvas triggers. Exact selectors above are intentionally narrow.

  // NOTE ABOUT BROWSER "SAVE PAGE AS":
  // Use "Webpage, Complete" (or MHTML/Single File if your Chrome offers it) rather than "HTML only".
  // However, Grok can virtualize old turns; browser Save As can only serialize nodes currently mounted.
  // The Markdown generated by this script is therefore still the primary archive.
  window.GrokConsoleExportExhaustive = Object.freeze({
    run,
    captureConversationAcrossScroll,
    expandVisibleReasoningDisclosures,
    getMessageCandidates,
    extractVisibleStructure,
    annotateSemanticRegions,
    domToMarkdown,
    formatAsMarkdown,
  });

  try {
    window.__grokConsoleExportExhaustiveLastResult = await run({ download: true });
  } catch (error) {
    console.error('[Grok Export Exhaustive] Export failed:', error);
    alert(`Grok export failed: ${error.message}`);
  }
})();
