# triggerview.py

import redis
from urllib.parse import urlsplit

#signals = '()'
#expression = '()'
#width = 20

rdb = None
redis_url = None

# 読み出す Key
key = "parameters:LogicFilter"

# Hash の全フィールドを取得
#data = r.hgetall(key)

def _setup(app):
    global redis_url
    redis_url = app.project.config["data_source"]["parameters"]["url"]

def _initialize(params=None):
    # Share the data source host and port; keep parameter storage in DB 2.
    global rdb
    if not redis_url:
        raise ValueError('Redis URL is not configured')
    connection = urlsplit(redis_url)
    if connection.scheme != 'redis' or not connection.hostname:
        raise ValueError('Specify a redis://host:port/ URL')
    rdb = redis.Redis(
        host=connection.hostname,
        port=connection.port or 6379,
        db=2,
        decode_responses=True,
        socket_connect_timeout=2,
        socket_timeout=2
    )

    
def _get_channels():
    return [
        {'name': 'LogicFilter', 'type': 'tree'},
    ]


def _get_data(channel):
    if channel == 'LogicFilter':
        val = rdb.hgetall(key)
        dval = {}
        for field, value in val.items():
            # No per-update console output.
            dval[field] = value
        return { 'tree': dval }

    return None


def _process_command(doc):
    commands = {
        'set_signals': 'trigger-signals',
        'set_expression': 'trigger-expression',
        'set_width': 'trigger-width',
    }
    try:
        selected = [name for name in commands if doc.get(name)]
        if not selected:
            return None
        if len(selected) != 1:
            raise ValueError('Specify one setting operation at a time')
        field = commands[selected[0]]
        value = doc.get(field)
        if value is None:
            raise ValueError(f'Missing parameter: {field}')
        value = int(value) if field == 'trigger-width' else str(value)
        rdb.hset(key, field, value)
        return True
    except (KeyError, ValueError, TypeError, redis.RedisError) as exc:
        return {'status': 'error', 'message': str(exc)}


# for testing
if __name__ == '__main__':
    redis_url = 'redis://localhost:6379/'
    _initialize()
    print(_get_data(_get_channels()[0]['name']))
    #print(_get_data('LogicFilter'))
