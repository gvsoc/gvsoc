# `ficlib`

`ficlib` suite is designed to ease the FCM design. It consists of several Python classes:

- `campaign_manager.py`: contains the core class `CampaignManager`. It defines and contains the fault injection campaign. Having this as a class object makes it easier to pass the state of the campaign around and so the infrastructure is rather agile. In particular, the user is free to define its own `fault_generator` (or `stat_analyzer` or whatever) which can make choices depending on the previous runs.

- `*_helpers.py`: hold the corresponding helper functions. The ELF parser is contained within `poi_helpers`. The `fic_proxy_helpers` will probably disappear.

## `CampaignManager` constructor

Must supply at least the following to the constructor:

- `pois`: list of memory regions to be integrity checked with entries of type `poi_helpers::PoI`. Use auxiliary functions from `poi_helpers` to construct it. Beyond that, each entry should specify the path of FIC which should perform the integrity check of this PoI and the relative device ID from point of view of FIC containing this PoI. One can simply set the FIC that faces the global interconnect and the relative device ID to `-1` which denotes global address space. In this case, the integrity check is done via debug IO request using global interconnect. Note that automating the process of identifying the FIC and target ID is equivalent to dynamically identifying the memory map.

- `fics`: the list of paths of all FIC. Primarily used for getting all relevant information about the system such as number of memories and their sizes under each FIC. Note that each FIC and its underlying devices may be connected to different clocks.

- `target`, `binary`, `builddir`: clear.

Further, the `fault_generator` callable has to be supplied before the worker threads are started (but it can be supplied after running `do_golden_run` to account for dynamically extracted data).

## Typical usage flow

1. Instantiate `campaign = CampaignManager(...)` as above.
2. Do golden run: `campaign.do_golden_run()`.
3. Define fault generating function and point `campaign.fault_generator` to it.
4. Run the campaign: `campaign.start_workers()`.

At this point, `campaign` has all relevant information. One can display it e.g. by `campaign.print_results()`.

For an example implementation, see `faulted_pulp_open/fcm.py`.
