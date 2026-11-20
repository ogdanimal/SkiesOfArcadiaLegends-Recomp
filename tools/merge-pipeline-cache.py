#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Builds release/initial_pipeline_cache.db, the pipeline list every package carries (issue #34): Aurora merges it
# into the player's own cache at the first start, and the host compiles everything listed before the game's first
# scene (video.cpp VideoPrepareShaders). Inputs are Aurora pipeline caches (pipeline_cache.db from a user's cache
# folder, or an earlier initial_pipeline_cache.db) from runs of the standalone on any platform: the rows are render
# configurations, not compiled code, so they're the same everywhere. A pipeline in several inputs keeps its
# earliest first_frame_used (the load order: the opening's pipelines first). Only the newest config_version of
# each shader type among the inputs is kept (older ones no longer load: rebuild the list from runs of the current
# build after Aurora changes a config version). Rows at any MSAA sample count are fine: Aurora moves a cached row
# to the count in use (recompcore-aurora-cache-sample-count). first_frame_used counts from each run's start, so
# runs from a new game order the list best.
#   tools/merge-pipeline-cache.py OUT.db IN.db [IN.db...]
import sqlite3
import sys

SCHEMA = 1
out_path, inputs = sys.argv[1], sys.argv[2:]
if not inputs:
    sys.exit('usage: merge-pipeline-cache.py OUT.db IN.db [IN.db...]')

rows = {}  # (type, hash) -> (config_version, config_size, config, first_frame_used)
for path in inputs:
    db = sqlite3.connect(f'file:{path}?mode=ro', uri=True)
    if db.execute('SELECT 1 FROM aurora_schema WHERE value = ?', (SCHEMA,)).fetchone() is None:
        sys.exit(f'{path}: not an Aurora pipeline cache of schema {SCHEMA}')
    n = 0
    for t, h, version, size, config, first in db.execute(
            'SELECT type, hash, config_version, config_size, config, first_frame_used FROM pipeline_cache'):
        if size != len(config):
            continue
        key = (t, h)
        old = rows.get(key)
        if old is None or version > old[0] or (version == old[0] and first < old[3]):
            rows[key] = (version, size, config, first)
        n += 1
    db.close()
    print(f'{path}: {n} rows')

newest = {}
for (t, _), (version, *_rest) in rows.items():
    newest[t] = max(newest.get(t, 0), version)
kept = {k: v for k, v in rows.items() if v[0] == newest[k[0]]}

out = sqlite3.connect(out_path)
out.executescript('''
DROP TABLE IF EXISTS aurora_schema;
DROP TABLE IF EXISTS pipeline_cache;
CREATE TABLE aurora_schema(value INTEGER);
CREATE TABLE pipeline_cache (
  type INTEGER NOT NULL,
  hash INTEGER NOT NULL,
  config_version INTEGER NOT NULL,
  config_size INTEGER NOT NULL,
  config BLOB NOT NULL,
  first_frame_used INTEGER NOT NULL,
  PRIMARY KEY (type, hash)
);
CREATE INDEX pipeline_cache_load_order_idx ON pipeline_cache(type, config_version, first_frame_used);
''')
out.execute('INSERT INTO aurora_schema VALUES (?)', (SCHEMA,))
out.executemany('INSERT INTO pipeline_cache VALUES (?, ?, ?, ?, ?, ?)',
                [(t, h, v, s, c, f) for (t, h), (v, s, c, f) in sorted(kept.items(), key=lambda kv: kv[1][3])])
out.commit()
out.execute('VACUUM')
out.close()
by_type = {}
for (t, _) in kept:
    by_type[t] = by_type.get(t, 0) + 1
print(f'{out_path}: {len(kept)} pipelines {dict(sorted(by_type.items()))}, dropped {len(rows) - len(kept)} older-version rows')
