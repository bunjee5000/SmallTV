"""Test the compiled production math, using independent offline fixtures.

Usage: python tests/test_dashboard_math.py path/to/dashboard_math.dll (or .so)
"""
import ctypes as c
import datetime as dt
import json
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
library = c.CDLL(str(Path(sys.argv.pop(1)).resolve()))
NO_TIME = -32768

class Day(c.Structure):
    _fields_ = [('prayer', c.c_int16 * 5), ('sunrise', c.c_int16),
                ('sunset', c.c_int16), ('adjusted', c.c_uint8)]
class Hijri(c.Structure):
    _fields_ = [('year', c.c_int16), ('month', c.c_uint8), ('day', c.c_uint8)]
class Moon(c.Structure):
    _fields_ = [('index', c.c_uint8), ('cycle', c.c_float)]

library.dashboardCivilDay.argtypes = [c.c_int] * 3
library.dashboardCivilDay.restype = c.c_int32
library.dashboardHijri.argtypes = [c.c_int32, c.c_int]
library.dashboardHijri.restype = Hijri
library.dashboardMoon.argtypes = [c.c_int64]
library.dashboardMoon.restype = Moon
library.dashboardPrayerDay.argtypes = [c.c_int32] + [c.c_float] * 4 + [c.c_int] * 2
library.dashboardPrayerDay.restype = Day
library.dashboardNextPrayer.argtypes = [c.POINTER(Day), c.c_int32, c.c_int64,
                                        c.POINTER(c.c_int64), c.POINTER(c.c_uint8)]
library.dashboardNextPrayer.restype = c.c_int

def day(year, month, date):
    return library.dashboardCivilDay(year, month, date)
def prayer(d, lat=52.3759, lon=9.7320, fajr=18, isha=17, asr=1, rule=0):
    return library.dashboardPrayerDay(d, lat, lon, fajr, isha, asr, rule)
def fixture(name):
    return json.loads((ROOT / 'tests/fixtures' / name).read_text())
def minutes(value):
    h, m = map(int, value.split(':'))
    return h * 60 + m

