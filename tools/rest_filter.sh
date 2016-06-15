#! /bin/sh
#
# Filters the output of the /metrics/json HTTP endpoint of opticsd.
# Depends on jq.
#
# Example :
#
#   curl -s localhost:3002/metrics/json | ./tools/rest_filter.sh blah
#
# Will returns all the metrics which contains 'blah' in their name.

jq "[to_entries | .[] | select(.key | contains(\"$1\"))] | from_entries"
