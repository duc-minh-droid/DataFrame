// Shared helpers for the trace tools.
import { spawnSync } from 'node:child_process';
import { existsSync, readFileSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

export const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), '..');
export const WEB = join(ROOT, 'web');
export const DATA = join(WEB, 'data');

export function findBinary() {
  const candidates = [
    process.env.DATAFRAME_BIN,
    join(ROOT, 'build', 'DataFrame.exe'),
    join(ROOT, 'build', 'DataFrame'),
    join(ROOT, 'build', 'Release', 'DataFrame.exe'),
  ].filter(Boolean);
  const bin = candidates.find((p) => existsSync(p));
  if (!bin) {
    console.error('DataFrame binary not found. Build it first (see README) or set DATAFRAME_BIN.');
    process.exit(1);
  }
  return bin;
}

export function runCpp(bin, pipeline) {
  const r = spawnSync(bin, ['--trace', pipeline, '--data-dir', DATA], { encoding: 'utf8', maxBuffer: 64 << 20 });
  if (!r.stdout) throw new Error('no output from binary for: ' + pipeline + '\n' + r.stderr);
  return JSON.parse(r.stdout);
}

export function recipes() {
  return JSON.parse(readFileSync(join(WEB, 'recipes.json'), 'utf8'));
}
