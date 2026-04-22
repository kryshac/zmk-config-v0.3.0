#!/usr/bin/env node

const fs = require("node:fs");
const path = require("node:path");

const DEFAULT_DTS = "dts/input/processors/speed_scaler.dtsi";

function parseArgs(argv) {
  const args = {
    dts: DEFAULT_DTS,
    svg: "trackball-speed-scale.svg",
    csv: "trackball-speed-scale.csv",
    maxSpeed: 2000,
    step: 10,
  };

  for (let i = 0; i < argv.length; i++) {
    const arg = argv[i];
    const next = argv[i + 1];

    if (arg === "--dts" && next != null) {
      args.dts = next;
      i++;
    } else if (arg === "--svg" && next != null) {
      args.svg = next;
      i++;
    } else if (arg === "--csv" && next != null) {
      args.csv = next;
      i++;
    } else if (arg === "--max-speed" && next != null) {
      args.maxSpeed = Number.parseInt(next, 10);
      i++;
    } else if (arg === "--step" && next != null) {
      args.step = Number.parseInt(next, 10);
      i++;
    } else if (arg === "-h" || arg === "--help") {
      printHelp();
      process.exit(0);
    } else {
      throw new Error(`unknown or incomplete argument: ${arg}`);
    }
  }

  return args;
}

function printHelp() {
  console.log(`Usage: node tools/trackball_speed_curve.js [options]

Options:
  --dts PATH          DTS file to read, default ${DEFAULT_DTS}
  --svg PATH          SVG output path, default trackball-speed-scale.svg
  --csv PATH          CSV output path, default trackball-speed-scale.csv
  --max-speed VALUE   Graph max speed, default 2000
  --step VALUE        CSV/polyline speed step, default 10
`);
}

function readProp(text, name) {
  const match = text.match(new RegExp(`\\b${name}\\s*=\\s*<(-?\\d+)>;`));
  if (!match) {
    throw new Error(`missing DTS property: ${name}`);
  }
  return Number.parseInt(match[1], 10);
}

function loadConfig(dtsPath) {
  const text = fs.readFileSync(dtsPath, "utf8");
  return {
    lowSpeed: readProp(text, "low-speed"),
    highSpeed: readProp(text, "high-speed"),
    curveExponent: readProp(text, "curve-exponent"),
    minMul: readProp(text, "slow-multiplier"),
    minDiv: readProp(text, "slow-divisor"),
    maxMul: readProp(text, "fast-multiplier"),
    maxDiv: readProp(text, "fast-divisor"),
  };
}

function scaleAt(speed, cfg) {
  const minScale = cfg.minMul / cfg.minDiv;
  const maxScale = cfg.maxMul / cfg.maxDiv;

  if (speed <= cfg.lowSpeed) {
    return minScale;
  }

  if (speed >= cfg.highSpeed) {
    return maxScale;
  }

  const span = cfg.highSpeed - cfg.lowSpeed;
  const t = (speed - cfg.lowSpeed) / span;
  return minScale + (maxScale - minScale) * t ** cfg.curveExponent;
}

function ensureParent(filePath) {
  const dir = path.dirname(filePath);
  if (dir && dir !== ".") {
    fs.mkdirSync(dir, { recursive: true });
  }
}

function writeCsv(filePath, cfg, maxSpeed, step) {
  const rows = ["speed,scale"];
  for (let speed = 0; speed <= maxSpeed; speed += step) {
    rows.push(`${speed},${scaleAt(speed, cfg).toFixed(6)}`);
  }

  ensureParent(filePath);
  fs.writeFileSync(filePath, `${rows.join("\n")}\n`, "utf8");
}

function svgPolyline(cfg, maxSpeed, width, height, step) {
  const points = [];
  for (let speed = 0; speed <= maxSpeed; speed += step) {
    const x = (speed / maxSpeed) * width;
    const y = (1.0 - scaleAt(speed, cfg)) * height;
    points.push(`${x.toFixed(3)},${y.toFixed(3)}`);
  }
  return points.join(" ");
}

