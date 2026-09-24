// JavaScript port of the C++ pipeline runner (Pipeline.h + DataFrame library).
//
// It exists so the playground can run pipelines you build in the browser.
// The shipped recipe traces come from the real C++ binary; tools/verify-port.mjs
// runs every recipe plus a batch of extra pipelines through both and checks the
// traces are identical (every number compared with ===, timings ignored).
//
// Works in the browser and in Node (plain ES module, no dependencies).

const NUM_RE = /^[+-]?(\d+\.?\d*|\.\d+)([eE][+-]?\d+)?$/;

function splitTrim(s, sep) {
  return s.split(sep).map((x) => x.trim());
}

// ---------------------------------------------------------------- frames --
// A frame mirrors DataFrame: names, one numeric array per column, and a
// (possibly empty) dictionary per column for categorical columns.

function makeFrame(names, labels) {
  const seen = new Set();
  for (const n of names) {
    if (seen.has(n)) throw new Error('Duplicate column name: ' + n);
    seen.add(n);
  }
  return { names, cols: names.map(() => []), labels: labels || names.map(() => []) };
}

function frameRows(f) {
  return f.cols.length ? f.cols[0].length : 0;
}

function colIndex(f, name) {
  const i = f.names.indexOf(name);
  if (i < 0) throw new Error("No column named '" + name + "'");
  return i;
}

export function parseCSV(text) {
  const lines = text.split('\n').map((l) => (l.endsWith('\r') ? l.slice(0, -1) : l));
  if (!lines.length || (lines.length === 1 && lines[0] === '')) throw new Error('Empty CSV');
  const splitLine = (line) => line.split(',');
  const names = splitLine(lines[0]);
  const cells = [];
  for (let i = 1; i < lines.length; i++) {
    if (lines[i] === '') continue;
    const row = splitLine(lines[i]);
    if (row.length !== names.length) throw new Error('Row size mismatch on data line ' + (cells.length + 1));
    cells.push(row);
  }
  const f = makeFrame(names);
  const numeric = names.map((_, c) => cells.every((r) => NUM_RE.test(r[c])));
  names.forEach((_, c) => {
    if (numeric[c]) return;
    const dict = [...new Set(cells.map((r) => r[c]))].sort((a, b) => (a < b ? -1 : a > b ? 1 : 0));
    f.labels[c] = dict;
  });
  for (const row of cells) {
    names.forEach((_, c) => {
      f.cols[c].push(numeric[c] ? Number(row[c]) : f.labels[c].indexOf(row[c]));
    });
  }
  return f;
}

function formatCell(f, c, v) {
  const d = f.labels[c];
  return d.length ? (v >= 0 && v < d.length ? d[Math.trunc(v)] : '?') : String(v);
}

// ----------------------------------------------------------------- views --
// A view mirrors DataFrameView: a frame plus row and column index arrays.

const view = (fid, frame) => ({
  fid,
  f: frame,
  rows: [...Array(frameRows(frame)).keys()],
  cols: [...frame.names.keys()],
});

function vColIdx(v, name) {
  for (let i = 0; i < v.cols.length; i++) if (v.f.names[v.cols[i]] === name) return i;
  throw new Error("No column named '" + name + "' in view");
}
const vAt = (v, r, c) => v.f.cols[v.cols[c]][v.rows[r]];
const vIsCat = (v, c) => v.f.labels[v.cols[c]].length > 0;
const vName = (v, c) => v.f.names[v.cols[c]];
const vFormat = (v, r, c) => formatCell(v.f, v.cols[c], vAt(v, r, c));
const vNumRows = (v) => v.rows.length;

function tableOf(v) {
  return {
    frame: v.fid,
    columns: v.cols.map((_, c) => ({ name: vName(v, c), kind: vIsCat(v, c) ? 'cat' : 'num' })),
    ids: v.rows.map((r) => v.fid + ':' + r),
    rows: v.rows.map((_, r) => v.cols.map((_, c) => (vIsCat(v, c) ? vFormat(v, r, c) : vAt(v, r, c)))),
  };
}

function storageOf(f, fid) {
  return {
    frame: fid,
    columns: f.names.map((name, c) => ({
      name,
      kind: f.labels[c].length ? 'cat' : 'num',
      labels: f.labels[c].slice(),
      data: f.cols[c].slice(),
    })),
  };
}

// ------------------------------------------------------------ operations --

function sum(arr) {
  let acc = 0;
  for (const x of arr) acc += x;
  return acc;
}

function quantiles(values, qs) {
  const t = values.slice().sort((a, b) => a - b);
  return qs.map((q) => {
    const pos = q * (t.length - 1);
    const lo = Math.floor(pos);
    const hi = Math.min(lo + 1, t.length - 1);
    return t[lo] + (t[hi] - t[lo]) * (pos - lo);
  });
}