class DashboardMathTests(unittest.TestCase):
    def test_civil_days_and_hijri(self):
        for row in fixture('hijri_civil_reference.json')['data']:
            y, m, date, hy, hm, hd = row
            d = day(y, m, date)
            self.assertEqual(d, (dt.date(y, m, date) - dt.date(1970, 1, 1)).days)
            result = library.dashboardHijri(d, 0)
            self.assertEqual((result.year, result.month, result.day), (hy, hm, hd), row)
            for correction in [-1, 1]:
                adjusted = library.dashboardHijri(d, correction)
                adjacent = library.dashboardHijri(d + correction, 0)
                self.assertEqual(bytes(adjusted), bytes(adjacent))

    def test_sun_against_usno(self):
        for reference in fixture('usno_sun_hannover_2026.json'):
            date = dt.date.fromisoformat(reference['date'])
            result = prayer(day(date.year, date.month, date.day))
            times = {v['phen']: minutes(v['time']) for v in reference['data']}
            for name, actual in [('Rise', result.sunrise), ('Set', result.sunset),
                                 ('Upper Transit', result.prayer[1])]:
                self.assertLessEqual(abs(actual - times[name]), 2, (reference['date'], name, actual, times[name]))

    def test_primary_moon_phases_against_usno(self):
        phases = {'New Moon': 0, 'First Quarter': 2, 'Full Moon': 4, 'Last Quarter': 6}
        worst = 0
        for ref in fixture('usno_moon_2026.json')['data']:
            h, m = map(int, ref['time'].split(':'))
            epoch = int(dt.datetime(ref['year'], ref['month'], ref['day'], h, m, tzinfo=dt.timezone.utc).timestamp())
            result = library.dashboardMoon(epoch)
            expected = phases[ref['phase']]
            self.assertEqual(result.index, expected, ref)
            error = abs(result.cycle - expected / 8)
            error = min(error, 1-error) * 360
            worst = max(worst, error)
            self.assertLess(error, 2, (ref, error))
        seen = {library.dashboardMoon((day(2026, 1, 1)+d)*86400).index for d in range(40)}
        self.assertEqual(seen, set(range(8)))
        print(f'Moon primary-phase maximum longitude error: {worst:.3f} degrees')

    def test_prayer_conventions_against_reference(self):
        angles = {'MWL': (18, 17), 'ISNA': (15, 15), 'Egypt': (19.5, 17.5), 'Karachi': (18, 18)}
        rules = {'AngleBased': 0, 'NightMiddle': 1, 'OneSeventh': 2}
        worst = 0
        for ref in fixture('prayer_reference.json')['data']:
            result = prayer(day(*ref['date']), fajr=angles[ref['method']][0], isha=angles[ref['method']][1],
                            asr=2 if ref['asr'] == 'Hanafi' else 1, rule=rules[ref['highLats']])
            for i, expected in enumerate(ref['minutes']):
                error = abs(result.prayer[i] - expected)
                worst = max(worst, error)
                self.assertLessEqual(error, 2, (ref, i, result.prayer[i], expected))
        print(f'Prayer reference maximum difference: {worst} minutes')

    def test_summer_and_polar_crossings(self):
        d = day(2026, 6, 21)
        raw = prayer(d, rule=3)
        self.assertEqual(raw.prayer[0], NO_TIME)
        self.assertEqual(raw.prayer[4], NO_TIME)
        for rule in [0, 1, 2]:
            adjusted = prayer(d, rule=rule)
            self.assertEqual(adjusted.adjusted, 17)
            self.assertLess(adjusted.prayer[0], adjusted.sunrise)
            self.assertGreater(adjusted.prayer[4], adjusted.sunset)
        polar = prayer(d, lat=89)
        self.assertEqual((polar.sunrise, polar.sunset), (NO_TIME, NO_TIME))
        self.assertNotEqual(polar.prayer[1], NO_TIME)
        winter = day(2026, 12, 21)
        self.assertGreater(prayer(winter, asr=2).prayer[2], prayer(winter, asr=1).prayer[2])

    def test_asr_unavailable_during_polar_winter(self):
        for month, sign in [(12, 1), (6, -1)]:
            d = day(2026, month, 21)
            for latitude in [68, 70, 75, 80, 85, 89]:
                for asr in [1, 2]:
                    for rule in range(4):
                        with self.subTest(month=month, latitude=sign*latitude, asr=asr, rule=rule):
                            result = prayer(d, lat=sign*latitude, lon=0, asr=asr, rule=rule)
                            self.assertEqual((result.sunrise, result.sunset), (NO_TIME, NO_TIME))
                            self.assertEqual(result.prayer[2], NO_TIME)
                            self.assertNotEqual(result.prayer[1], NO_TIME)
        # Twilight is still available in this reproduced case. Next-prayer
        # selection must skip the invalid Asr without suppressing Isha.
        d = day(2026, 12, 21)
        days = (Day * 3)(*(prayer(d+i-1, lat=80, lon=0) for i in range(3)))
        self.assertNotEqual(days[1].prayer[4], NO_TIME)
        epoch, adjusted = c.c_int64(), c.c_uint8()
        now = d*86400 + days[1].prayer[1]*60 + 1
        result = library.dashboardNextPrayer(days, d, now, c.byref(epoch), c.byref(adjusted))
        self.assertEqual(result, 4)
        self.assertEqual(epoch.value, d*86400 + days[1].prayer[4]*60)
        # An absent sunset during polar summer must not suppress a valid Asr.
        for month, latitude in [(6, 70), (12, -70)]:
            result = prayer(day(2026, month, 21), lat=latitude, lon=0)
            self.assertEqual(result.sunset, NO_TIME)
            self.assertNotEqual(result.prayer[2], NO_TIME)

    def test_next_prayer_at_boundaries(self):
        for month in [1, 6, 10, 12]:
            d = day(2026, month, 21)
            days = (Day * 3)(*(prayer(d+i-1) for i in range(3)))
            candidates = [( (d+i-1)*86400 + value*60, p )
                          for i, entry in enumerate(days) for p, value in enumerate(entry.prayer)
                          if value != NO_TIME]
            for hour in range(24):
                now = d*86400 + hour*3600
                expected = min(v for v in candidates if v[0] >= now)
                epoch, adjusted = c.c_int64(), c.c_uint8()
                result = library.dashboardNextPrayer(days, d, now, c.byref(epoch), c.byref(adjusted))
                self.assertEqual((epoch.value, result), expected)
            for epoch_value, name in candidates:
                epoch, adjusted = c.c_int64(), c.c_uint8()
                result = library.dashboardNextPrayer(days, d, epoch_value, c.byref(epoch), c.byref(adjusted))
                self.assertEqual(epoch.value, epoch_value)
        # Synthetic yesterday-Isha-after-midnight case (00:30 UTC).
        days = (Day * 3)()
        for entry in days:
            entry.prayer[:] = [NO_TIME]*5
        days[0].prayer[4] = 1470
        d = day(2026, 6, 21)
        epoch, adjusted = c.c_int64(), c.c_uint8()
        self.assertEqual(library.dashboardNextPrayer(days, d, d*86400, c.byref(epoch), c.byref(adjusted)), 4)
        self.assertEqual(epoch.value, d*86400+1800)

    def test_dst_epoch_countdowns(self):
        # Berlin jumps at 01:00 UTC on these dates. Absolute time advances by
        # one minute across either local-clock jump, never +/- one hour.
        for date in [(2026, 3, 29), (2026, 10, 25)]:
            d = day(*date)
            days = (Day * 3)(*(prayer(d+i-1) for i in range(3)))
            epochs=[]
            for minute in [59, 60]:
                now=d*86400+minute*60
                epoch, adjusted=c.c_int64(), c.c_uint8()
                library.dashboardNextPrayer(days,d,now,c.byref(epoch),c.byref(adjusted))
                epochs.append((epoch.value,epoch.value-now))
            self.assertEqual(epochs[0][0],epochs[1][0])
            self.assertEqual(epochs[0][1]-epochs[1][1],60)

if __name__ == '__main__':
    unittest.main()
