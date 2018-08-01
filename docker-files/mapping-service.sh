#!/usr/bin/env bash

export FCGI_WEB_SERVER_ADDRS=127.0.0.1

cd /app
exec /sbin/setuser www-data spawn-fcgi -n -p 10100 mapping_cgi >> /var/log/mapping.log 2>&1
