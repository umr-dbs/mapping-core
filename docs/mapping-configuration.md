# Mapping Configuration
Mapping can be configured via config file and environment variables. The configuration is loaded on startup in this order:
1. /etc/mapping.conf
2. mapping.conf relative to executable
3. from environment variables

Configurations can be overriden by specifying them in later steps of the configuration loading process.

## (F)CGI Mode
The CGI mode is specified via an environment variable. If the variable `FCGI_WEB_SERVER_ADDRS` is set, Mapping will assume it is run in FCGI mode. Otherwise it assumes CGI mode.


## Configurations
| Key        | Values           | Default | Description  |
| ------------- |-------------| -----| ----- |
| fcgi.threads      | \<integer\> | |the number of threads to spawn in FCGI mode |
| userdb.backend      | sqlite      | |   The backend to use for the user db |
| userdb.sqlite.location | \<path-to-the-sqlite-file\> || The file path where the sqlite database is stored |
| cache.enabled | true \| false     | false |Enable/Disable cache |
| cache.type | local \| remote | |Cache either inside (F)CGI process or use remote cache |
| cache.replacement | lru | |The replacement strategy of the cache |
| cache.\<type\>.size | \<integer\> | |Size of \<type\> in bytes. \<type\> can be raster, points, lines, polygons, plots, provenance |
| cache.strategy | always \| never | |When to cache |
| global.debug | 0 \| 1 | |Global debug flag e.g. used in services |
| global.opencl.preferredplatform | \<string\> | |The preferred platform for OpenCL |
| global.opencl.forcecpu | 0 \| 1 | |Force OpenCL to use the CPU instead of GPU |
| rasterdb.backend | local \| remote | local | Remote specifies to use a tileserver to fetch raster tiles instead of loading them from disk |
| rasterdb.tileserver.port | \<integer\> | | Specify the port for starting the tileserver. |
| rasterdb.remote.host | \<string\> | | Specify the host of the tileserver to connect to. |
| rasterdb.remote.port | \<integer\> | | Specify the port of the tileserver to connect to. |
| rasterdb.local.location | \<string\> | | Specify the location for the *local* rasterdb to use for storing data. |
| featurecollectiondb.backend | postgres | | The backend for the featurecollectiondb |
| featurecollectiondb.postgres.location | \<string\> || The SQL connection string e.g. `user = 'user' host = 'localhost' password = 'pass' dbname = 'featurecollectiondb_test'`. Note that the corresponding database needs to have the `POSTGIS` extension installed |

### Distributed mode
Specific configurations for distributed mode where the CGI connects to index node and workers compute results.

| Key        | Values           | Default | Description  |
| ------------- |-------------| -----| ----- |
| log.level | off \| error \| warn \| info \| debug \| trace | info | The log level of the index and nodes in distributed mode. |
| nodeserver.port | \<integer\> | | The port for a worker node to use |
| nodeserver.threads | \<integer\> | | The number of threads for a worker to use |
| nodeserver.cache.manager | local \| remote | | The cache manager to use.
| nodeserver.cache.local.replacement | lru | |The replacement strategy of the cache |
| nodeserver.cache.\<type\>.size | \<integer\> | |Size of \<type\> in bytes. \<type\> can be raster, points, lines, polygons, plots, provenance |
| nodeserver.cache.strategy | always \| never | |When to cache |
| indexserver.port |\<integer\> || The port for the index server to open and for the workers to connect to |
| indexserver.host | \<string\> || The host of the index node for the workers to connect to. |
| indexserver.scheduler | default \| bema | default | The scheduler of the indexserver |
| indexserver.reorg.interval | \<integer\> | | The reorganization interval e.g. 500 |
| indexserver.reorg.strategy | capacity \| graph \| geo | |  capacity: redistribute using memory-usage as metric, graph: Cluster entries by similar operator-graphs, cluster entries by spatial locality |
| indexserver.reord.relevance | lru \| costlru | | lru: simple lru replacement, costlru: cost-based lru
 