function describe(v) {
  const stats = ['count', 'mean', 'std', 'min', '25%', '50%', '75%', 'max'];
  const numeric = v.cols.map((_, c) => c).filter((c) => !vIsCat(v, c));
  if (!numeric.length) throw new Error('describe() needs at least one numeric column');
  if (!vNumRows(v)) throw new Error('describe() on an empty view');
  const names = ['stat', ...numeric.map((c) => vName(v, c))];
  const out = makeFrame(names);
  out.labels[0] = stats.slice();
  const cols = numeric.map((c) => v.rows.map((_, r) => vAt(v, r, c)));
  const quart = cols.map((c) => quantiles(c, [0.25, 0.5, 0.75]));
  stats.forEach((_, s) => {
    out.cols[0].push(s);
    cols.forEach((c, k) => {
      const n = c.length;
      const mean = sum(c) / n;
      let val;
      switch (s) {
        case 0: val = n; break;
        case 1: val = mean; break;
        case 2: {
          if (n < 2) { val = 0; break; }
          let acc = 0;
          for (const x of c) acc += (x - mean) * (x - mean);
          val = Math.sqrt(acc / (n - 1));
          break;
        }
        case 3: val = c.reduce((a, b) => (b < a ? b : a)); break;
        case 4: val = quart[k][0]; break;
        case 5: val = quart[k][1]; break;
        case 6: val = quart[k][2]; break;
        default: val = c.reduce((a, b) => (b > a ? b : a)); break;
      }
      out.cols[k + 1].push(val);
    });
  });
  return out;
}

function reduce(fn, v, rows, c) {
  if (fn === 'count') return rows.length;
  if (fn === 'sum' || fn === 'mean') {
    let acc = 0;
    for (const r of rows) acc += vAt(v, r, c);
    return fn === 'sum' ? acc : acc / rows.length;
  }
  if (fn === 'min') return rows.reduce((a, r) => Math.min(a, vAt(v, r, c)), Infinity);
  if (fn === 'max') return rows.reduce((a, r) => Math.max(a, vAt(v, r, c)), -Infinity);
  throw new Error("Unknown aggregation '" + fn + "'");
}

function groupBy(v, key) {
  const k = vColIdx(v, key);
  const map = new Map();
  for (let i = 0; i < vNumRows(v); i++) {
    let val = vAt(v, i, k);
    if (val === 0) val = 0; // -0 and 0 share a bucket, as in unordered_map<double>
    if (!map.has(val)) map.set(val, []);
    map.get(val).push(i);
  }
  const keys = [...map.keys()].sort((a, b) => a - b);
  return { k, keys, groups: keys.map((x) => map.get(x)) };
}

function agg(v, g, aggs) {
  const names = [vName(v, g.k)];
  const cols = [];
  for (const a of aggs) {
    if (a.fn === 'count') {
      names.push('count');
      cols.push(0);
    } else {
      const c = vColIdx(v, a.column);
      if (vIsCat(v, c)) throw new Error(a.fn + "() on categorical column '" + a.column + "'");
      names.push(a.column + '_' + a.fn);
      cols.push(c);
    }
  }
  const out = makeFrame(names);
  out.labels[0] = v.f.labels[v.cols[g.k]].slice();
  g.keys.forEach((key, i) => {
    out.cols[0].push(key);
    aggs.forEach((a, j) => out.cols[j + 1].push(reduce(a.fn, v, g.groups[i], cols[j])));
  });
  return out;
}

function joinKey(v, r, c) {
  if (vIsCat(v, c)) return 'c' + vFormat(v, r, c);
  const x = vAt(v, r, c);
  return 'n' + (Object.is(x, -0) ? '-0' : String(x));
}

function join(left, right, key, pairs) {
  const lk = vColIdx(left, key);
  const rk = vColIdx(right, key);
  if (vIsCat(left, lk) !== vIsCat(right, rk)) {
    throw new Error("join(): key '" + key + "' is categorical on one side only");
  }
  const names = left.cols.map((_, c) => vName(left, c));
  const rightCols = [];
  right.cols.forEach((_, i) => {
    if (i === rk) return;
    rightCols.push(i);
    let n = vName(right, i);
    if (names.includes(n)) n += '_right';
    names.push(n);
  });
  const out = makeFrame(names);
  left.cols.forEach((c, i) => (out.labels[i] = left.f.labels[c].slice()));
  rightCols.forEach((c, k) => (out.labels[left.cols.length + k] = right.f.labels[right.cols[c]].slice()));

  const index = new Map();
  for (let r = 0; r < vNumRows(right); r++) {
    const kk = joinKey(right, r, rk);
    if (!index.has(kk)) index.set(kk, []);
    index.get(kk).push(r);
  }
  for (let l = 0; l < vNumRows(left); l++) {
    const hit = index.get(joinKey(left, l, lk));
    if (!hit) continue;
    for (const r of hit) {
      left.cols.forEach((_, c) => out.cols[c].push(vAt(left, l, c)));
      rightCols.forEach((c, k) => out.cols[left.cols.length + k].push(vAt(right, r, c)));
      pairs.push([l, r]);
    }
  }
  return out;
}

