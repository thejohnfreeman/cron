import pytest

import cron
from   cron import *

#-------------------------------------------------------------------------------

def test_utc():
    assert UTC.name == "UTC"


def test_name():
    assert TimeZone("US/Eastern").name == "US/Eastern"


def test_us_central():
    tz = TimeZone("US/Central")
    H = 3600
    assert tz.at_local(1960/Jan/ 1, MIDNIGHT) == (-6 * H, "CST", False)
    assert tz.at_local(1960/Jul/ 1, MIDNIGHT) == (-5 * H, "CDT", True)
    assert tz.at_local(1969/Jan/ 1, MIDNIGHT) == (-6 * H, "CST", False)
    assert tz.at_local(1969/Jul/ 1, MIDNIGHT) == (-5 * H, "CDT", True)
    assert tz.at_local(1970/Jan/ 1, MIDNIGHT) == (-6 * H, "CST", False)
    assert tz.at_local(1970/Jul/ 1, MIDNIGHT) == (-5 * H, "CDT", True)
    assert tz.at_local(1971/Jan/ 1, MIDNIGHT) == (-6 * H, "CST", False)
    assert tz.at_local(1971/Jul/ 1, MIDNIGHT) == (-5 * H, "CDT", True)
    assert tz.at_local(1980/Jan/ 1, MIDNIGHT) == (-6 * H, "CST", False)
    assert tz.at_local(1980/Jul/ 1, MIDNIGHT) == (-5 * H, "CDT", True)
    assert tz.at_local(2000/Jan/ 1, MIDNIGHT) == (-6 * H, "CST", False)
    assert tz.at_local(2000/Jul/ 1, MIDNIGHT) == (-5 * H, "CDT", True)
    assert tz.at_local(2020/Jan/ 1, MIDNIGHT) == (-6 * H, "CST", False)
    assert tz.at_local(2020/Jul/ 1, MIDNIGHT) == (-5 * H, "CDT", True)


def test_call_time():
    tz = TimeZone("US/Eastern")
    t0 = (2016/Jan/1, MIDNIGHT) @ tz
    t1 = (2016/Jul/1, MIDNIGHT) @ tz
    t2 = (2016/Dec/1, MIDNIGHT) @ tz

    o = tz(t0)
    assert o.offset == -18000
    assert o.abbreviation == "EST"
    assert not o.is_dst

    assert tz.at(t0) == (-18000, "EST", False)
    assert tz.at(t1) == (-14400, "EDT", True)
    assert tz.at(t2) == (-18000, "EST", False)


def test_call_local_time():
    tz = TimeZone("US/Eastern")

    o = tz((2016/Jan/1, MIDNIGHT))
    assert o.offset == -18000
    assert o.abbreviation == "EST"
    assert not o.is_dst

    assert tz(2016/Jan/1, MIDNIGHT) == (-18000, "EST", False)
    assert tz(2016/Jul/1, MIDNIGHT) == (-14400, "EDT", True)
    assert tz(2016/Dec/1, MIDNIGHT) == (-18000, "EST", False)


def test_at():
    tz = TimeZone("US/Eastern")
    t0 = (2016/Jan/1, MIDNIGHT) @ tz  # EST
    t1 = (2016/Jul/1, MIDNIGHT) @ tz  # EDT
    t2 = (2016/Dec/1, MIDNIGHT) @ tz  # back to EST

    o = tz.at(t0)
    assert o.offset == -18000
    assert o.abbreviation == "EST"
    assert not o.is_dst

    assert tz.at(t0) == (-18000, "EST", False)
    assert tz.at(t1) == (-14400, "EDT", True)
    assert tz.at(t2) == (-18000, "EST", False)


def test_at_local():
    tz = TimeZone("US/Eastern")

    o = tz.at_local((2016/Jan/1, MIDNIGHT))
    assert o.offset == -18000
    assert o.abbreviation == "EST"
    assert not o.is_dst

    assert tz.at_local(2016/Jan/1, MIDNIGHT) == (-18000, "EST", False)
    assert tz.at_local(2016/Jul/1, MIDNIGHT) == (-14400, "EDT", True)
    assert tz.at_local(2016/Dec/1, MIDNIGHT) == (-18000, "EST", False)


def test_dst_transition():
    tz = TimeZone("US/Eastern")
    EST = -18000, "EST", False
    EDT = -14400, "EDT", True

    assert tz(2016/Mar/13, Daytime( 1,  0,  0)) == EST
    assert tz(2016/Mar/13, Daytime( 1, 59, 59)) == EST
    # At 2 AM, clocks spring forward one hour.
    assert tz(2016/Mar/13, Daytime( 3,  0,  0)) == EDT

    assert tz(2016/Nov/ 6, Daytime( 0,  0,  0)             ) == EDT
    assert tz(2016/Nov/ 6, Daytime( 0, 59, 59), first=True ) == EDT
    assert tz(2016/Nov/ 6, Daytime( 0, 59, 59), first=False) == EDT
    assert tz(2016/Nov/ 6, Daytime( 1,  0,  0), first=True ) == EDT
    assert tz(2016/Nov/ 6, Daytime( 1, 59, 59), first=True ) == EDT
    # At 2 AM, clocks fall back one hour.
    assert tz(2016/Nov/ 6, Daytime( 1,  0,  0), first=False) == EST
    assert tz(2016/Nov/ 6, Daytime( 1, 59, 59), first=False) == EST
    assert tz(2016/Nov/ 6, Daytime( 2,  0,  0), first=True ) == EST
    assert tz(2016/Nov/ 6, Daytime( 2,  0,  0), first=False) == EST


def test_nonexistent():
    # Should raise an exception from a nonexistent time.
    tz = TimeZone("US/Eastern")
    date = 2016/Mar/13
    daytime = Daytime(2, 30)

    with pytest.raises(ValueError):
        tz(date, daytime)
    with pytest.raises(ValueError):
        tz((date, daytime))
    with pytest.raises(ValueError):
        tz.at_local(date, daytime)
    with pytest.raises(ValueError):
        tz.at_local((date, daytime))




