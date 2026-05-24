#!/usr/bin/env node
import { readFileSync } from "node:fs";
import { resolve } from "node:path";
import { DiagramLinter } from "./wokwi-tools/node_modules/@wokwi/diagram-lint/dist/index.js";
import { REMOTE_BOARDS_URL } from "./wokwi-tools/node_modules/@wokwi/diagram-lint/dist/registry/part-registry.js";

const diagramPath = resolve(process.argv[2] ?? "diagram.json");
const linter = new DiagramLinter();

try {
  const response = await fetch(REMOTE_BOARDS_URL);
  if (response.ok) {
    linter.getRegistry().loadBoardsBundle(await response.json());
  }
} catch {
  // Offline linting still works with the package's bundled board definitions.
}

const result = linter.lintJSON(readFileSync(diagramPath, "utf8"));

for (const issue of result.issues) {
  const where = issue.partId ? ` part=${issue.partId}` : "";
  console.log(`${issue.severity}: ${issue.rule}${where}: ${issue.message}`);
}

if (!result.valid) {
  process.exit(1);
}

console.log(`diagram ok (${result.stats.warnings} warnings)`);
