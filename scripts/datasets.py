from ssgetpy import search, fetch
import pprint

out = search(nzbounds = (1e6, 1e9), kind="Graph", limit=1e3)

for entry in out:
  print(f'{entry.name}/{entry.group} {entry.rows} {entry.nnz} {entry.kind}')