// -------------------------------------------------------------- pipeline --

export function parsePipeline(text) {
  return splitTrim(text, '|')
    .filter((p) => p !== '')
    .map((part) => {
      const pieces = splitTrim(part, ':');
      return { op: pieces[0], args: pieces.slice(1), text: part };
    });
}

function parseCondition(s) {
  for (const op of ['>=', '<=', '!=', '==', '>', '<']) {
    const pos = s.indexOf(op);
    if (pos > 0) return { column: s.slice(0, pos).trim(), op, value: s.slice(pos + op.length).trim() };
  }
  throw new Error("filter: expected <column><op><value>, got '" + s + "'");
}

function parseAggs(s) {
  return splitTrim(s, ',').map((a) => {
    if (a === 'count') return { fn: 'count', column: '' };
    const open = a.indexOf('(');
    const close = a.indexOf(')');
    if (open < 0 || close < 0 || close < open) throw new Error("groupby: expected fn(column), got '" + a + "'");
    return { fn: a.slice(0, open).trim(), column: a.slice(open + 1, close).trim() };
  });
}

const lit = (s) => '"' + s + '"';

function positionsIn(before, v) {
  const pos = new Map(before.rows.map((r, i) => [r, i]));
  return v.rows.map((r) => pos.get(r));
}

/**
 * Run a pipeline. `files` maps file names (as used in load:/join:) to CSV text.
 * Returns the same trace object the C++ binary prints with --trace.
 */
