#!/usr/bin/env python3
import argparse
import json
import re

redmule_regex = r'^\d+: \d+: \[.*/chip/cluster_(?P<cluster_idx>\d+)/redmule/trace.*' \
                r'Finished : (?P<tstart>\d+) ns ---> (?P<tend>\d+) ns \| .* \| uti = (?P<util>\d+\.\d+) \|'
idma_regex = r'^\d+: \d+: \[.*/chip/cluster_(?P<cluster_idx>\d+)/idma/fe/trace.*' \
             r'Finished : (?P<tstart>\d+) ns ---> (?P<tend>\d+) ns'
hbm_bw_regex = r'^DRAMSysRecordable(?P<channel_idx>\d+)\.controller\d+\s*AVG BW:\s*.*\|\s*(?P<bandwidth>\d+\.\d+)\s*%$'
hbm_time_regex = r'^DRAMSysRecordable\d+\.controller\d+\s*Total Time:\s*(?P<time>\d+) (?P<unit>ns|us)$'
barrier_regex = r'^\d+: \d+: \[.*/chip/cluster_(?P<cluster_idx>\d+)/cluster_registers/trace.*' \
                r'Cluster Sync: (?P<tstart>\d+) ns -> (?P<tend>\d+) ns \| .* \| Type = (?P<type>\d)$'


def parse_trace(input, output):
    # Extract relevant phases from GVSoC log
    phases = []
    hbm = []
    # Parse file line by line
    with open(input) as f:
        for line in f:
            # Match against redmule regex
            m = re.match(redmule_regex, line)
            if m:
                phase = m.groupdict()
                phase['agent'] = 'redmule'
                phase['attrs'] = {'info': line}
                phases.append(phase)
                continue
            # Match against idma regex
            m = re.match(idma_regex, line)
            if m:
                phase = m.groupdict()
                phase['agent'] = 'idma'
                phase['attrs'] = {'info': line}
                phases.append(phase)
                continue
            # Match against HBM time regex
            m = re.match(hbm_time_regex, line)
            if m:
                hbm.append(m.groupdict())
                continue
            # Match against HBM BW regex
            m = re.match(hbm_bw_regex, line)
            if m:
                hbm[-1].update(m.groupdict())
                continue
            # Match against barrier regex
            m = re.match(barrier_regex, line)
            if m:
                phase = m.groupdict()
                phase['agent'] = 'cluster sync'
                phase['attrs'] = {'info': line}
                phases.append(phase)
                continue

    # Reformat phases following Snitch's roi.json format
    rois = {}
    for phase in phases:
        track = 'cluster_' + phase['cluster_idx']
        if track not in rois:
            rois[track] = []
        rois[track].append({
            'label': phase['agent'],
            'tstart': int(phase['tstart']),
            'tend': int(phase['tend']),
            'attrs': phase.get('attrs', {})
        })
    for channel in hbm:
        track = 'hbm_' + channel['channel_idx']
        time = int(channel['time'])
        time = time * 1000 if channel['unit'] == 'us' else time
        rois[track] = [{
            'label': 'all',
            'tstart': 0,
            'tend': time,
            'attrs': {'bandwidth': float(channel['bandwidth'])}
        }]

    # Write to JSON file
    with open(output, 'w') as f:
        json.dump(rois, f, indent=2)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('input', help='Path to GVSoC trace')
    parser.add_argument('output', help='Output path')
    args = parser.parse_args()

    parse_trace(args.input, args.output)


if __name__ == '__main__':
    main()
