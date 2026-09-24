// Check that the JavaScript port (web/engine.js) produces exactly the same
// trace as the C++ binary: every recipe, plus a batch of extra pipelines that
// the recipes do not cover. Numbers are compared with ===; only timings and the
// "engine" tag are ignored.
//
//   node tools/verify-port.mjs
import { readFileSync, readdirSync } from 'node:fs';
import { join } from 'node:path';
import { DATA, findBinary, recipes, runCpp } from './common.mjs';
import { runPipeline } from '../web/engine.js';

const files = Object.fromEntries(
  readdirSync(DATA).filter((f) => f.endsWith('.csv')).map((f) => [f, readFileSync(join(DATA, f), 'utf8')]),
);

const extra = [
  'load:sales.csv | sort:region | sort:units:desc',
  'load:sales.csv | filter:region==North | filter:price<20 | select:product,price',
  'load:sales.csv | filter:region==Nowhere',
  'load:sales.csv | groupby:discount:count,mean(units)',
  'load:sales.csv | join:regions.csv:region | groupby:manager:sum(units),mean(target)',
  'load:sales.csv | join:regions.csv:region | describe',
  'load:sales.csv | head:3 | describe',
  'load:sales.csv | select:price,region | sort:price | head:5',
  'load:sales.csv | filter:discount>0 | groupby:region:count | join:regions.csv:region | sort:target:desc',
  'load:iris.csv | filter:sepal_width<3 | groupby:species:min(sepal_width),max(sepal_width)',
  'load:iris.csv | sort:sepal_length:desc | head:6 | describe',
  'load:iris.csv | join:species.csv:species | groupby:native_range:count',
  'load:trades.csv | groupby:ticker:count,mean(price) | join:tickers.csv:ticker',
  'load:trades.csv | filter:volume>=5000 | sort:price | select:ticker,price',
  'load:trades.csv | sort:signal | sort:ticker',
  'load:regions.csv | join:sales.csv:region',
  'load:tickers.csv | describe',
];
const failing = [
  'load:sales.csv | filter:nosuch>1',
  'load:sales.csv | filter:region>North',
  'load:sales.csv | groupby:region:mean(product)',
  'load:sales.csv | select:units | describe | describe | select:region',
  'filter:units>1',
  'load:nope.csv',
  'load:sales.csv | pivot:region',
];

function strip(t) {
  const c = structuredClone(t);
  delete c.engine;
  for (const s of c.steps) if (s.stats) delete s.stats.micros;
  return c;
}

function diff(a, b, path = '') {
  if (typeof a === 'number' && typeof b === 'number') return a === b ? null : `${path}: ${a} !== ${b}`;
  if (a === null || b === null || typeof a !== 'object' || typeof b !== 'object') {
    return a === b ? null : `${path}: ${JSON.stringify(a)} !== ${JSON.stringify(b)}`;
  }
  if (Array.isArray(a) !== Array.isArray(b)) return `${path}: array vs object`;
  const keys = new Set([...Object.keys(a), ...Object.keys(b)]);
  for (const k of keys) {
    if (!(k in a) || !(k in b)) return `${path}.${k}: missing on one side`;
    const d = diff(a[k], b[k], `${path}.${k}`);
    if (d) return d;
  }
  return null;
}

const bin = findBinary();
const pipelines = [...recipes().datasets.flatMap((d) => d.recipes.map((r) => r.pipeline)), ...extra];
let ok = 0;
let bad = 0;
let values = 0;
for (const p of pipelines) {
  const c = strip(runCpp(bin, p));
  const j = strip(runPipeline(p, files));
  const d = diff(c, j);
  if (d) {
    bad++;
    console.log('DIFF  ' + p + '\n      ' + d);
  } else {
    ok++;
    values += JSON.stringify(c).length;
  }
}
for (const p of failing) {
  const c = runCpp(bin, p);
  const j = runPipeline(p, files);
  const same = !c.ok && !j.ok && c.steps.length === j.steps.length;
  if (same) ok++;
  else {
    bad++;
    console.log('DIFF  (error case) ' + p);
  }
}
console.log(`${ok} identical, ${bad} different  (${pipelines.length} traces + ${failing.length} error cases, ${values} bytes compared)`);
process.exit(bad ? 1 : 0);
