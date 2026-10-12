// Exercise the real embedded UI's load/collect path without a browser or device.
const fs = require('fs');
const vm = require('vm');
const assert = require('assert');
const html = fs.readFileSync('src/webui.html', 'utf8');
const elements = new Map();
for (const match of html.matchAll(/\bid="([^"]+)"/g)) {
  assert(!elements.has(match[1]), `Duplicate HTML id ${match[1]}`);
  const element = { value: '', checked: false, style: {}, classList: { add() {}, remove() {} },
    querySelector() { return null; }, appendChild() {}, closest() { return this; }, remove() { elements.delete(match[1]); } };
  let value = '';
  Object.defineProperty(element, 'value', { get: () => value, set: v => { value = String(v); } });
  elements.set(match[1], element);
}
const context = vm.createContext({ console, Date, Math, JSON, Promise,
  setTimeout() {}, setInterval() {}, window: {},
  document: { getElementById: id => elements.get(id) || null,
    querySelector: () => null, querySelectorAll: () => [], createElement: () => ({}) } });
const script = html.match(/<script>([\s\S]*?)<\/script>/)[1]
  .replace('loadConfig().then(loadStatus);', '').replace('setInterval(loadStatus,5000);', '');
vm.runInContext(script, context); // also a syntax check for the full embedded script
context.renderWifi = () => {};
context.renderSymbols = () => {};
context.renderSyms = () => {};
context.symRows = () => {};
context.loadThemes = () => {};
context.renderAps = () => {};
async function load(config) {
  context.j = () => Promise.resolve(config);
  await context.loadConfig();
  return JSON.parse(JSON.stringify(context.collect()));
}
(async () => {
  const baseline = { mode: 'carousel', carouselClock: true, carouselRadar: true,
    brightness: 50, rotation: 0, ticker: {}, clock: { tz: 'Europe/Berlin' }, wifi: [] };
  let result = await load(baseline);
  assert.deepStrictEqual(result.dashboard, { enabled: true, weather: true, countdown: true,
    lat: 52.3759, lon: 9.732, method: 0, asr: 0, highLatitude: 0, hijriCorrection: 0,
    fajrAngle: 18, ishaAngle: 17 });
  assert.strictEqual(result.carouselClock, true);
  assert.strictEqual(result.carouselRadar, true);
  assert.strictEqual(result.clock.tzPosix, 'CET-1CEST,M3.5.0,M10.5.0/3');
  const custom = { enabled: false, weather: false, countdown: false,
    lat: 0, lon: 0, method: 4, asr: 1, highLatitude: 3, hijriCorrection: -1,
    fajrAngle: 18.5, ishaAngle: 17.5 };
  result = await load({ ...baseline, dashboard: custom });
  assert.deepStrictEqual(result.dashboard, custom);
  assert.strictEqual(elements.get('dashAngles').style.display, 'flex');
  elements.get('dashMethod').value = '0';
  context.dashMethodChanged();
  assert.strictEqual(elements.get('dashAngles').style.display, 'none');
  const zone = new Intl.DateTimeFormat('en-GB', { timeZone: 'Europe/Berlin', hour: '2-digit', minute: '2-digit', hour12: false });
  assert.strictEqual(zone.format(new Date('2026-03-29T00:59:00Z')), '01:59');
  assert.strictEqual(zone.format(new Date('2026-03-29T01:00:00Z')), '03:00');
  assert.strictEqual(zone.format(new Date('2026-10-25T00:59:00Z')), '02:59');
  assert.strictEqual(zone.format(new Date('2026-10-25T01:00:00Z')), '02:00');
  console.log('Embedded UI dashboard defaults, settings round-trip, carousel, timezone and DST checks: PASS');
})().catch(error => { console.error(error); process.exitCode = 1; });
