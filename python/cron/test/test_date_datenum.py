import datetime

import pytest

import cron
from   cron import *

import data

#-------------------------------------------------------------------------------

def test_datenum0():
    assert Date.MIN.datenum  == cron.DATENUM_MIN
    assert (1/Jan/2).datenum ==   1
    assert (1/Feb/1).datenum ==  31
    assert (2/Jan/1).datenum == 365
    assert Date.MAX.datenum == cron.DATENUM_MAX


def test_datenum_round_trip():
    """
    Samples datenums, making sure a round trip through date is idempotent.
    """
    for datenum in range(cron.DATENUM_MIN, cron.DATENUM_MAX + 1, 37):
        assert Date.from_datenum(datenum).datenum == datenum


def test_vs_toordinal():
    """
    Tests that datenum is one less than datetime.date's (1-indexed) ordinal.
    """
    for parts in data.TEST_DATE_PARTS:
        datenum = Date.from_parts(*parts).datenum
        ordinal = datetime.date(*parts).toordinal()
        assert datenum + 1 == ordinal