export function runPipeline(text, files) {
  const frames = [];
  let cur = null;
  const push = (f) => frames.push(f) - 1;
  const read = (name) => {
    if (!(name in files)) throw new Error('Failed to open CSV: ' + name);
    return parseCSV(files[name]);
  };
  const res = { engine: 'js', pipeline: text, ok: true, steps: [] };

  for (const spec of parsePipeline(text)) {
    const index = res.steps.length;
    const st = { index, op: spec.op, text: spec.text };
    const a = spec.args;
    const need = (n) => {
      if (a.length < n) throw new Error(spec.op + ": missing arguments in '" + spec.text + "'");
    };
    let before = cur;
    const t0 = typeof performance !== 'undefined' ? performance.now() : 0;
    let map = {};
    let storage = null;
    let cpp = '';
    try {
      if (spec.op === 'load') {
        need(1);
        const fid = push(read(a[0]));
        cur = view(fid, frames[fid]);
        cpp = 'DataFrame df = readCSVFile(' + lit(a[0]) + ');';
        map = { file: a[0] };
        storage = storageOf(frames[fid], fid);
      } else {
        if (!cur) throw new Error('pipeline must start with load:<file>');
        if (spec.op === 'filter') {
          need(1);
          const cond = parseCondition(a[0]);
          const col = vColIdx(before, cond.column);
          const isCat = vIsCat(before, col);
          let rhs;
          if (isCat) {
            if (cond.op !== '==' && cond.op !== '!=') {
              throw new Error("filter: only == and != work on categorical column '" + cond.column + "'");
            }
            rhs = before.f.labels[before.cols[col]].indexOf(cond.value);
          } else {
            rhs = parseFloat(cond.value);
            if (Number.isNaN(rhs)) throw new Error("filter: '" + cond.value + "' is not a number");
          }
          const cmp = {
            '>=': (x) => x >= rhs, '<=': (x) => x <= rhs, '>': (x) => x > rhs,
            '<': (x) => x < rhs, '==': (x) => x === rhs, '!=': (x) => x !== rhs,
          }[cond.op];
          const colData = before.f.cols[before.cols[col]];
          const rows = before.rows.filter((r) => cmp(colData[r]));
          cur = { ...before, rows };
          const kept = positionsIn(before, cur);
          const keptSet = new Set(kept);
          const dropped = before.rows.map((_, i) => i).filter((i) => !keptSet.has(i));
          map = { column: cond.column, cmp: cond.op, value: isCat ? cond.value : rhs, kept, dropped };
          const rhsCpp = isCat ? 'df.encode(' + lit(cond.column) + ', ' + lit(cond.value) + ')' : cond.value;
          cpp = '.filter(' + lit(cond.column) + ', [](double x) { return x ' + cond.op + ' ' + rhsCpp + '; })';
        } else if (spec.op === 'select') {
          need(1);
          const names = splitTrim(a[0], ',');
          const cols = names.map((n) => before.cols[vColIdx(before, n)]);
          cur = { ...before, cols };
          const dropped = before.cols.map((_, c) => vName(before, c)).filter((n) => !names.includes(n));
          map = { kept: names, dropped };
          cpp = '.select({ ' + names.map(lit).join(', ') + ' })';
        } else if (spec.op === 'sort') {
          need(1);
          const asc = !(a.length > 1 && a[1] === 'desc');
          const colData = before.f.cols[before.cols[vColIdx(before, a[0])]];
          const rows = before.rows.slice().sort((x, y) => {
            const d = colData[x] < colData[y] ? -1 : colData[x] > colData[y] ? 1 : 0;
            return asc ? d : -d;
          });
          cur = { ...before, rows };
          map = { column: a[0], ascending: asc, order: positionsIn(before, cur) };
          cpp = '.sort(' + lit(a[0]) + (asc ? '' : ', false') + ')';
        } else if (spec.op === 'head') {
          const n = a.length ? parseInt(a[0], 10) : 5;
          if (Number.isNaN(n)) throw new Error('stoul');
          cur = { ...before, rows: before.rows.slice(0, n) };
          map = { n, kept: cur.rows.length };
          cpp = '.head(' + n + ')';
        } else if (spec.op === 'groupby') {
          need(2);
          const aggs = parseAggs(a[1]);
          const g = groupBy(before, a[0]);
          const out = agg(before, g, aggs);
          const keyCat = vIsCat(before, g.k);
          map = {
            key: a[0],
            groups: g.groups.map((rows, i) => ({
              key: keyCat ? vFormat(before, rows[0], g.k) : g.keys[i],
              rows,
            })),
            aggs: aggs.map((x, i) => ({ fn: x.fn, column: x.column, output: out.names[i + 1] })),
          };
          cpp = '.groupby(' + lit(a[0]) + ').agg({ ' +
            aggs.map((x) => '{ ' + lit(x.fn) + ', ' + lit(x.column) + ' }').join(', ') + ' })';
          const fid = push(out);
          cur = view(fid, out);
          storage = storageOf(out, fid);
        } else if (spec.op === 'join') {
          need(2);
          const rfid = push(read(a[0]));
          const right = view(rfid, frames[rfid]);
          const pairs = [];
          const out = join(before, right, a[1], pairs);
          const lhit = new Set(pairs.map((p) => p[0]));
          const rhit = new Set(pairs.map((p) => p[1]));
          map = {
            file: a[0],
            key: a[1],
            right: tableOf(right),
            pairs,
            unmatchedLeft: before.rows.map((_, i) => i).filter((i) => !lhit.has(i)),
            unmatchedRight: right.rows.map((_, i) => i).filter((i) => !rhit.has(i)),
          };
          cpp = '.join(readCSVFile(' + lit(a[0]) + ').view(), ' + lit(a[1]) + ')';
          const fid = push(out);
          cur = view(fid, out);
          storage = storageOf(out, fid);
        } else if (spec.op === 'describe') {
          const out = describe(before);
          const BINS = 8;
          const hist = {};
          before.cols.forEach((_, c) => {
            if (vIsCat(before, c)) return;
            let lo = Infinity;
            let hi = -Infinity;
            for (let r = 0; r < vNumRows(before); r++) {
              lo = Math.min(lo, vAt(before, r, c));
              hi = Math.max(hi, vAt(before, r, c));
            }
            const counts = new Array(BINS).fill(0);
            for (let r = 0; r < vNumRows(before); r++) {
              const b = hi > lo ? Math.floor(((vAt(before, r, c) - lo) / (hi - lo)) * BINS) : 0;
              counts[Math.min(b, BINS - 1)]++;
            }
            hist[vName(before, c)] = { min: lo, max: hi, counts };
          });
          map = { bins: BINS, hist };
          cpp = '.describe()';
          const fid = push(out);
          cur = view(fid, out);
          storage = storageOf(out, fid);
        } else {
          throw new Error("unknown step '" + spec.op + "'");
        }
      }
    } catch (e) {
      st.error = e.message;
      res.steps.push(st);
      res.ok = false;
      break;
    }
    const t1 = typeof performance !== 'undefined' ? performance.now() : 0;
    Object.assign(st, {
      cpp,
      stats: {
        rowsIn: before ? before.rows.length : 0,
        rowsOut: cur.rows.length,
        colsIn: before ? before.cols.length : 0,
        colsOut: cur.cols.length,
        micros: Math.round((t1 - t0) * 10000) / 10,
      },
      before: before ? tableOf(before) : null,
      after: tableOf(cur),
      view: { frame: cur.fid, rowIndices: cur.rows.slice(), columnIndices: cur.cols.slice() },
      map,
      storage,
    });
    res.steps.push(st);
  }
  return res;
}
