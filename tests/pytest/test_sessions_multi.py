# Python 3

import pytest
import requests
import os

from settings import *


# py.test -n N
N = int(os.environ.get('PYTEST_XDIST_WORKER_COUNT'))


queries = (
    """
store(
  build(<x:int64>[i=1:{sz}], i),
  ar_{qid}_{sid})""",
    """
store(
  apply(
    build(<x:int64>[i=1:{sz}], i),
    y, x * x),
  ar_{qid}_{sid})""",
    """
store(
  cross_join(
    build(<x:int64>[i=1:{sz}], i),
    build(<y:double>[i=1:{sz}], i * i)),
  ar_{qid}_{sid})""",
)


@pytest.fixture()
def array_size(worker_id):
    return 100 * (int(worker_id[2:]) + 1) ** 2


@pytest.mark.parametrize('_', range(N))
def test_batch(_, array_size):
    # Split sessions equally among the parallel threads
    cnt = int(MAX_SESSIONS / N)
    sids = []

    # Get all the session allocated to this thread
    for i in range(cnt):
        req = requests.get(shim_url(Shim.new_session))
        assert req.status_code == 200
        assert check_shim_id(req.text)
        sids.append(req.text)

    # Run the following twice
    for j in range(2):
        # For each session, run one of the available queries
        for i in range(cnt):
            qid = (i + j) % len(queries)
            req = requests.get(
                shim_url(Shim.execute_query),
                params={'id': sids[i],
                        'query': queries[qid].format(sz=array_size,
                                                     sid=sids[i],
                                                     qid=qid)})
            assert req.status_code == 200
            assert check_scidb_id(req.text)

        # Delete the array of each session
        # for i in range(cnt):
        #     qid = (i + j) % len(queries)
        #     req = requests.get(
        #         shim_url(Shim.execute_query),
        #         params={'id': sids[i],
        #                 'query': 'remove(ar_{qid}_{sid})'.format(qid=qid,
        #                                                          sid=sids[i])})
        #     assert req.status_code == 200
        #     assert check_scidb_id(req.text)

    # Releases all the allocated sessions
    for i in range(cnt):
        req = requests.get(shim_url(Shim.release_session),
                           params={'id': sids[i]})
        assert req.status_code == 200
        assert req.text == ''
