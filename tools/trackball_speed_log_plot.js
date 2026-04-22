#!/usr/bin/env node

const fs = require("node:fs");
const path = require("node:path");

const LOG_RE =
  /TB_SPEED\s+ms=(?<ms>\d+)\s+listener=(?<listener>-?\d+)\s+axis=(?<axis>\S+)\s+code=(?<code>-?\d+)\s+raw=(?<raw>-?\d+)\s+scaled=(?<scaled>-?\d+)\s+dt=(?<dt>\d+)\s+speed=(?<speed>\d+)\s+scale_permill=(?<scale_permill>\d+)\s+remainder=(?<remainder>-?\d+)/;

function parseArgs(argv) {
  const args = {
    log: null,
    csv: "trackball-live-speed.csv",
    svg: "trackball-live-speed.svg",
  };

  for (let i = 0; i < argv.length; i++) {
    const arg = argv[i];
    const next = argv[i + 1];

    if (arg === "--csv" && next != null) {
      args.csv = next;
      i++;
    } else if (arg === "--svg" && next != null) {
      args.svg = next;
      i++;
    } else if (arg === "-h" || arg === "--help") {
      printHelp();
      process.exit(0);
    } else if (arg.startsWith("--")) {
      throw new Error(`unknown or incomplete argument: ${arg}`);
    } else if (args.log == null) {
      args.log = arg;
    } else {
      throw new Error(`unexpected argument: ${arg}`);
    }
  }

  return args;
}

function printHelp() {
  console.log(`Usage: node tools/trackball_speed_log_plot.js [firmware.log] [options]

If firmware.log is omitted, stdin is used.

Options:
  --csv PATH   CSV output path, default trackball-live-speed.csv
  --svg PATH   SVG output path, default trackball-live-speed.svg
`);
}

function readStdin() {
  return fs.readFileSync(0, "utf8");
}

function readRecords(logPath) {
  const text = logPath == null ? readStdin() : fs.readFileSync(logPath, "utf8");
  const records = [];

  for (const line of text.split(/\r?\n/)) {
    const match = line.match(LOG_RE);
    if (!match || !match.groups) {
      continue;
    }

    const record = { axis: match.groups.axis };
    for (const key of [
      "ms",
      "listener",
      "code",
      "raw",
      "scaled",
      "dt",
      "speed",
      "scale_permill",
      "remainder",
    ]) {
      record[key] = Number.parseInt(match.groups[key], 10);
    }
    records.push(record);
  }

  return records;
}

function ensureParent(filePath) {
  const dir = path.dirname(filePath);
  if (dir && dir !== ".") {
    fs.mkdirSync(dir, { recursive: true });
  }
}

function writeCsv(filePath, records) {
  const rows = ["sample,ms,axis,raw,scaled,dt,speed,scale,remainder"];

  records.forEach((record, sample) => {
    rows.push(
      [
        sample,
        record.ms,
        record.axis,
        record.raw,
        record.scaled,
        record.dt,
        record.speed,
        (record.scale_permill / 1000).toFixed(3),
        record.remainder,
      ].join(","),
    );
  });

  ensureParent(filePath);
  fs.writeFileSync(filePath, `${rows.join("\n")}\n`, "utf8");
}

function seriesPoints(records, field, width, height) {
  if (records.length === 0) {
    return "";
  }

  const values = records.map((record) => record[field]);
  const maxValue = Math.max(...values) || 1;
  const count = Math.max(records.length - 1, 1);

  return values
    .map((value, index) => {
      const x = (index / count) * width;
      const y = height - (value / maxValue) * height;
      return `${x.toFixed(3)},${y.toFixed(3)}`;
    })
    .join(" ");
}

function writeSvg(filePath, records) {
  const width = 660;
  const panelHeight = 140;
  const maxSpeed = records.reduce((max, record) => Math.max(max, record.speed), 0);
  const maxScale = records.reduce((max, record) => Math.max(max, record.scale_permill), 0);
  const speedPoints = seriesPoints(records, "speed", width, panelHeight);
  const scalePoints = seriesPoints(records, "scale_permill", width, panelHeight);

  const svg = `<svg xmlns="http://www.w3.org/2000/svg" width="820" height="460" viewBox="0 0 820 460">
  <title>Trackball Live Speed Debug</title>
  <rect width="820" height="460" fill="#ffffff"/>
  <g font-family="Arial, sans-serif" fill="#222">
    <text x="410" y="34" text-anchor="middle" font-size="22" font-weight="700">Trackball Live Speed Debug</text>
    <text x="410" y="58" text-anchor="middle" font-size="14" fill="#555">${records.length} captured input events</text>
  </g>

  <g transform="translate(96 86)">
    <text x="0" y="-12" font-family="Arial, sans-serif" font-size="14" fill="#222">Estimated speed, max ${maxSpeed}</text>
    <rect x="0" y="0" width="${width}" height="${panelHeight}" fill="#fafafa" stroke="#ddd"/>
    <polyline points="${speedPoints}" fill="none" stroke="#1167b1" stroke-width="3" stroke-linejoin="round" stroke-linecap="round"/>
    <text x="-12" y="5" text-anchor="end" font-family="Arial, sans-serif" font-size="12" fill="#555">${maxSpeed}</text>
    <text x="-12" y="${panelHeight}" text-anchor="end" font-family="Arial, sans-serif" font-size="12" fill="#555">0</text>
  </g>

  <g transform="translate(96 284)">
    <text x="0" y="-12" font-family="Arial, sans-serif" font-size="14" fill="#222">Applied scale, max ${(maxScale / 1000).toFixed(3)}x</text>
    <rect x="0" y="0" width="${width}" height="${panelHeight}" fill="#fafafa" stroke="#ddd"/>
    <polyline points="${scalePoints}" fill="none" stroke="#2f855a" stroke-width="3" stroke-linejoin="round" stroke-linecap="round"/>
    <text x="-12" y="5" text-anchor="end" font-family="Arial, sans-serif" font-size="12" fill="#555">${(maxScale / 1000).toFixed(3)}</text>
    <text x="-12" y="${panelHeight}" text-anchor="end" font-family="Arial, sans-serif" font-size="12" fill="#555">0</text>
    <text x="${(width / 2).toFixed(3)}" y="174" text-anchor="middle" font-family="Arial, sans-serif" font-size="14" fill="#222">Captured event order</text>
  </g>
</svg>
`;

  ensureParent(filePath);
  fs.writeFileSync(filePath, svg, "utf8");
}

function main() {
  const args = parseArgs(process.argv.slice(2));
  const records = readRecords(args.log);

  if (records.length === 0) {
    throw new Error("no TB_SPEED log lines found");
  }

  writeCsv(args.csv, records);
  writeSvg(args.svg, records);
}

try {
  main();
} catch (error) {
  console.error(error.message);
  process.exit(1);
}
