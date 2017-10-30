## Python 3

import enum
import string
import urllib


SHIM_URL = 'http://localhost:8080'
ID_CHARSET = string.digits + string.ascii_lowercase
MAX_SESSIONS = 50


class Shim(enum.Enum):
    cancel = 'cancel'
    execute_query = 'execute_query'
    new_session = 'new_session'
    read_bytes = 'read_bytes'
    release_session = 'release_session'
    upload = 'upload'


def shim_url(endpoint):
    return urllib.parse.urljoin(SHIM_URL, endpoint.value)


def check_shim_id(id):
    return len(id) == 32 and all(c in ID_CHARSET for c in id)


def check_scidb_id(id):
    return len(id) == 19 and all(c in string.digits for c in id)
