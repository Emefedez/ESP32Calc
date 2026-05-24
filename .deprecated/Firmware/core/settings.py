"""
Persistent settings stored as JSON (MicroPython flash).
"""
import json


_SETTINGS = {}


def load(path='/settings.json'):
    global _SETTINGS
    try:
        with open(path) as f:
            _SETTINGS = json.load(f)
    except (OSError, ValueError):
        _SETTINGS = {}


def save(path='/settings.json'):
    with open(path, 'w') as f:
        json.dump(_SETTINGS, f)


def get(key, default=None):
    return _SETTINGS.get(key, default)


def set(key, value):
    _SETTINGS[key] = value


def delete(key):
    _SETTINGS.pop(key, None)
