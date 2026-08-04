# In recursive mode we should not display empty listings

- PRIORITY: 100
- TAGS: bug, historical
- ACTIVITY: COMPOUNDING
- GATES: REVIEW RETRO
- RESOLUTION: DONE

When we use `-r` with the `ls` subcommand and we also apply a filter with `-f`
we might have sections that are empty. In that case we should not display the
section title anymore.
