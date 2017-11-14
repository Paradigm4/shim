## Python 3

import pytest
import requests

from settings import *


@pytest.mark.parametrize('i', range(100))
def test_new_session(i):
    req = requests.get(shim_url(Shim.new_session))
    assert req.status_code == 200
    assert check_shim_id(req.text)
    id = req.text

    req = requests.get(shim_url(Shim.release_session),
                       params={'id': id})
    assert req.status_code == 200
    assert req.text == ''



@pytest.mark.parametrize('i', range(10))
def test_max_new_sessions(i):
    # Request All
    ids = []
    for i in range(MAX_SESSIONS):
        req = requests.get(shim_url(Shim.new_session))
        assert req.status_code == 200
        assert check_shim_id(req.text)
        ids.append(req.text)

    # Request New -- Fail
    req = requests.get(shim_url(Shim.new_session))
    assert req.status_code == 503
    assert req.text == 'Out of resources'

    # Relase One
    req = requests.get(shim_url(Shim.release_session),
                       params={'id': ids[0]})
    assert req.status_code == 200
    assert req.text == ''

    # Request New
    req = requests.get(shim_url(Shim.new_session))
    assert req.status_code == 200
    assert check_shim_id(req.text)
    id_new = req.text

    # Request New -- Fail
    req = requests.get(shim_url(Shim.new_session))
    assert req.status_code == 503
    assert req.text == 'Out of resources'

    # Release Remaining
    for id in ids[1:]:
        req = requests.get(shim_url(Shim.release_session),
                           params={'id': id})
        assert req.status_code == 200
        assert req.text == ''

    # Release New
    req = requests.get(shim_url(Shim.release_session),
                       params={'id': id_new})
    assert req.status_code == 200
    assert req.text == ''
