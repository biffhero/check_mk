#!/usr/bin/env python
# encoding: utf-8

import pytest
import livestatus
import time
from testlib import web, ec

def ensure_core_and_get_connection(site, ec, core):
    if core != None:
        site.set_config("CORE", core, with_restart=True)
        live = site.live
    else:
        live = ec.status

    return live


# TODO: Enable this once the livestatus columns have been added for status_event_limit_*
# @pytest.mark.parametrize(("core"), [ "nagios", "cmc" ])
@pytest.mark.parametrize(("core"), [ "cmc" ])
def test_command_reload(site, ec, core):
    live = ensure_core_and_get_connection(site, ec, core)

    old_t = live.query_value("GET eventconsolestatus\nColumns: status_config_load_time\n")
    assert old_t > time.time() - 86400

    time.sleep(1) # needed to have at least one second after EC start
    live.command("[%d] EC_RELOAD" % (int(time.time())))
    time.sleep(1) # needed to have at least one second after EC reload

    new_t = live.query_value("GET eventconsolestatus\nColumns: status_config_load_time\n")
    assert new_t > old_t


# core == None means direct query to status socket
# TODO: Enable this once the livestatus columns have been added for status_event_limit_*
# @pytest.mark.parametrize(("core"), [ None, "nagios", "cmc" ])
@pytest.mark.parametrize(("core"), [ None ])
def test_status_table_via_core(site, ec, core):
    live = ensure_core_and_get_connection(site, ec, core)

    result = live.query_table_assoc("GET status\n")
    assert len(result) == 1

    status = result[0]

    for column_name in [
            'status_config_load_time',
            'status_num_open_events',
            'status_messages',
            'status_message_rate',
            'status_average_message_rate',
            'status_connects',
            'status_connect_rate',
            'status_average_connect_rate',
            'status_rule_tries',
            'status_rule_trie_rate',
            'status_average_rule_trie_rate',
            'status_drops',
            'status_drop_rate',
            'status_average_drop_rate',
            'status_events',
            'status_event_rate',
            'status_average_event_rate',
            'status_rule_hits',
            'status_rule_hit_rate',
            'status_average_rule_hit_rate',
            'status_average_processing_time',
            'status_average_request_time',
            'status_average_sync_time',
            'status_replication_slavemode',
            'status_replication_last_sync',
            'status_replication_success',
            'status_event_limit_host',
            'status_event_limit_rule',
            'status_event_limit_overall',
        ]:
        assert column_name in status

    assert type(status["status_event_limit_host"]) == int
    assert type(status["status_event_limit_rule"]) == int
    assert type(status["status_event_limit_overall"]) == int
