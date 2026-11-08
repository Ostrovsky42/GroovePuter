#!/usr/bin/env node
/* Extract only pinned chord/scale vocabulary. Never import either MIDI generator. */
import * as fs from "node:fs";
import * as path from "node:path";
import { createRequire } from "node:module";
import { execFileSync } from "node:child_process";

type SourceEntry = { symbol: string; exportName: string; index: number; line: number };

function argument(name: string): string {
  const index = process.argv.indexOf(name);
  if (index < 0 || index + 1 >= process.argv.length) {
    throw new Error(`missing ${name}`);
  }
  return process.argv[index + 1];
}

function sourceEntries(source: string, declaration: string): SourceEntry[] {
  const declarationAt = source.indexOf(declaration);
  if (declarationAt < 0) throw new Error(`missing source list: ${declaration}`);
  const equals = source.indexOf("=", declarationAt);
  const open = source.indexOf("[", equals + 1);
  const close = source.indexOf("]", open + 1);
  if (open < 0 || close < 0) throw new Error(`malformed source list: ${declaration}`);
  const body = source.slice(open + 1, close);
  const object = /\{\s*symbol:\s*'([^']*)'(?:\s*,\s*fileName:\s*'([^']*)')?(?:\s*,\s*title:\s*'([^']*)')?\s*\}/g;
  const result: SourceEntry[] = [];
  let match: RegExpExecArray | null;
  while ((match = object.exec(body)) !== null) {
    const before = source.slice(0, open + 1 + match.index);
    const line = before.split("\n").length;
    result.push({
      symbol: match[1],
      exportName: match[2] ?? match[1],
      index: result.length,
      line,
    });
  }
  if (result.length === 0) throw new Error(`no entries found: ${declaration}`);
  return result;
}

function main(): void {
  const sourceRoot = path.resolve(argument("--source"));
  const outputPath = path.resolve(argument("--out"));
  const scriptRoot = path.resolve(__dirname, "../..");
  const lock = JSON.parse(fs.readFileSync(
    path.join(scriptRoot, "tools/harmony/source_lock.json"), "utf8"));
  const sourceLock = lock.sources.fannon;
  const commit = execFileSync("git", ["-C", sourceRoot, "rev-parse", "HEAD"],
    { encoding: "utf8" }).trim();
  if (commit !== sourceLock.commit) throw new Error(`unexpected Fannon commit: ${commit}`);

  const sourceFileSha256: Record<string, string> = {};
  for (const [relativePath, expected] of Object.entries(sourceLock.files)) {
    const actual = require("node:crypto").createHash("sha256")
      .update(fs.readFileSync(path.join(sourceRoot, relativePath))).digest("hex");
    if (actual !== expected) throw new Error(`source digest mismatch: ${relativePath}`);
    sourceFileSha256[relativePath] = actual;
  }

  const requireFromSource = createRequire(path.join(sourceRoot, "package.json"));
  const tonal = requireFromSource("@tonaljs/tonal");
  const lockFile = JSON.parse(fs.readFileSync(path.join(sourceRoot, "package-lock.json"), "utf8"));
  const tonalPackages: Record<string, { version: string; license: string }> = {};
  for (const name of Object.keys(sourceLock.tonal_packages)) {
    const lockPackage = lockFile.packages[`node_modules/${name}`];
    const installed = JSON.parse(fs.readFileSync(
      path.join(sourceRoot, "node_modules", name, "package.json"), "utf8"));
    tonalPackages[name] = { version: installed.version, license: installed.license };
    if (installed.version !== lockPackage.version || installed.license !== "MIT") {
      throw new Error(`unexpected Tonal package metadata: ${name}`);
    }
  }
  if (JSON.stringify(tonalPackages) !== JSON.stringify(sourceLock.tonal_packages)) {
    throw new Error("installed Tonal.js packages differ from source lock");
  }

  const chordSourcePath = "src/generateChords.ts";
  const scaleSourcePath = "src/generateScales.ts";
  const chordSource = fs.readFileSync(path.join(sourceRoot, chordSourcePath), "utf8");
  const scaleSource = fs.readFileSync(path.join(sourceRoot, scaleSourcePath), "utf8");
  const chordEntries = sourceEntries(chordSource, "const chordTypesVariant1");
  const scaleEntries = sourceEntries(scaleSource, "const scaleTypesVariant1");
  if (chordEntries.length !== sourceLock.expected_input_counts.qualities ||
      scaleEntries.length !== sourceLock.expected_input_counts.scales) {
    throw new Error("vocabulary cardinality differs from pinned source contract");
  }

  const qualities = chordEntries.map(entry => {
    const chord = tonal.Chord.getChord(entry.symbol, "C4");
    if (chord.empty) throw new Error(`Tonal.js rejected chord alias: ${entry.symbol}`);
    return {
      symbol: entry.symbol,
      export_name: entry.exportName,
      root_pc: 0,
      pitch_classes: chord.notes.map((note: string) => tonal.Note.chroma(note)),
      tonal_aliases: chord.aliases,
      source: { path: chordSourcePath, line: entry.line, index: entry.index },
    };
  });

  const scales = scaleEntries.map(entry => {
    const scale = tonal.Scale.get(`C4 ${entry.symbol}`);
    if (scale.empty) throw new Error(`Tonal.js rejected scale alias: ${entry.symbol}`);
    return {
      symbol: entry.symbol,
      export_name: entry.exportName,
      root_pc: 0,
      pitch_classes: scale.notes.map((note: string) => tonal.Note.chroma(note)),
      tonal_aliases: scale.aliases,
      source: { path: scaleSourcePath, line: entry.line, index: entry.index },
    };
  });

  const output = {
    schema_version: 1,
    source_repository: sourceLock.repository,
    source_commit: commit,
    source_file_sha256: sourceFileSha256,
    tonal_packages: tonalPackages,
    qualities,
    scales,
  };
  fs.mkdirSync(path.dirname(outputPath), { recursive: true });
  fs.writeFileSync(outputPath, `${JSON.stringify(output, null, 2)}\n`);
}

main();
