// Run every recipe in web/recipes.json through the C++ binary and write the
// traces the playground ships with: web/traces/<recipe-id>.json
//
//   node tools/gen-traces.mjs
import { mkdirSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';
import { WEB, findBinary, recipes, runCpp } from './common.mjs';

const bin = findBinary();
const out = join(WEB, 'traces');
mkdirSync(out, { recursive: true });
let n = 0;
for (const ds of recipes().datasets) {
  for (const r of ds.recipes) {
    const trace = runCpp(bin, r.pipeline);
    if (!trace.ok) {
      console.error(`${r.id}: ${trace.steps.at(-1).error}`);
      process.exit(1);
    }
    writeFileSync(join(out, r.id + '.json'), JSON.stringify(trace) + '\n');
    const last = trace.steps.at(-1).stats;
    console.log(`${r.id.padEnd(16)} ${trace.steps.length} steps -> ${last.rowsOut} rows x ${last.colsOut} cols`);
    n++;
  }
}
console.log(`wrote ${n} traces to web/traces/ using ${bin}`);