function writeSvg(filePath, cfg, maxSpeed, step) {
  const width = 620;
  const height = 320;
  const lowX = (cfg.lowSpeed / maxSpeed) * width;
  const highX = (cfg.highSpeed / maxSpeed) * width;
  const polyline = svgPolyline(cfg, maxSpeed, width, height, step);
  const minScale = cfg.minMul / cfg.minDiv;
  const maxScale = cfg.maxMul / cfg.maxDiv;

  const svg = `<svg xmlns="http://www.w3.org/2000/svg" width="760" height="460" viewBox="0 0 760 460">
  <title>Trackball Speed Scale</title>
  <desc>Generated speed scale graph from ${DEFAULT_DTS}.</desc>
  <rect width="760" height="460" fill="#ffffff"/>

  <g font-family="Arial, sans-serif" font-size="14" fill="#222">
    <text x="380" y="34" text-anchor="middle" font-size="22" font-weight="700">Trackball Speed Scale</text>
    <text x="380" y="58" text-anchor="middle" fill="#555">min=${minScale.toFixed(3)}x max=${maxScale.toFixed(3)}x exponent=${cfg.curveExponent}</text>
  </g>

  <g transform="translate(84 54)">
    <rect x="0" y="0" width="${width}" height="${height}" fill="#fafafa" stroke="#ddd"/>
    <g stroke="#e3e3e3" stroke-width="1">
      <line x1="0" y1="256" x2="${width}" y2="256"/>
      <line x1="0" y1="192" x2="${width}" y2="192"/>
      <line x1="0" y1="128" x2="${width}" y2="128"/>
      <line x1="0" y1="64" x2="${width}" y2="64"/>
      <line x1="0" y1="0" x2="0" y2="${height}"/>
      <line x1="${(width * 0.25).toFixed(3)}" y1="0" x2="${(width * 0.25).toFixed(3)}" y2="${height}"/>
      <line x1="${(width * 0.5).toFixed(3)}" y1="0" x2="${(width * 0.5).toFixed(3)}" y2="${height}"/>
      <line x1="${(width * 0.75).toFixed(3)}" y1="0" x2="${(width * 0.75).toFixed(3)}" y2="${height}"/>
      <line x1="${width}" y1="0" x2="${width}" y2="${height}"/>
    </g>
    <g stroke="#222" stroke-width="2">
      <line x1="0" y1="${height}" x2="${width}" y2="${height}"/>
      <line x1="0" y1="0" x2="0" y2="${height}"/>
    </g>
    <g stroke="#d94545" stroke-width="2" stroke-dasharray="6 5">
      <line x1="${lowX.toFixed(3)}" y1="0" x2="${lowX.toFixed(3)}" y2="${height}"/>
      <line x1="${highX.toFixed(3)}" y1="0" x2="${highX.toFixed(3)}" y2="${height}"/>
    </g>
    <polyline points="${polyline}" fill="none" stroke="#1167b1" stroke-width="4" stroke-linejoin="round" stroke-linecap="round"/>
    <g font-family="Arial, sans-serif" font-size="13" fill="#222">
      <text x="-12" y="324" text-anchor="end">0</text>
      <text x="-12" y="260" text-anchor="end">0.2</text>
      <text x="-12" y="196" text-anchor="end">0.4</text>
      <text x="-12" y="132" text-anchor="end">0.6</text>
      <text x="-12" y="68" text-anchor="end">0.8</text>
      <text x="-12" y="4" text-anchor="end">1.0</text>
      <text x="${lowX.toFixed(3)}" y="-10" text-anchor="middle" fill="#d94545">low ${cfg.lowSpeed}</text>
      <text x="${highX.toFixed(3)}" y="-10" text-anchor="middle" fill="#d94545">high ${cfg.highSpeed}</text>
      <text x="${(width / 2).toFixed(3)}" y="344" text-anchor="middle">${Math.floor(maxSpeed / 2)}</text>
      <text x="${width}" y="344" text-anchor="middle">${maxSpeed}</text>
    </g>
    <text x="${(width / 2).toFixed(3)}" y="388" text-anchor="middle" font-family="Arial, sans-serif" font-size="15" fill="#222">Speed estimate</text>
    <text x="-48" y="160" text-anchor="middle" transform="rotate(-90 -48 160)" font-family="Arial, sans-serif" font-size="15" fill="#222">Output scale</text>
  </g>
</svg>
`;

  ensureParent(filePath);
  fs.writeFileSync(filePath, svg, "utf8");
}

function main() {
  const args = parseArgs(process.argv.slice(2));
  const cfg = loadConfig(args.dts);
  writeCsv(args.csv, cfg, args.maxSpeed, args.step);
  writeSvg(args.svg, cfg, args.maxSpeed, args.step);
}

try {
  main();
} catch (error) {
  console.error(error.message);
  process.exit(1);
}
