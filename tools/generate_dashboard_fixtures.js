// Run with the author's published praytime.js path as argv[2]. This calculator
// is used only to generate test data, never linked into the firmware.
const fs = require('fs');
const path = require('path');
const crypto = require('crypto');
const { PrayTime } = require(path.resolve(process.argv[2]));
const directory = path.resolve(__dirname, '../tests/fixtures');
fs.mkdirSync(directory, { recursive: true });
const data = [];
for (let month = 1; month <= 12; month++) {
  for (const method of ['MWL', 'ISNA', 'Egypt', 'Karachi']) {
    for (const asr of ['Standard', 'Hanafi']) {
      for (const highLats of ['AngleBased', 'NightMiddle', 'OneSeventh']) {
        const calc = new PrayTime(method);
        calc.adjust({ asr, highLats, maghrib: '0 min', dhuhr: '0 min', iterations: 3 });
        calc.location([52.3759, 9.7320]).utcOffset(0).format('X');
        const date = [2026, month, 21];
        const times = calc.getTimes(date);
        const midnight = Date.UTC(2026, month - 1, 21) / 1000;
        data.push({ date, method, asr, highLats,
          minutes: ['fajr', 'dhuhr', 'asr', 'maghrib', 'isha'].map(k => Math.round((times[k] - midnight) / 60)) });
      }
    }
  }
}
const reference = { source: 'https://github.com/zarrabi/praytime/blob/main/src/praytime.js',
  sha256: crypto.createHash('sha256').update(fs.readFileSync(process.argv[2])).digest('hex'),
  note: 'v3.2, UTC, three iterations, zero Dhuhr/Maghrib offsets; nighttime from same-date rise/set in reference.', data };
function writeFixture(file, object) {
  const { data, ...metadata } = object;
  const prefix = JSON.stringify(metadata, null, 2).slice(0, -2);
  fs.writeFileSync(path.join(directory, file), prefix + ',\n  "data": [\n' +
    data.map(row => '    ' + JSON.stringify(row)).join(',\n') + '\n  ]\n}\n');
}
writeFixture('prayer_reference.json', reference);
const calendar = new Intl.DateTimeFormat('en-u-ca-islamic-civil-nu-latn', {
  timeZone: 'UTC', year: 'numeric', month: 'numeric', day: 'numeric' });
const hijri = [];
for (let year = 2020; year <= 2035; year++) for (let month = 1; month <= 12; month++) {
  for (const day of [1, 28]) {
    const parts = Object.fromEntries(calendar.formatToParts(new Date(Date.UTC(year, month - 1, day))).map(p => [p.type, p.value]));
    hijri.push([year, month, day, +parts.year, +parts.month, +parts.day]);
  }
}
writeFixture('hijri_civil_reference.json', { source: 'Node Intl / ICU islamic-civil calendar', node: process.version, data: hijri });
console.log(`Generated ${data.length} prayer and ${hijri.length} civil Hijri reference cases.`);
