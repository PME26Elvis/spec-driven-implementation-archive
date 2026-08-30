# CVC v1.0.1 — Specification-Driven Implementation Task Pack

## 1. Purpose

This task pack defines a single complete software assignment: implement **CVC**, a local command-line version-control system for one repository at a time.

The assignment is intentionally implementation-heavy. It is designed to require substantial handwritten C17 work in parsing, object storage, hashing, diffing, repository traversal, history management, branch operations, merge, recovery-oriented writes, integrity checking, and command-line behavior.

This pack defines **what must be built and how externally visible behavior must work**. Development workflow and implementation process are outside the specification.

## 2. Normative Language

The words **MUST**, **MUST NOT**, **REQUIRED**, **SHALL**, **SHALL NOT**, **SHOULD**, and **MAY** are normative.

If two documents appear to conflict, precedence is:

1. `specs/01_SCOPE_AND_CONSTRAINTS.md`
2. `specs/02_CLI_CONTRACT.md`
3. `specs/03_REPOSITORY_MODEL.md`
4. `specs/04_CONFIG_JSON.md`
5. `specs/05_DIFF_AND_FILTERING.md`
6. `specs/06_BRANCH_MERGE_ROLLBACK.md`
7. `specs/07_ERROR_RECOVERY_AND_INTEGRITY.md`
8. `specs/08_EDGE_CASES.md`
9. acceptance documents
10. examples

Ambiguities MUST be resolved conservatively in favor of data safety and deterministic behavior.

## 3. Product Summary

CVC provides:

- repository initialization;
- working-tree status;
- snapshot saving without a staging area;
- content-addressed loose objects using the fixed v1 canonical serialization;
- commit history;
- branches and switching;
- line-based diff and diff statistics;
- include/exclude path patterns;
- handwritten JSON configuration parsing;
- symlink versioning without dereferencing targets;
- three-way text merge with conflict reporting;
- rollback by creating a new commit rather than rewriting history;
- repository verification;
- atomic metadata updates and repository writer locking;
- deterministic repository semantics.

CVC deliberately does **not** implement compression, object packs, remotes, networking, authentication, staging, tags, rebasing, submodules, binary versioning, or large-file versioning.

## 4. Required Reading

An implementation is not conforming unless all specification and acceptance documents have been considered.

- [Scope and Constraints](specs/01_SCOPE_AND_CONSTRAINTS.md)
- [CLI Contract](specs/02_CLI_CONTRACT.md)
- [Repository Model](specs/03_REPOSITORY_MODEL.md)
- [JSON Configuration](specs/04_CONFIG_JSON.md)
- [Diff and Filtering](specs/05_DIFF_AND_FILTERING.md)
- [Branch, Merge, and Rollback](specs/06_BRANCH_MERGE_ROLLBACK.md)
- [Error, Recovery, and Integrity](specs/07_ERROR_RECOVERY_AND_INTEGRITY.md)
- [Edge Cases](specs/08_EDGE_CASES.md)
- [Test Matrix](acceptance/01_TEST_MATRIX.md)
- [Manual Acceptance Checklist](acceptance/02_MANUAL_CHECKLIST.md)
- [Deliverables and Definition of Done](acceptance/03_DELIVERABLES_AND_DOD.md)
- [Prohibited Substitutions](acceptance/04_PROHIBITED_IMPLEMENTATIONS.md)

## 5. Assignment Boundary

The submission is one project. Auxiliary internal modules and test executables are expected, but a second product or unrelated developer-tool project is not part of this task.

The expected implementation scale is substantial enough that a trivial backup script, shell wrapper, or snapshot copier cannot satisfy the requirements. No exact source-line target is itself an acceptance condition.

## 6. Release Identity

Task-pack version: **1.0.1**.

A submission claiming completion MUST satisfy every mandatory Release Gate in `acceptance/03_DELIVERABLES_AND_DOD.md`.
